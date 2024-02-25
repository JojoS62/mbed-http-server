/* 
 * Copyright (c) 2019 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ClientConnection.h"
#include "HttpServer.h"
#include "sha1.h"
#include "base64.h"

#define MAGIC_NUMBER		"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_ORIGIN           "Origin:"
#define OP_CONT		0x0
#define OP_TEXT		0x1
#define OP_BINARY	0x2
#define OP_CLOSE	0x8
#define OP_PING		0x9
#define OP_PONG		0xA

// max size of the WS Message Header
#define WEBSOCKETS_MAX_HEADER_SIZE (14)

typedef enum {
    WSC_NOT_CONNECTED,
    WSC_HEADER,
    WSC_CONNECTED
} WSclientsStatus_t;

typedef enum {
    WStype_ERROR,
    WStype_DISCONNECTED,
    WStype_CONNECTED,
    WStype_TEXT,
    WStype_BIN,
    WStype_FRAGMENT_TEXT_START,
    WStype_FRAGMENT_BIN_START,
    WStype_FRAGMENT,
    WStype_FRAGMENT_FIN,
    WStype_PING,
    WStype_PONG,
} WStype_t;

typedef struct {
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;

    WSopcode_t opCode;
    bool mask;

    size_t payloadLen;

    uint8_t * maskKey;
} WSMessageHeader_t;



ClientConnection::ClientConnection(HttpServer* server, const char* name) :
    _threadClientConnection(osPriorityNormal, 3*1024, nullptr, name),
    _parser(&_request) 
{ 
    _threadName = name;
    _isWebSocket = false;
    _server = server;
    _cIsClient = false;          // for websocket send, meaning ???
    _socketIsOpen = false;
    _semWaitForSocket.try_acquire();
    _threadClientConnection.start(callback(this, &ClientConnection::receiveData));
};

ClientConnection::~ClientConnection() {
    _handler = nullptr;
};

void ClientConnection::start(TCPSocket* socket) {
    _socketIsOpen = true;
    _socket = socket;
    // _socket->set_timeout(10000);
    _socket->set_blocking(true);
    _webSocketHandler = nullptr; 
    _parser.clear();
    _request.clear();
    _semWaitForSocket.release();
}

void ClientConnection::textWs(const char *url, const char *text, int length)
{
    if (!_isWebSocket) 
        return;

    if (_wsOrigin.compare(url) == 0) {
        sendFrame(WSop_text, (const uint8_t*)text, length);
    }
}

void ClientConnection::receiveData()
{
    while (1) {
        bool wsCloseRequest = false;
        _semWaitForSocket.acquire();

        debug("%s: run receiveData\n", _threadName);
        while(_socketIsOpen) {
            nsapi_size_or_error_t recv_ret;
            // if (!_isWebSocket) {
            //     _socket->set_timeout(10000);     // timeout to defend connections without sending data
            // }
            while ((recv_ret = _socket->recv(_recv_buffer, HTTP_RECEIVE_BUFFER_SIZE)) > 0) {
                if (_isWebSocket) {
                    break;  // Websocket must not be parsed
                }

                // Pass the chunk into the http_parser
                int nparsed = _parser.execute((const char*)_recv_buffer, recv_ret);
                if (nparsed != recv_ret) {
                    debug("%s: Parsing failed... parsed %d bytes, received %d bytes\n", _threadName, nparsed, recv_ret);
                    recv_ret = -2101;
                    break;
                }

                if (_request.is_message_complete()) {
                    break;
                }
            }

            // cyclic callback if websocket
            // if (_isWebSocket) {
            //     if (recv_ret == NSAPI_ERROR_WOULD_BLOCK) {
            //         if (_webSocketHandler)
            //             _webSocketHandler->onTimer();
            //     }
            // }
            
            // ws upgrade or simple http handling
            if (recv_ret > 0) {
                if (_isWebSocket) { 
                    _timerWSTimeout.reset();                                // received some data, reset watchdog
                    wsCloseRequest = (handleWebSocket(recv_ret) == false);  // I'm already a Websocket
                } else {
                    if (_request.get_Upgrade()) {                 
                        handleUpgradeRequest();                             // handle upgrade request 
                        // _socket->set_timeout(_wsTimerCycle.count());
                        _timerWSTimeout.reset();
                        _timerWSTimeout.start();
                    } else {                                                
                        _parser.finish();                                   // no websocket, normal http handling
                        _handler = _server->getHTTPHandler(_request.get_url().c_str());
                        if (_handler)
                            _handler(&_request, this);
                    } 
                } 
            }

            // check for connection close
            if(_isWebSocket) {
                if (wsCloseRequest || (recv_ret == 0) || (_timerWSTimeout.elapsed_time() > 20s) ) {
                    _webSocketHandler->onClose();
                    sendFrame(WSop_close);
                    _server->decWebsocketCount();       // websocket was closed, decrement websocket count
                    if (_webSocketHandler)
                        delete _webSocketHandler;
                    _isWebSocket = false;
                    _socket->close();                       // close socket. Because allocated by accept(), it will be deleted by itself
                    _socketIsOpen = false;
                    _timerWSTimeout.stop();
                }
            } else
            {
                debug("%s: socket closed, recv_ret %d\n", _threadName, recv_ret);
                _socket->close();                       // close socket. Because allocated by accept(), it will be deleted by itself
                _socketIsOpen = false;
            }
        }
    }
}

void ClientConnection::printRequestHeader()
{
    debug("[Http]Request came in: %s %s\n", http_method_str(_request.get_method()), _request.get_url().c_str());
    
    MapHeaderIterator it;
    int i = 0;

    for (it = _request.headers.begin(); it != _request.headers.end(); it++) {
        debug("[%d] ", i);
        debug(it->first.c_str());
        debug(" : ");
        debug(it->second.c_str());
        debug("\n");
        i++;
    }
    fflush(stdout);
}

void ClientConnection::handleUpgradeRequest() {
    //HttpResponseBuilder builder(101);
    
    MapHeaderIterator it;

    bool upgradeWebsocketfound = false;
    it = _request.headers.find("Upgrade");
    if (it != _request.headers.end() && (it->second == "websocket")) {
        upgradeWebsocketfound = true;
    }

    string secWebsocketKey;
    it = _request.headers.find("Sec-WebSocket-Key");
    if (it != _request.headers.end()) {
        secWebsocketKey = it->second;
    }

    CreateWSHandlerFn createFn = _server->getWSHandler(_request.get_url().c_str());

    if (upgradeWebsocketfound && !secWebsocketKey.empty() && createFn) {        // neccessary header keys found and handler available
        if (_server->isWebsocketAvailable()) {                                  // Websockets available?
            _isWebSocket = sendUpgradeResponse(secWebsocketKey.c_str());        // do upgrade handshake

            if (_isWebSocket && createFn) {                                     // if upgrade successful
                _server->incWebsocketCount();
                _webSocketHandler = createFn();                                 // create handler instance
                _webSocketHandler->setOrigin(_request.get_url().c_str());
                _wsOrigin = _request.get_url().c_str();
                _webSocketHandler->onOpen(this);                                // handler callback for onOpen()
            } 
        }
    }
}

nsapi_size_or_error_t ClientConnection::send(const char* buffer, size_t len)
{
    size_t bytesSent = 0;
    while(bytesSent < len) {
        nsapi_size_or_error_t sent = _socket->send(buffer + bytesSent,  len - bytesSent);
        if (sent < 0) {
            if (sent != NSAPI_ERROR_WOULD_BLOCK)
                return sent;
            else 
                continue;
        }
        bytesSent += sent;
    }
    return bytesSent;
}

bool ClientConnection::handleWebSocket(int size)
{
	uint8_t* ptr = _recv_buffer;

	bool fin = (*ptr & 0x80) == 0x80;
	uint8_t opcode = *ptr & 0xF;

	if (opcode == OP_PING) {
		*ptr = ((*ptr & 0xF0) | OP_PONG);
		_socket->send(_recv_buffer, size);
		return true;
	}
	if (opcode == OP_CLOSE) {
        //printf("received OP_CLOSE\n");
		return false;
	}
	ptr++;

	if (!fin || !_mPrevFin) {	
		debug("WARN: Data consists of multiple frame not supported\r\n");
		_mPrevFin = fin;
		return true; // not an error, just discard it
	}
	_mPrevFin = fin;

	bool mask = (*ptr & 0x80) == 0x80;
	uint8_t len = *ptr & 0x7F;
	ptr++;
	
	if (len > 125) {
		debug("WARN: Extended payload length not supported\r\n");
		return true; // not an error, just discard it
	}

	char* data;
	if (mask) {
		char* maskingKey = (char*)ptr;
		data = (char*)(ptr + 4);
		for (int i = 0; i < len; i++) {
        	data[i] = data[i] ^ maskingKey[(i % 4)];
        }
	} else {
		data = (char*)ptr;
	}
	if (_webSocketHandler) {
		if (opcode == OP_TEXT) {
			data[len] = '\0';
			_webSocketHandler->onMessage(data);
		} else if (opcode == OP_BINARY) {
			_webSocketHandler->onMessage(data, len);
		}
	}
	return true;
}

bool ClientConnection::sendUpgradeResponse(const char* key)
{
	char buf[128];

	if (strlen(key) + sizeof(MAGIC_NUMBER) > sizeof(buf)) {
		return false;
	}
	strcpy(buf, key);
	strcat(buf, MAGIC_NUMBER);

    uint8_t hash[20];
    mbedtls_sha1((const unsigned char*)buf, strlen(buf), hash);

	char encoded[30];
    size_t olen;
    mbedtls_base64_encode((unsigned char*)encoded, sizeof(encoded), &olen, hash, 20);

    char resp[] = "HTTP/1.1 101 Switching Protocols\r\n" \
	    "Upgrade: websocket\r\n" \
    	"Connection: Upgrade\r\n" \
    	"Sec-WebSocket-Accept: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n\r\n";
    char* ptr = strstr(resp, "XXXXX");
    strcpy(ptr, encoded);
    strcpy(ptr+strlen(encoded), "\r\n\r\n");

    int ret = _socket->send(resp, strlen(resp));
    if (ret < 0) {
    	debug("ERROR: Failed to send response\r\n");
    	return false;
    }

    return true;
}


/*
 * @param buf uint8_t *         ptr to the buffer for writing
 * @param opcode WSopcode_t
 * @param length size_t         length of the payload
 * @param mask bool             add dummy mask to the frame (needed for web browser)
 * @param maskkey uint8_t[4]    key used for payload
 * @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
 */
