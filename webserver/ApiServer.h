//
// Created by Melanie on 11/11/2023.
//

#ifndef PICOW_WLAN_SETUP_WEBINTERFACE_APISERVER_H
#define PICOW_WLAN_SETUP_WEBINTERFACE_APISERVER_H


class ApiServer
{
public:
    ApiServer();

    static HttpResponse RequestHandler(const char *request);
};


#endif //PICOW_WLAN_SETUP_WEBINTERFACE_APISERVER_H
