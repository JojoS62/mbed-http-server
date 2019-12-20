/*
 * PackageLicenseDeclared: Apache-2.0
 * Copyright (c) 2017 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HTTP_SERVER_h__
#define __HTTP_SERVER_h__

#include "mbed.h"

#include "HttpRequestParser.h"
#include "HttpResponseBuilder.h"

#include "HttpParsedRequest.h"
#include "WebSocketHandler.h"
#include "HTTPHandler.h"
#include "ClientConnection.h"

#include <string>
#include <map>
#include <vector>

#ifndef HTTP_SERVER_MAX_CONCURRENT
#define HTTP_SERVER_MAX_CONCURRENT      5
#endif

#if HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_SOCKET_MAX
#warning "HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_SOCKET_MAX, HTTPServer needs more sockets for this setting, increase socket count in mbed_app.json"
#endif
#if HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_TCP_SOCKET_MAX
#warning "HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_TCP_SOCKET_MAX, HTTPServer needs more TCP sockets for this setting, increase socket count in mbed_app.json"
#endif

#ifdef DEBUG_WEBSOCKETS
#define DEBUG_WEBSOCKETS(...) printf(__VA_ARGS__)
#else
#define DEBUG_WEBSOCKETS(...)
#endif

//typedef HttpParsedRequest ParsedHttpRequest;

typedef WebSocketHandler* (*CreateWSHandlerFn)();
typedef std::map<std::string, CreateWSHandlerFn> WebSocketHandlerContainer;


/**
 * \brief HttpServer implements the logic for setting up an HTTP server.
 */
class HttpServer {
public:
    /**
     * HttpRequest Constructor
     *
     * @param[in] network The network interface
    */
    HttpServer(NetworkInterface* network, int nWorkerThreads, int nWebSocketsMax);

    ~HttpServer();

    /**
     * Start running the server (it will run on it's own thread)
     */
    nsapi_error_t start(uint16_t port);

    void setHTTPHandler(const char* path, CallbackRequestHandler handler);
    CallbackRequestHandler getHTTPHandler(const char* path);

    void setWSHandler(const char* path, CreateWSHandlerFn handler);
    CreateWSHandlerFn getWSHandler(const char* path);

    bool isWebsocketAvailable() { return (_nWebSockets < _nWebSocketsMax); };
    int getWebsocketCount() { return _nWebSockets; };
    bool incWebsocketCount() { 
        if (_nWebSockets < _nWebSocketsMax) {
            _nWebSockets++;
            return true;
        } else
        {
            return false;
        }
    };
    
    void decWebsocketCount() { _nWebSockets--; };
    

private:
    void main();
    TCPSocket* _serverSocket;
    NetworkInterface* _network;
    Thread _threadHTTPServer;
    int _nWorkerThreads;
    int _nWebSockets;
    int _nWebSocketsMax;
    CallbackRequestHandler _handler;
    vector<ClientConnection*> _clientConnections;

    WebSocketHandlerContainer _WSHandlers;
    HTTPSocketHandlerContainer _HTTPHandlers;
};

#endif // __HTTP_SERVER_h__