uint8_t ClientConnection::createHeader(uint8_t * headerPtr, WSopcode_t opcode, size_t length, bool mask, uint8_t maskKey[4], bool fin) {
    uint8_t headerSize;
    // calculate header Size
    if(length < 126) {
        headerSize = 2;
    } else if(length < 0xFFFF) {
        headerSize = 4;
    } else {
        headerSize = 10;
    }

    if(mask) {
        headerSize += 4;
    }

    // create header

    // byte 0
    *headerPtr = 0x00;
    if(fin) {
        *headerPtr |= (1 << 7);    ///< set Fin
    }
    *headerPtr |= opcode;    ///< set opcode
    headerPtr++;

    // byte 1
    *headerPtr = 0x00;
    if(mask) {
        *headerPtr |= (1 << 7);    ///< set mask
    }

    if(length < 126) {
        *headerPtr |= length;
        headerPtr++;
    } else if(length < 0xFFFF) {
        *headerPtr |= 126;
        headerPtr++;
        *headerPtr = ((length >> 8) & 0xFF);
        headerPtr++;
        *headerPtr = (length & 0xFF);
        headerPtr++;
    } else {
        // Normally we never get here (to less memory)
        *headerPtr |= 127;
        headerPtr++;
        *headerPtr = 0x00;
        headerPtr++;
        *headerPtr = 0x00;
        headerPtr++;
        *headerPtr = 0x00;
        headerPtr++;
        *headerPtr = 0x00;
        headerPtr++;
        *headerPtr = ((length >> 24) & 0xFF);
        headerPtr++;
        *headerPtr = ((length >> 16) & 0xFF);
        headerPtr++;
        *headerPtr = ((length >> 8) & 0xFF);
        headerPtr++;
        *headerPtr = (length & 0xFF);
        headerPtr++;
    }

    if(mask) {
        *headerPtr = maskKey[0];
        headerPtr++;
        *headerPtr = maskKey[1];
        headerPtr++;
        *headerPtr = maskKey[2];
        headerPtr++;
        *headerPtr = maskKey[3];
        headerPtr++;
    }
    return headerSize;
}

