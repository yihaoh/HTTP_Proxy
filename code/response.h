#ifndef __RESPONSE__H__
#define __RESPONSE__H__

#include "parse.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <error.h>
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
#include <time.h>
#include <unistd.h>
#include <unordered_map>

#include "request.h"
using namespace std;

#define BUFF_SIZE 65536

class Response {
private:
  int id;
  string url; // request url
  string response;
  int target_sockfd;
  int browser_fd;
  string status;
  string cache_control;
  string expires;
  bool is_get;
  string server; // for log just one sentence
  string etag;
  string last_modified;
  string transfer_encoding;

public:
  // seems like this constructor is never used, might discard later
  Response(int id_, string url_, int target_sockfd_, int browser_fd_,
           bool is_get_, string server_);

  Response(Request &req);
  int process_response();
  vector<char> process_content_len(vector<char>, int, int);
  vector<char> process_chunk(vector<char>, int);
  void send_all(vector<char>);
  void parse_header(Parse_Res &parse);
  bool check_cacheablity();
  string get_url() { return url; }
  int get_id() { return id; }
  string get_expire_time() { return expires; }
  string get_cache_control() { return cache_control; }
  string get_etag() { return etag; }
  string get_last_modified() { return last_modified; }
  string get_whole_response() { return response; }
};

#endif
