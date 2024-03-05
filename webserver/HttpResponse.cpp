//
// Created by Melanie on 11/11/2023.
//

#include <sstream>
#include <utility>
#include "HttpResponse.h"
#include "HttpStatus.h"

HttpResponse::HttpResponse()
{
    m_headers.emplace_back(std::pair("Content-Type", "application/json"));
    m_headers.emplace_back(std::pair("Connection", "close"));
}


std::string HttpResponse::ToString(int statusCode)
{
    if (statusCode)
        m_statusCode = statusCode;

    std::ostringstream response;

    response << "HTTP/1.1 " << m_statusCode << " " << HttpStatus::reasonPhrase(m_statusCode) << "\r\n";
    if (!m_body.empty())
        AddHeader("Content-Length", std::to_string(m_body.length()));

    for (const auto& h : m_headers)
        response << h.first << ": " << h.second << "\r\n";

    response << "\r\n"; // End of headers
    response << m_body;
    response.flush();

    return response.str();
}

void HttpResponse::AddHeader(const std::string& header, const std::string& value)
{
    if (header != "Accept") // Allow only Accept to stack
    {
        m_headers.remove_if([=] (auto p) { return p.first == header; });
    }
    m_headers.emplace_back(header, value);
}

void HttpResponse::setStatusCode(HttpStatus::Code code)
{
    m_statusCode = toInt(code);
}

void HttpResponse::setBody(std::string body)
{
    m_body = std::move(body);
}

void HttpResponse::appendBody(const std::string& body)
{
    m_body += body;
}

