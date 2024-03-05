// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PICOW_WLAN_SETUP_WEBINTERFACE_APISERVER_H
#define PICOW_WLAN_SETUP_WEBINTERFACE_APISERVER_H


class ApiServer
{
public:
    ApiServer();

    static HttpResponse RequestHandler(const char *request);
};


#endif //PICOW_WLAN_SETUP_WEBINTERFACE_APISERVER_H
