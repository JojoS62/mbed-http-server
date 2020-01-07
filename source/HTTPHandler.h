#ifndef __HTTP_Handler__
#define __HTTP_Handler__

#include <map>

class HttpParsedRequest;
class ClientConnection;

typedef Callback<void(HttpParsedRequest*, ClientConnection* )> CallbackRequestHandler;
typedef std::map<std::string, CallbackRequestHandler> HTTPSocketHandlerContainer;

#endif