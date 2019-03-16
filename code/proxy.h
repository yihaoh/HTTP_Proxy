#ifndef PROXY_H
#define PROXY_H

#include "cache.h"
#include "request.h"
#include "response.h"
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
#include <thread>
#include <unistd.h>
using namespace std;

#define SERVERPORT "12345"
// Cache cache;

class Proxy {
private:
  /* Proxy server variables  */
  int server_sockfd;
  // int browser_fd;
  int status;
  struct addrinfo host;
  struct addrinfo *host_list;
  // struct addrinfo *p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  /**********************************/

  Cache cache;
  /* Proxy client variables */

public:
  Proxy();
  void Run();
  int init_log();
  ~Proxy() {}
  // void new_request(int id, int fd, string s);
  void send_copy(int, Request);
};

#endif
