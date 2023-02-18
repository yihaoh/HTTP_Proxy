#include "request.h"

// Request Object Constructor
Request::Request(int id, int browser_fd, string host_ip)
    : id(id), browser_fd(browser_fd), host_ip(host_ip) {}

//****************************************************************//
/*
  process_request receives request from client, check if valid,
  and put it in a correct format so it is ready to be sent out
  by either call_connect or call_get_post
 */
void Request::process_request() {
  // request holds the text of the entire request, build it up from scratch
  request = "";

  // use a buffer to continuously fetch data from client (brwoser)
  char *buff = new char[BUFF_SIZE];
  memset(buff, 0, BUFF_SIZE);
  int data_len;
  int cur = 0;
  while ((data_len = recv(browser_fd, buff, BUFF_SIZE, 0)) > 0) {
    // add whatever is received to the request, receive number of char range is [0, BUFF_SIZE], we don't know how many exactly is received each time
    request.append(string(buff));
    cur += data_len;

    // \r\n\r\n marks the end of request header (check https://docs.citrix.com/en-us/citrix-adc/current-release/appexpert/http-callout/http-request-response-notes-format.html)
    if (request.find("\r\n\r\n") != string::npos) {
      // no need to worry about body for GET and CONNECT (because there is none)
      if (request.find("GET") != string::npos ||
          request.find("CONNECT") != string::npos) {
        break;
      }

      // if POST request
      else {
        // check content length
        int header_len = request.find("\r\n\r\n") + 4;  // \r\n\r\n marks the end of header, so the header len is the index of \r + 4
        unsigned long found_content = request.find("Content-Length:");
        if (found_content != string::npos) {
          string tp = request.substr(found_content);
          tp = tp.substr(16, tp.find("\r\n") - 16); // find the string between : and \r\n, this is the content length (body_len)
          int body_len = stoi(tp);
          if (header_len + body_len <= cur) {
            // if header and body are all received, break
            break;
          }
        } else if (request.find("chunked") != string::npos &&
                   request.find("0\r\n\r\n") != string::npos) {
          break;
        } else {
          break;
        }
      }
    }
  }

  delete[] buff;

  // modify this later
  if (data_len < 0) {
    cout << "Error: " << id << ": recv fail" << endl;
    return;
  }

  // request does not end properly, send the bad request to client
  if (request.find("\r\n\r\n") == string::npos) {
    send(browser_fd, bad_request.c_str(), bad_request.length(), 0);
    return;
  }

  // parse request
  Parse_Req Request(request);
  port = Request.get_port();
  target_ip = Request.get_host();
  url = Request.get_url();
  header = Request.get_header();
  method = Request.get_method();
  cache_control = Request.get_cache_control();

  // cout << id << ": " << header << endl;

  //  cout << id << ": full RAW request:  " << request << endl;

  // cut redumdunt part: host in the url
  // if target ip can be found in first line in request (e.g. GET ip/.../... )
  // we need to remove the ip address in the first line, otherwise there will be some problems
  size_t need_cut = header.find(target_ip);
  if (need_cut != string::npos && (method == "GET" || method == "POST")) {
    string first_line = header.substr(need_cut + target_ip.length());
    first_line = method + " " + first_line;
    request = first_line + request.substr(request.find("\r\n"));
  }
  // if Connection field exist, set it to close, otherwise the website will never finish loading even if all content is loaded
  size_t connection = request.find("Connection:");
  if (connection != string::npos && (method == "GET" || method == "POST")) {
    string tp = request.substr(connection);
    tp = tp.substr(0, tp.find("\r\n") + 2);
    request = request.substr(0, connection) + "Connection: close\r\n" +
              request.substr(connection + tp.size());
  }

  // cout << id << ": full REAL request: " << request << endl;

  // Log Request
  time_t cur_time = time(NULL);
  cout << id << ": " << Request.get_header() << " from " << host_ip << " @ "
       << asctime(gmtime(&cur_time)) << endl;
}

//*********************************************************************************//
/*
 call_connect implement the "CONNECT" request, building a tunnel for client and
 server to transmit data
 */
