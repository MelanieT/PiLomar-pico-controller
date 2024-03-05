// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

#include <string>
#include <list>
#include "HttpRequest.h"
#include "HttpResponse.h"

class Url
{
public:
    Url(std::string method, std::string url, void (*handler)(const HttpRequest& request, HttpResponse& response));
    std::string method();
    std::string url();
    void call(const HttpRequest &request, HttpResponse& response);

private:
    std::string m_method;
    std::string m_url;
    void (*m_handler)(const HttpRequest& request, HttpResponse& response);
};

#ifndef PICOW_WLAN_SETUP_WEBINTERFACE_URLMAPPER_H
#define PICOW_WLAN_SETUP_WEBINTERFACE_URLMAPPER_H


class UrlMapper
{
public:
    UrlMapper();
    static void AddMapping(std::string method, std::string url, void (*handler)(const HttpRequest& request, HttpResponse& response));
    static void Map(const HttpRequest& request, HttpResponse& response);
private:
    static std::list<Url> m_mapping;
};


#endif //PICOW_WLAN_SETUP_WEBINTERFACE_URLMAPPER_H
