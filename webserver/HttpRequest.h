// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PICOW_WLAN_SETUP_WEBINTERFACE_HTTPREQUEST_H
#define PICOW_WLAN_SETUP_WEBINTERFACE_HTTPREQUEST_H


#include <list>
#include <string>
#include <optional>

class HttpRequest
{
public:
    explicit HttpRequest(const char *request);
    std::string method() const;
    std::string url() const;
    std::optional<std::string> param(std::string name);
//    std::list<std::string> params(std::string name);
    std::optional<std::string> header(std::string name);
//    std::list<std::string> headers(std::string name);
    std::string body() const;

private:
    static char fromHex(char ch);
    static std::string urlDecode(std::string text);
    std::string m_method;
    std::string m_url;
    std::list<std::pair<std::string, std::string>> m_params;
    std::list<std::pair<std::string, std::string>> m_headers;
    std::string m_body;
    static std::optional<std::string> getValue(const std::list<std::pair<std::string, std::string>>& collection, std::string name);

};


#endif //PICOW_WLAN_SETUP_WEBINTERFACE_HTTPREQUEST_H
