/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */

#ifndef HTTPPARSER_REQUEST_H
#define HTTPPARSER_REQUEST_H

#include <string>
#include <vector>
#include <sstream>

namespace httpparser
{

struct Request {
    Request()
        : versionMajor(0), versionMinor(0), keepAlive(false)
    {}
    
    struct HeaderItem
    {
        std::string name;
        std::string value;
    };

    std::string method;
    std::string uri;
    int versionMajor;
    int versionMinor;
    std::vector<HeaderItem> headers;
    std::vector<char> content;
    bool keepAlive;

    // new attributes
    std::string get_header_line() const{
        std::stringstream strstream;
        strstream << method << " " << uri << " HTTP/" << versionMajor << "." << versionMinor;
        return strstream.str();
    }
    

    std::string inspect() const
    {
        std::stringstream stream;
        stream << method << " " << uri << " HTTP/"
               << versionMajor << "." << versionMinor << "\n";

        for(std::vector<Request::HeaderItem>::const_iterator it = headers.begin();
            it != headers.end(); ++it)
        {
            stream << it->name << ": " << it->value << "\n";
        }

        std::string data(content.begin(), content.end());
        stream << data << "\n";
        stream << "+ keep-alive: " << keepAlive << "\n";;
        return stream.str();
    }
};

} // namespace httpparser


#endif // HTTPPARSER_REQUEST_H