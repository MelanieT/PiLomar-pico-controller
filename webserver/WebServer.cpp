#include "WebServer.h"

#define TCP_PORT 80

WebServer::WebServer()
= default;

int WebServer::Init(int RetryForever)
{
    return (0);
}

void WebServer::ProcessMessages(HttpResponse (*Cb)(const char *Request))
{
}
