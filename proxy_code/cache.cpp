#include "cache.h"

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
#include <pthread.h>
#include <mutex>


// initialize the pthread_mutex_t lock
//pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
std::mutex cache_lock;

// class Cache
// constructor
Cache::Cache() {
}

Cache::Cache(std::vector<std::pair<std::string, std::vector<char> > > cache_vector, std::map<std::string, std::pair<time_t, std::string> > cache_map) :
    cache_vector(cache_vector), cache_map(cache_map) {
}

//copy constructor
Cache::Cache(const Cache & rhs) :
    cache_vector(rhs.cache_vector),
    cache_map(rhs.cache_map) {
}

//assignment constructor
Cache & Cache::operator=(const Cache & rhs) {
  if (this != &rhs) {
    cache_vector = rhs.cache_vector;
    cache_map = rhs.cache_map;
  }
  return *this;
}

// Destructor
Cache::~Cache() {
}

// other methods
// get cache_vector
std::vector<std::pair<std::string, std::vector<char> > > Cache::get_cache_vector() const{
  std::lock_guard<std::mutex> lock(cache_lock);
  //pthread_mutex_lock(&cache_lock);
  return cache_vector;
  //pthread_mutex_unlock(&cache_lock);
}

// get cache_map
std::map<std::string, std::pair<time_t, std::string> > Cache::get_cache_map() const{
  std::lock_guard<std::mutex> lock(cache_lock);
  //pthread_mutex_lock(&cache_lock);
  return cache_map;
  //pthread_mutex_unlock(&cache_lock);
}

// check in cache
bool Cache::check_in_cache(std::string uri){
  std::lock_guard<std::mutex> lock(cache_lock);
  //pthread_mutex_lock(&cache_lock);
  if (cache_map.find(uri) != cache_map.end()){
    return true;
  }
  else{
    return false;
  }
  //pthread_mutex_unlock(&cache_lock);
}

// add to cache
void Cache::add_cache(int id, std::string uri, httpparser::Response response, std::vector<char> response_buffer){
  std::lock_guard<std::mutex> lock(cache_lock);
  //pthread_mutex_lock(&cache_lock);
  std::string response_msg = response.inspect();
  //std::cout << "check: response_msg" << std::endl;
  time_t expire_time = cal_expire(response);
  //std::cout << "check: expire_time" << std::endl;

  // add the cache
  std::pair<std::string, std::vector<char> > curr_vector = std::make_pair(uri, response_buffer);
  cache_vector.push_back(curr_vector);
  std::pair<time_t, std::string> value = std::make_pair(expire_time, response_msg);
  cache_map.insert({uri, value});
  // print
  std::cout << id << ": " << "cached, expires at " << asctime(gmtime(&expire_time));

  // check whether the size of the cache exceeds 200
  if (cache_vector.size() > 200){
    std::string curr_uri = cache_vector.front().first;
    cache_map.erase(curr_uri);
    cache_vector.erase(cache_vector.begin());
  }

  // check whether need revalidate
  if (check_revalidation(response) == true){
    std::cout << id << ": " << "cached, but requires re-validation" << std::endl;
  }
  //pthread_mutex_unlock(&cache_lock);
}

// revalid cache
int Cache::revalid_cache(int id, std::string uri, httpparser::Response response){
  std::lock_guard<std::mutex> lock(cache_lock);
  //pthread_mutex_lock(&cache_lock);
  if (response.get_response_line().find("304 Not Modified") != std::string::npos){
    // response not modified
    // update the expire time
    time_t new_time = cal_expire(response);
    cache_map[uri].first = new_time;
    std::cout << id << ": " << "cached, expires at " << asctime(gmtime(&new_time));
    // check whether need revalidate
    if (check_revalidation(response) == true){
      std::cout << id << ": " << "cached, but requires re-validation" << std::endl;
    }
    return 1;
  }
  else if (response.get_response_line().find("200 OK") != std::string::npos){
    if (check_cacheable(response) == true){
      time_t new_time = cal_expire(response);
      std::string new_msg = response.inspect();
      cache_map[uri].first = new_time;
      cache_map[uri].second = new_msg;

      std::cout << id << ": " << "cached, expires at " << asctime(gmtime(&new_time));
      // check whether need revalidate
      if (check_revalidation(response) == true){
        std::cout << id << ": " << "cached, but requires re-validation" << std::endl;
      }
      return 1;
    }
    else{
        if (response.get_cache_control().find("no-store") != std::string::npos){
            std::cout << id << ": " << "not cacheable because " << "no-store" << std::endl;
        }
        else if (response.get_cache_control().find("private") != std::string::npos){
            std::cout << id << ": " << "not cacheable because " << "private" << std::endl;
        }
        // remove the cache
        for (std::vector<std::pair<std::string, std::vector<char> > >::iterator it = cache_vector.begin() ; it != cache_vector.end(); ++it){
          if (it->first == uri){
            cache_vector.erase(it);
          }
        }
        cache_map.erase(uri);
        return 0;
    }
  }
  else{
    return -1;
  }
  //pthread_mutex_unlock(&cache_lock);
}

// other methods
// check cacheability
bool check_cacheable(httpparser::Response response){
  // fetch the fields in response
  std::string cache_control = response.get_cache_control();
  if ((cache_control.find("no-store") != std::string::npos) || (cache_control.find("private") != std::string::npos)){
    //std::cout << "check: cacheable false" << std::endl;
    return false;
  }
  else{
    //std::cout << "check: cacheable true" << std::endl;
    return true;
  }
}

// check revalidation
bool check_revalidation(httpparser::Response response){
  // fetch the fields in response
  std::string cache_control = response.get_cache_control();
  if ((cache_control.find("no-cache") != std::string::npos) || (cache_control.find("must-revalidate") != std::string::npos)){
    return true;
  }
  else{
    return false;
  }
}

// calculate expiration time
time_t cal_expire(httpparser::Response response){
  // fetch the fields in response
  std::string cache_control = response.get_cache_control();
  size_t pos = cache_control.find("max-age");
  time_t result;

  if (pos != std::string::npos){
    // get the age
    size_t par = cache_control.find(',', pos);
    std::string age;
    if (par != std::string::npos){
      size_t length = par - pos - 8;
      age = cache_control.substr(pos + 8, length);
    }
    else{
      age = cache_control.substr(pos + 8);
    }

    int age_int = atoi(age.c_str());
    std::string date = response.get_date();
    struct tm date_utc;
    strptime(date.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &date_utc);
    date_utc.tm_sec += age_int;
    result = mktime(&date_utc);
  }
  else{
    std::string expire = response.get_expire();
    if (expire == ""){
      std::string date = response.get_date();
      struct tm date_utc;
      strptime(date.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &date_utc);
      result = mktime(&date_utc);
    }
    struct tm date_expire;
    strptime(expire.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &date_expire);
    result = mktime(&date_expire);
  }
  return result;
}
