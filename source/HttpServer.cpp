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

#include "HttpServer.h"


/**
 * HttpRequest Constructor
 *
 * @param[in] network The network interface
*/
HttpServer::HttpServer(NetworkInterface* network, int nWorkerThreads, int nWebSocketsMax)  :
    _threadHTTPServer(osPriorityNormal, 2*1024, nullptr, "HTTPServerThread") { 
    _network = network;
    _nWebSockets = 0;
    _nWebSocketsMax = nWebSocketsMax;
    _nWorkerThreads = nWorkerThreads;
}

HttpServer::~HttpServer() {
}

/**
 * Start running the server (it will run on it's own thread)
 */
nsapi_error_t HttpServer::start(uint16_t port) {
    // create client connections
    // needs RAM for buffers!
    _clientConnections.reserve(_nWorkerThreads);
    for(int i=0; i < _nWorkerThreads; i++) {
        string *threadName = new string;
        *threadName = "HTTPClientThread_" + to_string(i);
        ClientConnection *clientCon = new ClientConnection(this, threadName->c_str());
        MBED_ASSERT(clientCon);
        _clientConnections.push_back(clientCon);
    }

    // create server socket and start to listen
    _serverSocket = new TCPSocket();
    MBED_ASSERT(_serverSocket);
    nsapi_error_t ret;

    ret = _serverSocket->open(_network);
    if (ret != NSAPI_ERROR_OK) {
        return ret;
    }

    ret = _serverSocket->bind(port);
    if (ret != NSAPI_ERROR_OK) {
        return ret;
    }

    _serverSocket->listen(_nWorkerThreads); // max. concurrent connections...

    _threadHTTPServer.start(callback(this, &HttpServer::main));

    return NSAPI_ERROR_OK;
}


void HttpServer::main() {
    while (1) {
        nsapi_error_t accept_res = -1;
        TCPSocket* clt_sock = _serverSocket->accept(&accept_res);
        if (accept_res == NSAPI_ERROR_OK) {
            // find idle client connection
            vector<ClientConnection*>::iterator it = _clientConnections.begin();
            while (((*it)->isIdle() == false) && (it < _clientConnections.end())) {
                 it++;
            }
            
            if ((*it)->isIdle()) {
                (*it)->start(clt_sock);
            } else
            {
                clt_sock->close();               // no idle connections, close. Todo: wait with timeout
            }
            
        }
    }
}

void HttpServer::setWSHandler(const char* path, CreateWSHandlerFn handler)
{
	_WSHandlers[path] = handler;
}

CreateWSHandlerFn HttpServer::getWSHandler(const char* path)
{
	WebSocketHandlerContainer::iterator it;

	it = _WSHandlers.find(path);
	if (it != _WSHandlers.end()) {
		return it->second;
	}
	return nullptr;
}

void HttpServer::setHTTPHandler(const char* path, CallbackRequestHandler handler)
{
	_HTTPHandlers[path] = handler;
}


CallbackRequestHandler HttpServer::getHTTPHandler(const char* url)
{
	HTTPSocketHandlerContainer::iterator it;

    string path(url);
    size_t found = path.find_last_of("/");
    if(found) {
        path.erase(found+1, path.length());
    }

	it = _HTTPHandlers.find(path);
	if (it != _HTTPHandlers.end()) {
		return it->second;
	}
    // if no matching handler is found, return 1st (root handler)
    if (_HTTPHandlers.size() > 0) {
    	it = _HTTPHandlers.begin();
        return it->second;
    }
    else	
        return nullptr;
}

