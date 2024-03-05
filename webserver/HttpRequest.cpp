// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

#include "HttpRequest.h"

#include <utility>
#include "regex"

HttpRequest::HttpRequest(const char *request)
{
    std::string req(request);

    req = std::regex_replace(req, std::regex("\r"), ""); // Remove CR
    auto head = req.substr(0, req.find("\n\n")); // Find first double LF
    auto m = head.substr(0, head.find('\n'));
    std::string headers;
    if (head.length() >= m.length())
        headers = head.substr(m.length() + 1);

    m_method = m.substr(0, m.find(' '));

    auto firstline = m.substr(m_method.length() + 1);
    auto u = firstline.substr(0, firstline.find(' '));
    if (u.find('?') != -1)
    {
        m_url = u.substr(0, firstline.find('?'));
        std::string params = u.substr(m_url.length() + 1);
        std::istringstream p(params);
        std::string  param;

        while (std::getline(p, param, '&'))
        {
            if (param.find('=') != -1)
            {
                auto name= param.substr(0, param.find('='));
                auto value = param.substr(name.length() + 1);

                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c){ return std::tolower(c); });

                m_params.emplace_back(name, urlDecode(value));
            }
            else
            {
                m_params.emplace_back(param, "");
            }
        }

    }
    else
    {
        m_url = u;
    }


    m_body = req.length() > head.length() + 2 ? req.substr(head.length() + 2) : "";

    std::istringstream h(headers);
    std::string line;

    while (std::getline(h, line))
    {
        if (line.find(':') != -1)
        {
            auto name = line.substr(0, line.find(':'));
            auto value = line.substr(name.length() + 1);

            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            regex_replace(value, std::regex("^ *"), "");
            m_headers.emplace_back(name, value);
        }
    }

}

char HttpRequest::fromHex(char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

std::string HttpRequest::urlDecode(std::string text) {
    char h;
    std::ostringstream escaped;
    escaped.fill('0');

    for (auto i = text.begin(), n = text.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        if (c == '%') {
            if (i[1] && i[2]) {
                h = fromHex(i[1]) << 4 | fromHex(i[2]);
                escaped << h;
                i += 2;
            }
        } else if (c == '+') {
            escaped << ' ';
        } else {
            escaped << c;
        }
    }

    return escaped.str();
}

std::string HttpRequest::method() const
{
    return m_method;
}

std::string HttpRequest::url() const
{
    return m_url;
}

std::optional<std::string> HttpRequest::param(std::string name)
{
    return getValue(m_params, std::move(name));
}

std::optional<std::string> HttpRequest::header(std::string name)
{
    return getValue(m_headers, std::move(name));
}

//std::list<std::string> HttpRequest::params(std::string name)
//{
//    return std::list<std::string>();
//}
//
//std::list<std::string> HttpRequest::headers(std::string name)
//{
//    return std::list<std::string>();
//}

std::string HttpRequest::body() const
{
    return m_body;
}

std::optional<std::string> HttpRequest::getValue(const std::list <std::pair<std::string, std::string>>& collection, std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    for (const auto& item : collection)
    {
        if (item.first == name)
            return {item.second};
    }

    return {};
}

