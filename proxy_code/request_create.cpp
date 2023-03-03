#include "request_create.h"

using namespace std;

int higher_fd(int fd1, int fd2) {
    int res;
    if (fd1 > fd2) {
        res = fd1 + 1;
    }
    else {
        res = fd2 + 1;
    }
    return res;
}

void recv_request(int req_id, int browser_sock_fd, std::string ip_addr, Cache cache) {
    // get the buffer from the socket
    std::string buffer_str = "";
    const char * buffer_res;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int req_len = 0, tp = 0;

    tp = recv(browser_sock_fd, &buffer, BUFFER_SIZE, 0);
    while (tp > 0) {
        buffer_str.append(buffer);
        req_len += tp;
        // reset the buffer
        memset(buffer, 0, BUFFER_SIZE);
        if (buffer_str.find("\r\n\r\n")) {
            break;
        }
    }
    buffer_res = buffer_str.c_str();
    // std::cout << buffer_str << std::endl;

    // get the time
    time_t time_now = time(0);
    char * time_str = ctime(& time_now);
    tm * time_utc = gmtime(& time_now);
    time_str = asctime(time_utc);


    // parse the buffer received from the socket
    httpparser::Request request;
    httpparser::HttpRequestParser parser;
    httpparser::HttpRequestParser::ParseResult res = parser.parse(request, buffer_res, buffer_res + req_len);
    // declare attributes we want to fetch
    std::string request_method;
    std::string request_uri;
    std::string request_msg;
    std::vector<httpparser::Request::HeaderItem> request_headers;
    std::string request_host;


    // if (res != httpparser::HttpRequestParser::ParsingCompleted){
    //     std::cout << "ERROR: Request Parsing failed" << std::endl;
    // }

    request_method = request.method;
    request_uri = request.uri;
    request_msg = request.inspect();
    request_headers = request.headers;
    for (std::vector<httpparser::Request::HeaderItem>::const_iterator it = request_headers.begin();
    it != request_headers.end(); ++it) {
        if (it->name == "Host"){
            request_host = it->value;
            break;
        }
    }

    // print the request
    std::cout << req_id << ": " << "\"" << request.get_header_line() << "\"" << " from " << ip_addr << " @ " << time_str;


    // create a socket to connect to the origin server
    int status;
    int server_sock_fd;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    string port = request.method == "CONNECT" || request_uri.find(":443 ") != string::npos ? "443" : "80";
    request_host = request_host.find(":443") != string::npos ? request_host.substr(0, request_host.find(":443")) : request_host;

    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family   = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(request_host.c_str(), port.c_str(), &host_info, &host_info_list);
    if (status != 0) {
        std::cout << request_host << " " << port << std::endl;
        std::cout << "ERROR: Cannot get address info for host" << std::endl;
        string bad_request = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(browser_sock_fd, bad_request.c_str(), bad_request.length(), 0);
        close(browser_sock_fd);
        return;
    }

    server_sock_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (server_sock_fd == -1) {
        std::cout << "ERROR: Cannot create socket" << std::endl;
        close(browser_sock_fd);
        return;
    }
  
    std::cout << "NOTE: Connecting to " << request_host << " on port " << port << "..." << std::endl;

    status = connect(server_sock_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        std::cout << "ERROR: Cannot connect to server" << std::endl;
        close(server_sock_fd);
        close(browser_sock_fd);
        return;
    }

    freeaddrinfo(host_info_list);


    // handle GET and POST request
    if (request_method == "GET") {
        // check whether the requesting uri is in cache or not
        // not in cache
        if (cache.check_in_cache(request_uri) == false){
            // print the message
            std::cout << req_id << ": " << "Requesting " << "\"" << request.get_header_line() << "\"" << " from " << request.uri << std::endl;

            // send the request to origin server
            int send_req = send(server_sock_fd, buffer_str.c_str(), buffer_str.length(), 0);
            if (send_req < 0) {
                std::cout << "ERROR: Failed to send request to the origin server" << std::endl;
            }

            // get the response from the origin server
            vector<char> re_buffer_str = recv_response(server_sock_fd, browser_sock_fd);
            if (re_buffer_str.empty()) {
            close(server_sock_fd);
            close(browser_sock_fd);
            return;
        }
            

            // parse the response
            httpparser::Response response;
            httpparser::HttpResponseParser re_parser;
            httpparser::HttpResponseParser::ParseResult re_res = re_parser.parse(response, re_buffer_str.data(), re_buffer_str.data() + re_buffer_str.size()); //re_buffer_res + strlen(re_buffer_res));

            // if (re_res != httpparser::HttpResponseParser::ParsingCompleted){
            //     // std::cout << string(re_buffer_str.begin(), re_buffer_str.end()) << std::endl;
            //     std::cout << "ERROR: Response Parsing failed" << std::endl;
            // }

            // fetch the attributes we want
            std::string response_msg = response.inspect();
            std::string response_header = response.get_response_line();

            // print the line
            std::cout << req_id << ": " << "Received " << "\"" << response_header << "\"" << " from " << request.uri << std::endl;

            // response is "200 OK"
            if (response_header.find("200 OK") != std::string::npos){
                //std::cout << "find 200 ok add cache" << std::endl;
                // check whether the response is cacheable or not
                if (check_cacheable(response) == true){
                    // if cacheable, add the new response to the cache
                    // otherwise, do nothing
                    //std::cout << "cacheable" << std::endl;
                    cache.add_cache(req_id, request_uri, response, re_buffer_str);
                }
                else{
                    //std::cout << "not cacheable" << std::endl;
                    if (response.get_cache_control().find("no-store") != std::string::npos){
                        std::cout << req_id << ": " << "not cacheable because " << "no-store" << std::endl;
                    }
                    else if (response.get_cache_control().find("private") != std::string::npos){
                        std::cout << req_id << ": " << "not cacheable because " << "private" << std::endl;
                    }
                }

                // send the response back to the client
                int send_response = send(browser_sock_fd, re_buffer_str.data(), re_buffer_str.size(), 0);
                std::cout << req_id << ": " << "Responding " << "\"" << response_header << "\"" << std::endl;
                if (send_response < 0) {
                    std::cout << "ERROR: Failed to send response to the client" << std::endl;
                }

                // if (string(re_buffer_str.begin(), re_buffer_str.end()).find("chunked")) {
                //     char tp_buff[BUFFER_SIZE];
                //     while (1) {
                //         int len = recv(server_sock_fd, &tp_buff, BUFFER_SIZE, 0);
                //         if (len <= 0) {
                //             break;
                //         }
                //         send(browser_sock_fd, &tp_buff, len, 0);
                //     }
                // }
            }
        }

        // in cache
        else{
            // get the value
            // lock
            std::pair<time_t, std::string> curr_value = cache.get_cache_map()[request.uri];
            time_t expire_time = curr_value.first;
            time_t curr_time = mktime(time_utc);
            std::string curr_response = curr_value.second;

            if ((expire_time < curr_time) || (curr_response.find("no-cache") != std::string::npos) ||
            (curr_response.find("must-revalidate") != std::string::npos)){
                if (expire_time < curr_time){
                    std::cout << req_id << "in cache, but expired at " << asctime(gmtime(&expire_time));
                }
                std::cout << req_id << "in cache, requires validation" << std::endl;

                // get the response from the server
                // send the request to origin server
                int send_req = send(server_sock_fd, buffer_str.c_str(), buffer_str.length(), 0);
                if (send_req < 0) {
                    std::cout << "ERROR: Failed to send request to the origin server" << std::endl;
                }

                // get the response from the origin server
                vector<char> re_buffer_str = recv_response(server_sock_fd, browser_sock_fd);
                if (re_buffer_str.empty()) {
            close(server_sock_fd);
            close(browser_sock_fd);
            return;
        }
                

                // parse the response
                httpparser::Response response;
                httpparser::HttpResponseParser re_parser;
                httpparser::HttpResponseParser::ParseResult re_res = re_parser.parse(response, re_buffer_str.data(), re_buffer_str.data() + re_buffer_str.size()); //re_buffer_res + strlen(re_buffer_res));

                // if (re_res != httpparser::HttpResponseParser::ParsingCompleted){
                //     // std::cout << string(re_buffer_str.begin(), re_buffer_str.end()) << std::endl;
                //     std::cout << "ERROR: Response Parsing failed" << std::endl;
                // }

                // fetch the attributes we want
                std::string response_msg = response.inspect();
                std::string response_header = response.get_response_line();

                // print the line
                std::cout << req_id << ": " << "Received " << "\"" << response_header << "\"" << " from " << request.uri << std::endl;

                // revalid cache
                int revalid = cache.revalid_cache(req_id, request_uri, response);
                if (revalid == 1){
                    // update cache
                    // send the response back to the client
                    int send_response = send(browser_sock_fd, re_buffer_str.data(), re_buffer_str.size(), 0);
                    std::cout << req_id << ": " << "Responding " << "\"" << response_header << "\"" << std::endl;
                    if (send_response < 0) {
                        std::cout << "ERROR: Failed to send response to the client" << std::endl;
                    }
                }
                else if (revalid == 0){
                    // not cacheable
                    // send back the new response
                    // send the response back to the client
                    int send_response = send(browser_sock_fd, re_buffer_str.data(), re_buffer_str.size(), 0);
                    std::cout << req_id << ": " << "Responding " << "\"" << response_header << "\"" << std::endl;
                    if (send_response < 0) {
                        std::cout << "ERROR: Failed to send response to the client" << std::endl;
                    }
                }

            }

            // in cache and valid
            else{
                std::cout << req_id << "in cache, valid" << std::endl;

                // send the response back to the client
                std::vector<char> fetch_response;
                for (std::vector<std::pair<std::string, std::vector<char> > >::iterator it = cache.get_cache_vector().begin() ; it != cache.get_cache_vector().end(); ++it){
                    if (it->first == request_uri){
                        fetch_response = it->second;
                    }
                }

                std::string re_msg = cache.get_cache_map()[request_uri].second;
                size_t idx = re_msg.find("\n");
                std::string re_line = re_msg.substr(0, idx);

                int send_response = send(browser_sock_fd, fetch_response.data(), fetch_response.size(), 0);
                std::cout << req_id << ": " << "Responding " << "\"" << re_line << "\"" << std::endl;
                if (send_response < 0) {
                    std::cout << "ERROR: Failed to send response to the client" << std::endl;
                }
            }

        }
    }

    else if (request_method == "POST") {
        // print the message
        std::cout << req_id << ": " << "Requesting " << "\"" << request.get_header_line() << "\"" << " from " << request.uri << std::endl;

        // send the request to origin server
        int send_req = send(server_sock_fd, buffer_str.c_str(), buffer_str.length(), 0);
        if (send_req < 0) {
            std::cout << "ERROR: Failed to send request to the origin server" << std::endl;
        }

        // get the response from the origin server
        vector<char> re_buffer_str = recv_response(server_sock_fd, browser_sock_fd);
        if (re_buffer_str.empty()) {
            close(server_sock_fd);
            close(browser_sock_fd);
            return;
        }
        

        // parse the response
        httpparser::Response response;
        httpparser::HttpResponseParser re_parser;
        httpparser::HttpResponseParser::ParseResult re_res = re_parser.parse(response, re_buffer_str.data(), re_buffer_str.data() + re_buffer_str.size()); //re_buffer_res + strlen(re_buffer_res));

        // if (re_res != httpparser::HttpResponseParser::ParsingCompleted){
        //     // std::cout << string(re_buffer_str.begin(), re_buffer_str.end()) << std::endl;
        //     std::cout << "ERROR: Response Parsing failed" << std::endl;
        // }

        // fetch the attributes we want
        std::string response_msg = response.inspect();
        std::string response_header = response.get_response_line();

        // print the line
        std::cout << req_id << ": " << "Received " << "\"" << response_header << "\"" << " from " << request.uri << std::endl;


        // send the response back to the client
        int send_response = send(browser_sock_fd, re_buffer_str.data(), re_buffer_str.size(), 0);
        std::cout << req_id << ": " << "Responding " << "\"" << response_header << "\"" << std::endl;
        if (send_response < 0) {
            std::cout << "ERROR: Failed to send response to the client" << std::endl;
        }
    }

    else if (request_method == "CONNECT") {
        std::string conn_response = "HTTP/1.1 200 OK\r\n\r\n";
        int send_conn_res = send(browser_sock_fd, conn_response.c_str(), strlen(conn_response.c_str()), 0);
        // std::cout << "Responding " << "\"" << conn_response << "\"" << std::endl;
        std::cout << "NOTE: CONNECT succeed" << std::endl;
        if (send_conn_res == -1) {
            close(server_sock_fd);
            close(browser_sock_fd);
            std::cout << "ERROR: CONNECT method failed" << std::endl;
            return;
        }

        fd_set read_fd, write_fd;
        int status;
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);

        while(1) {
            FD_ZERO(&read_fd);
            // FD_ZERO(&write_fd);
            FD_SET(browser_sock_fd, &read_fd);
            FD_SET(server_sock_fd, &read_fd);

            status = select(higher_fd(browser_sock_fd, server_sock_fd), &read_fd, NULL, NULL, NULL);
            if (status < 0) {
                std::cout << "ERROR: Select failed" << std::endl;
                break;
            }

            if (FD_ISSET(browser_sock_fd, &read_fd)) {
                // FD_CLR(browser_sock_fd, &read_fd);
                memset(&buffer, 0, sizeof(buffer));

                ssize_t bytes_recv = recv(browser_sock_fd, buffer, BUFFER_SIZE, 0);
                if (bytes_recv <= 0) {
                    std::cout << "NOTE: CONNECT Terminate: browser closed" << std::endl;
                    break;
                }

                ssize_t bytes_send = send(server_sock_fd, buffer, bytes_recv, 0);
                if (bytes_send < 0) {
                    std::cout << "ERROR: Failed to send to server" << std::endl;
                    break;
                }
            }

            else if (FD_ISSET(server_sock_fd, &read_fd)) {
                // FD_CLR(server_sock_fd, &read_fd);
                memset(&buffer, 0, sizeof(buffer));

                ssize_t bytes_recv = recv(server_sock_fd, buffer, BUFFER_SIZE, 0);
                if (bytes_recv <= 0) {
                    std::cout << "NOTE: CONNECT Terminate: server closed" << std::endl;
                    break;
                }
                
                ssize_t bytes_send = send(browser_sock_fd, buffer, bytes_recv, 0);
                if (bytes_send < 0) {
                    std::cout << "ERROR: Failed to send to browser" << std::endl;
                    break;
                }
            }
        }
    }
    // recv and send end, close tunnel
    std::cout << req_id << ": " << "Tunnel closed" << std::endl;

    close(server_sock_fd);
    close(browser_sock_fd);
}


