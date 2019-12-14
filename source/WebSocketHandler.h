#ifndef __WEB_SOCKET_HANDLER_H__
#define __WEB_SOCKET_HANDLER_H__

class ClientConnection;

class WebSocketHandler
{
public:
    virtual ~WebSocketHandler() {};

    virtual void onOpen(ClientConnection *clientConnection) { _clientConnection = clientConnection; };
    virtual void onClose() {};
    // to receive text message
    virtual void onMessage(const char* text) {};
    // to receive binary message
    virtual void onMessage(const char* data, size_t size) {};
    virtual void onTimer() {};
    virtual void onError() {};
    virtual void setOrigin(char* origin) { /* use strcpy to copy originstring if needed */ };

protected:
    ClientConnection *_clientConnection;
};


#endif