/**
 *
 * @param client WSclient_t *   ptr to the client struct
 * @param opcode WSopcode_t
 * @param length size_t         length of the payload
 * @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
 * @return true if ok
 */
bool ClientConnection::sendFrameHeader(WSopcode_t opcode, int length, bool fin) {
    uint8_t maskKey[4]                         = { 0x00, 0x00, 0x00, 0x00 };
    uint8_t buffer[WEBSOCKETS_MAX_HEADER_SIZE] = { 0 };

    int headerSize = createHeader(buffer, opcode, length, _cIsClient, maskKey, fin);

    if(send((const char*)buffer, headerSize) != headerSize) {
        return false;
    }

    return true;
}

/**
 *
 * @param client WSclient_t *   ptr to the client struct
 * @param opcode WSopcode_t
 * @param payload uint8_t *     ptr to the payload
 * @param length size_t         length of the payload
 * @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
 * @param headerToPayload bool  set true if the payload has reserved 14 Byte at the beginning to dynamically add the Header (payload neet to be in RAM!)
 * @return true if ok
 */
bool ClientConnection::sendFrame( WSopcode_t opcode, const uint8_t * payload, int length, bool fin) {
    if (!_socketIsOpen) {  // Todo: isConnected()    (client->tcp && !client->tcp->connected()) {
        DEBUG_WEBSOCKETS("[WS][sendFrame] not Connected!?\n");
        return false;
    }

    if (!_isWebSocket) {  // Todo: isWCSconnected  (client->status != WSC_CONNECTED) {
        DEBUG_WEBSOCKETS("[WS][sendFrame] not in WSC_CONNECTED state!?\n");
        return false;
    }

    DEBUG_WEBSOCKETS("[WS][sendFrame] ------- send message frame -------\n");
    DEBUG_WEBSOCKETS("[WS][sendFrame] fin: %u opCode: %u mask: %u length: %u headerToPayload: %u\n", fin, opcode, _cIsClient, length);

    if(opcode == WSop_text) {
        DEBUG_WEBSOCKETS("[WS][sendFrame] text: %s\n", payload);
    }

    uint8_t maskKey[4]                         = { 0x00, 0x00, 0x00, 0x00 };
    uint8_t header[WEBSOCKETS_MAX_HEADER_SIZE] = { 0 };

    int headerSize;
    bool ret = true;

    // calculate header Size
    if(length < 126) {
        headerSize = 2;
    } else if(length < 0xFFFF) {
        headerSize = 4;
    } else {
        headerSize = 10;
    }

    if(_cIsClient) {
        headerSize += 4;
    }

    createHeader(header, opcode, length, _cIsClient, maskKey, fin);

    if(send((const char*)header, headerSize) != headerSize) {       // send header
        ret = false;
    }

    if(payload && length > 0) {
        if(send((const char*)payload, length) != length) {       // send payload
            ret = false;
        }
    }

    return ret;
}
