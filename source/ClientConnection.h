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

#ifndef __ClientConnection_h__
#define __ClientConnection_h__

#include "mbed.h"
#include "HttpRequestParser.h"
#include "HttpParsedRequest.h"
#include "WebSocketHandler.h"
#include "HTTPHandler.h"
#include <string>
#include <map>

using namespace std::chrono;

// Websocket defines
typedef enum {
    WSop_continuation = 0x00,    ///< %x0 denotes a continuation frame
    WSop_text         = 0x01,    ///< %x1 denotes a text frame
    WSop_binary       = 0x02,    ///< %x2 denotes a binary frame
                                 ///< %x3-7 are reserved for further non-control frames
    WSop_close = 0x08,           ///< %x8 denotes a connection close
    WSop_ping  = 0x09,           ///< %x9 denotes a ping
    WSop_pong  = 0x0A            ///< %xA denotes a pong
                                 ///< %xB-F are reserved for further control frames
} WSopcode_t;


//typedef HttpResponse ParsedHttpRequest;
class HttpServer;

class ClientConnection {
public:
    ClientConnection(HttpServer* server, const char* name);
    ~ClientConnection();

    void start(TCPSocket* socket);
    bool isIdle() {return !_socketIsOpen; };

    // HTTP send
    nsapi_size_or_error_t send(const char* buffer, size_t len);

    // Websocket functions
    bool sendFrame(WSopcode_t opcode, const uint8_t * payload = NULL, int length = 0, bool fin = true);

    HttpServer* getServer() { return _server; };
    void setWSTimer(milliseconds cycleTime) {_wsTimerCycle = cycleTime;};
    const char* getThreadname() { return _threadName; };
    bool isWebSocket() { return _isWebSocket; };
    void textWs(const char* url, const char* text, int length);

private:
    void receiveData();
    bool handleWebSocket(int size);
    void handleUpgradeRequest();
    bool sendUpgradeResponse(const char* key);
    void printRequestHeader();
    uint8_t createHeader(uint8_t * buf, WSopcode_t opcode, size_t length, uint8_t maskKey[4], bool fin);
    bool sendFrameHeader(WSopcode_t opcode, int length = 0, bool fin = true);

    const char* _threadName;
    bool _socketIsOpen;
    HttpServer* _server;
    TCPSocket* _socket;
    Thread  _threadClientConnection;
    HttpParsedRequest  _request;
    HttpRequestParser _parser;
    bool _isWebSocket;
    bool _mPrevFin;
    uint8_t _recv_buffer[HTTP_RECEIVE_BUFFER_SIZE];
    CallbackRequestHandler _handler;
    WebSocketHandler* _webSocketHandler;
    Timer _timerWSTimeout;
    milliseconds _wsTimerCycle;
    std::string _wsOrigin;
};



#endif