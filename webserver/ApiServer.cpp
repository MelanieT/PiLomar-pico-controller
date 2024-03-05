//
// Created by Melanie on 11/11/2023.
//

#include "WebServer.h"
#include "ApiServer.h"
#include "HttpResponse.h"
#include "HttpRequest.h"
#include "UrlMapper.h"
#include <string>
#include <regex>

extern WebServer webserver;

ApiServer::ApiServer()
= default;

HttpResponse ApiServer::RequestHandler(const char *request)
{
    HttpRequest req(request);

    HttpResponse response;

    UrlMapper::Map(req, response);

    return response;
}
