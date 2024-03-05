// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PICOW_WLAN_SETUP_WEBINTERFACE_HTTPRESPONSE_H
#define PICOW_WLAN_SETUP_WEBINTERFACE_HTTPRESPONSE_H

#include <string>
#include <list>
#include "HttpStatus.h"

class HttpResponse
{
public:
    HttpResponse();

    void AddHeader(const std::string& header, const std::string& value);
    void setStatusCode(HttpStatus::Code);
    void setBody(std::string body);
    void appendBody(const std::string& body);
    std::string ToString(int statusCode = 0);

private:
    std::list<std::pair<std::string, std::string>> m_headers;
    int m_statusCode = 200;
    std::string m_body;
};


#endif //PICOW_WLAN_SETUP_WEBINTERFACE_HTTPRESPONSE_H
