// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

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
