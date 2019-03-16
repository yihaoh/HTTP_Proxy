#ifndef __REQUEST__H__
#define __REQUEST__H__

//#include "cache.h"
#include "parse.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <error.h>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
using namespace std;

#define BUFF_SIZE 65536

class Request {
private:
  int id;
  int browser_fd;
  int target_sockfd;
  string request;
  int port;
  string host_ip;   // browser's ip addr
  string target_ip; // ip connect to
  string url;
  string response;
  string method;
  string cache_control;
  string header; // GET /foo/fun.html HTTP/1.1
  string bad_request = "HTTP/1.1 400 Bad Request";

  // Cache cache;

public:
  Request(int, int, string);
  void process_request();
  void call_connect();
  void call_get_post();
  void call_revalidate(string); // pass in an etag
  void send_all(string);
  // void parse_request();
  string get_method() { return method; }
  int get_id() { return id; }
  string get_url() { return url; }
  int get_browser_fd() { return browser_fd; }
  int get_target_sockfd() { return target_sockfd; }
  string get_target_ip() { return target_ip; }
  string get_cache_control() { return cache_control; }
  int can_use_cache();
  //~Request() {}
};

#endif
