#include "request_create.h"
#include <arpa/inet.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <error.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

// This function returns socket ID
int create_browser_socket(int listen_sock_fd) {
    struct sockaddr_storage other_addr;
    socklen_t socket_addr_len = sizeof(other_addr);
    return accept(listen_sock_fd, (struct sockaddr *)&other_addr, &socket_addr_len);
}


int main() {
    // create log
    mkdir("/var/log/erss", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    int log = open("/var/log/erss/proxy.log", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
    dup2(log, STDOUT_FILENO);
    

    // create socket for listen
    int status;
    int listen_sock_fd;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    const char *hostname = NULL;
    const char *port = "12345";

    memset(&host_info, 0, sizeof(host_info));

    host_info.ai_family = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
    host_info.ai_flags = AI_PASSIVE;

    status = getaddrinfo(hostname, port, &host_info, &host_info_list);
    if (status != 0) {
        std::cout << "ERROR: cannot get address info for host" << std::endl;
        return -1;
    }

    listen_sock_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (listen_sock_fd == -1) {
        std::cout << "ERROR: cannot create socket" << std::endl;
        return -1;
    }

    int yes = 1;
    status = setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    status = bind(listen_sock_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        std::cout << "ERROR: cannot bind socket" << std::endl;
        return -1;
    } 

    status = listen(listen_sock_fd, 500);
    if (status == -1) {
        std::cout << "ERROR: cannot listen on socket" << std::endl; 
        return -1;
    }

    std::cout << "NOTE: Waiting for connection on port " << port << std::endl;

    
    int browser_sock_fd;
    int req_id = 0;
    struct sockaddr_storage other_addr;
    socklen_t socket_addr_len = sizeof(other_addr);

    Cache cache;

    while (1) {
        browser_sock_fd = accept(listen_sock_fd, (struct sockaddr *)&other_addr, &socket_addr_len);
        if (browser_sock_fd == -1) {
            std::cout<< "ERROR: cannot accept connection on socket" << endl;
            return -1;
        }
        
        

        // receive request from browser_sock_fd, send to the origin server and get response
        thread(recv_request, req_id, browser_sock_fd, inet_ntoa(((struct sockaddr_in *)&other_addr)->sin_addr), cache).detach();
        // recv_request(req_id, browser_sock_fd, inet_ntoa(((struct sockaddr_in *)&other_addr)->sin_addr));
        req_id++;
    }
  

    
    close(listen_sock_fd);

    // close log
    close(log);

    return 0;
}