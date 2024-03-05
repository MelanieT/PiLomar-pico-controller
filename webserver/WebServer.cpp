// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

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
