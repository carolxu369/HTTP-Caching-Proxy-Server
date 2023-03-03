#ifndef __REQUEST_CREATE__H__
#define __REQUEST_CREATE__H__

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>

#include "httpparser/request.h"
#include "httpparser/httprequestparser.h"
#include "httpparser/response.h"
#include "httpparser/httpresponseparser.h"
#include "cache.h"

#define BUFFER_SIZE 65536
#define MAX_RES_HEADER_LEN 8192

using namespace std;

int higher_fd(int fd1, int fd2);
void recv_request(int req_id, int browser_sock_fd, std::string ip_addr, Cache cache);
vector<char> recv_response(int server_sock_fd, int browser_sock_fd);

#endif