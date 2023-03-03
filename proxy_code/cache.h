#ifndef __CACHE__H__
#define __CACHE__H__

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <ctime>

#include "httpparser/request.h"
#include "httpparser/httprequestparser.h"
#include "httpparser/response.h"
#include "httpparser/httpresponseparser.h"

class Cache{
private:
    size_t size = 200;
    // vector for FIFO data structure of the cache
    std::vector<std::pair<std::string, std::vector<char> > > cache_vector;
    // map to store and find cache
    // key: url
    // first: expired-time
    // second: response message
    std::map<std::string, std::pair<time_t, std::string> > cache_map;

public:
    // constructor
    Cache();
    Cache(std::vector<std::pair<std::string, std::vector<char> > > cache_vector, std::map<std::string, std::pair<time_t, std::string> > cache_map);
    //copy constructor
    Cache(const Cache & rhs);
    //assignment constructor
    Cache & operator=(const Cache & rhs);
    // Destructor
    ~Cache();

    // other methods
    // get cache_vector
    std::vector<std::pair<std::string, std::vector<char> > > get_cache_vector() const;

    // get cache_map
    std::map<std::string, std::pair<time_t, std::string> > get_cache_map() const;

    // check in cache
    bool check_in_cache(std::string uri);

    // add to cache
    void add_cache(int id, std::string uri, httpparser::Response response, std::vector<char> response_buffer);

    // revalid cache
    int revalid_cache(int id, std::string uri, httpparser::Response response);

};

// other methods
// check cacheability
bool check_cacheable(httpparser::Response response);

// check revalidation
bool check_revalidation(httpparser::Response response);

// calculate expiration time
time_t cal_expire(httpparser::Response response);

#endif