vector<char> recv_response(int server_sock_fd, int browser_sock_fd) {
    int header_len = MAX_RES_HEADER_LEN;    // response header size limit
    vector<char> re_buffer(MAX_RES_HEADER_LEN);
    int index = 0;
    int tp_len = 0;

    // first receive up to 8192 bytes
    while (header_len > 0 && (tp_len = recv(server_sock_fd, &re_buffer.data()[index], header_len, 0)) > 0) {
        header_len -= tp_len;
        index += tp_len;

        // as long as we reach the end of header, break. Then process Content-Length or chunk separately
        // we still need break here because we cannot guarantee the entire response is larger than 8192
        if (string(re_buffer.begin(), re_buffer.begin() + index).find("\r\n\r\n")) {
            break;
        }
    }
    re_buffer.resize(index);

    string tmp(re_buffer.begin(), re_buffer.end());
    string res_header = tmp.substr(0, tmp.find("\r\n\r\n") + 4);

    int content_len_idx = res_header.find("Content-Length: ");
    if (content_len_idx != string::npos) {
        // parse the content length
        int content_len_start = content_len_idx + 16;
        int content_len_end = res_header.find("\r\n", content_len_start);
        int content_len = stoi(res_header.substr(content_len_start, content_len_end - content_len_start));

        // keep receiving if there is content remaining
        int remain = content_len - (MAX_RES_HEADER_LEN - res_header.size());
        // std::cout << "Printing Content-Length: " << content_len << std::endl;
        // std::cout << "Total received so far: " << tp_len << std::endl;
        // std::cout << "Header length: " << res_header.size() << std::endl;
        // std::cout << "Remain to receive: " << remain << std::endl;
        while (remain > 0) {
            re_buffer.resize(index + BUFFER_SIZE);
            tp_len = recv(server_sock_fd, &re_buffer.data()[index], BUFFER_SIZE, 0);
            index += tp_len;
            remain -= tp_len;
            // std::cout << remain << " " << tp_len << std::endl;
        }
        // std::cout << "Total Length: " << res.size() << std::endl;
        re_buffer.resize(index);
    } else if (res_header.find("chunk")) {
        // re_buffer.resize(index + BUFFER_SIZE);
        send(browser_sock_fd, re_buffer.data(), index, 0);
        while (1) {
            re_buffer.clear();
            re_buffer.resize(BUFFER_SIZE);
            tp_len = recv(server_sock_fd, &re_buffer.data()[0], BUFFER_SIZE, 0);
            if (tp_len <= 0) {
                break;
            }
            send(browser_sock_fd, &re_buffer.data()[0], tp_len, 0);
            // end of chunk, break
            // if (string(re_buffer.begin(), re_buffer.begin() + index).find("0\r\n\r\n")) {
            //     break;
            // }
        }
        re_buffer.clear();
    }

    return re_buffer;
}