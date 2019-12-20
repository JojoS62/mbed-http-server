#ifndef __HTTP_Handler__
#define __HTTP_Handler__

#include "ClientConnection.h"

typedef Callback<void(HttpParsedRequest*, ClientConnection* )> CallbackRequestHandler;
typedef std::map<std::string, CallbackRequestHandler> HTTPSocketHandlerContainer;

#endif