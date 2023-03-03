/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */

#ifndef HTTPPARSER_RESPONSE_H
#define HTTPPARSER_RESPONSE_H

#include <string>
#include <vector>
#include <sstream>

namespace httpparser
{

struct Response {
    Response()
        : versionMajor(0), versionMinor(0), keepAlive(false), statusCode(0)
    {}
    
    struct HeaderItem
    {
        std::string name;
        std::string value;
    };

    int versionMajor;
    int versionMinor;
    std::vector<HeaderItem> headers;
    std::vector<char> content;
    bool keepAlive;
    
    unsigned int statusCode;
    std::string status;

    // new attributes
    // get response line
    std::string get_response_line() const{
        std::stringstream strstream;
        strstream << "HTTP/" << versionMajor << "." << versionMinor
               << " " << statusCode << " " << status;
        return strstream.str();
    }

    // get date
    std::string get_date() const{
        std::string response_date = "";
        for(std::vector<Response::HeaderItem>::const_iterator it = headers.begin();
            it != headers.end(); ++it)
        {
            if (it->name == "Date"){
                response_date = it->value;
                break;
            }
        }
        return response_date;
    }

    // get expire
    std::string get_expire() const{
        std::string expire = "";
        for(std::vector<Response::HeaderItem>::const_iterator it = headers.begin();
            it != headers.end(); ++it)
        {
            if (it->name == "Expires"){
                expire = it->value;
                break;
            }
        }
        return expire;
    }

    // get cache-control
    // get date
    std::string get_cache_control() const{
        std::string response_cache_control = "";
        for(std::vector<Response::HeaderItem>::const_iterator it = headers.begin();
            it != headers.end(); ++it)
        {
            if (it->name == "Cache-Control"){
                response_cache_control = it->value;
                break;
            }
        }
        return response_cache_control;
    }

    std::string inspect() const
    {
        std::stringstream stream;
        stream << "HTTP/" << versionMajor << "." << versionMinor
               << " " << statusCode << " " << status << "\n";

        for(std::vector<Response::HeaderItem>::const_iterator it = headers.begin();
            it != headers.end(); ++it)
        {
            stream << it->name << ": " << it->value << "\n";
        }

        std::string data(content.begin(), content.end());
        stream << data << "\n";
        return stream.str();
    }
};

} // namespace httpparser

#endif // HTTPPARSER_RESPONSE_H