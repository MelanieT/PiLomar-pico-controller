// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "HttpResponse.h"

class WebServer
{
public:
    WebServer();

    virtual int Init(int RetryForever);
    virtual void ProcessMessages(HttpResponse (*Cb)(const char *Request));

private:
};

#endif