void Request::call_connect() {
  struct addrinfo hint, *server;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;

  try {
    if (getaddrinfo(target_ip.c_str(), to_string(port).c_str(), &hint,
                    &server) != 0) {
      cout << "ERROR: " << id << "connect getaddrinfo" << endl;
      return;
    }

    // create socket to connect to target
    target_sockfd =
        socket(server->ai_family, server->ai_socktype, server->ai_protocol);

    if (connect(target_sockfd, server->ai_addr, server->ai_addrlen) < 0) {
      cout << "ERROR: " << id << "connect connect" << endl;
      close(target_sockfd);
      return;
    }

    // connection succeed, tell browser
    string success("HTTP/1.1 200 Connection Established\r\n\r\n");
    if (send(browser_fd, success.c_str(), strlen(success.c_str()), 0) < 0) {
      // error occur
      cout << "ERROR: " << id << ": when sending back success" << endl;
    }

    // start forwarding packets back and forth
    fd_set readfds;
    int highest = max(browser_fd, target_sockfd);
    char *buff = new char[BUFF_SIZE];
    memset(buff, 0, BUFF_SIZE);
    while (1) {
      // register both target and browser to readfds
      FD_ZERO(&readfds);
      FD_SET(target_sockfd, &readfds);
      FD_SET(browser_fd, &readfds);

      // use select to see which channel (browser or target) got stuff to transmit
      if (select(highest + 1, &readfds, NULL, NULL, NULL) < 0) {
        cout << "ERROR: " << id << ": when sending back success" << endl;
        break;
      }

      memset(buff, 0, BUFF_SIZE);

      // browser send to server
      if (FD_ISSET(browser_fd, &readfds)) {
        // receive data from browser
        int data_len = recv(browser_fd, buff, BUFF_SIZE, 0);

        if (data_len < 0) {
          // error occur
          break;
        } else if (data_len == 0) {
          break; // done
        }

        // send data to target
        if (send(target_sockfd, buff, data_len, 0) < 0) {
          // error occur
          cout << "sending back error" << endl;
          break;
        }

      } else if (FD_ISSET(target_sockfd, &readfds)) { // server send to browser
        // receive data from target
        int data_len = recv(target_sockfd, buff, BUFF_SIZE, 0);

        if (data_len < 0) {
          // error occur
          break;

        } else if (data_len == 0) {
          break; // done
        }

        // send data to browser
        if (send(browser_fd, buff, data_len, 0) < 0) {
          // error occur
          cout << "ERROR: " << id << ": sending back error" << endl;
          break;
        }
      }
    }
    // done forwarding, clear everything
    close(target_sockfd);
    close(browser_fd);
    delete[] buff;

    cout << id << ": Tunnel Closed" << endl;
  } catch (...) {
    close(target_sockfd);
    close(browser_fd);
    return;
  }
}

//*******************************************************************//
/*
  call_get_post implements "GET" and "POST" request, it simply
  send client request to the server
 */
void Request::call_get_post() {
  struct addrinfo hint, *server;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;

  try {
    if (getaddrinfo(target_ip.c_str(), to_string(port).c_str(), &hint,
                    &server) != 0) {
      cout << "ERROR: " << id << ": post get getaddrinfo error" << endl;
      close(target_sockfd);
      return;
    }

    // create socket to connect to target
    target_sockfd =
        socket(server->ai_family, server->ai_socktype, server->ai_protocol);

    if (connect(target_sockfd, server->ai_addr, server->ai_addrlen) < 0) {
      // error occur
      cout << "ERROR: " << id << ": post get connect error" << endl;
      close(target_sockfd);
      return;
    }

    cout << id << ": Requesting \"" << header << "\" from " << target_ip
         << endl;

    // send out request at once
    if (send(target_sockfd, request.c_str(), request.length(), 0) < 0) {
      cout << "ERROR: " << id << ": post or get send error" << endl;
      return;
    }
  } catch (...) {
    close(target_sockfd);
    close(browser_fd);
    return;
  }
}

//******************************************************************//
/*
  call_revalidate simply sends out a revalidate request to server
 */
void Request::call_revalidate(string etag) {
  struct addrinfo hint, *server;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;

  try {
    if (getaddrinfo(target_ip.c_str(), to_string(port).c_str(), &hint,
                    &server) != 0) {
      // error occur
      cout << "ERROR: " << id << ": post get getaddrinfo error" << endl;
      close(target_sockfd);
      return;
    }

    // create socket to connect to target
    target_sockfd =
        socket(server->ai_family, server->ai_socktype, server->ai_protocol);

    if (connect(target_sockfd, server->ai_addr, server->ai_addrlen) < 0) {
      // error occur
      cout << "ERROR: " << id << ": revalidate connect error" << endl;
      close(target_sockfd);
      return;
    }

    // Construct revalidate request
    string reval_req = header + "\r\n" + "If-None-Match: " + etag +
                       "\r\nHost: " + target_ip + "\r\n\r\n";

    if (send(target_sockfd, reval_req.c_str(), reval_req.length(), 0) < 0) {
      cout << "revalidate send error" << endl;
    }
  } catch (...) {
    close(target_sockfd);
    close(browser_fd);
  }
}

//******************************************************************//
/*
  send_all sends all bytes in request to the server as a single
  send might not actually send all
 */
void Request::send_all(string req) {
  size_t sent = 0;
  while (1) {
    if (sent < req.size()) {
      sent += send(target_sockfd, &(req.data()[sent]), BUFF_SIZE, 0);
    } else {
      sent += send(target_sockfd, &(req.data()[sent]), req.size() - sent, 0);
      break;
    }
  }
  return;
}

//***********************************************************//
/*
  check if the request can use cache
 */
int Request::can_use_cache() {
  if (method != "GET") {
    return -1;
  }
  string control = cache_control;

  if (control.find("no-store") != string::npos) {
    return -1; // cannot
  }

  if (control.find("only-if-cached") != string::npos) {
    return 1; // can
  }

  return 0; // proxy should validate age of the cached file
}
