#ifndef __PARSE_H__
#define __PARSE_H__

#include <climits>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

// This file includes: class Parse_Req & class Parse_Res

// Parse Request
class Parse_Req {
private:
  string request; // full header
  string url;
  string header; // first line
  string host;
  int port;
  string method;
  int content_length;
  string cache_control;

  // parse all wanted fields from request header and save
  void complete_parse() {
    // parse header
    header = request.substr(0, request.find("\r\n"));

    // parse method
    method = request.substr(0, request.find(" "));

    // parse url
    size_t start = request.find(" ") + 1;
    size_t end = request.find(" ", start);
    url = request.substr(start, end - start);

    // parse port
    port = 80;
    string tp = request.substr(request.find("Host:") + 6);
    tp = tp.substr(0, tp.find("\r\n"));
    if (tp.find(":") != string::npos) {
      tp = tp.substr(tp.find(":") + 1, tp.find("\r\n") - tp.find(":") - 1);
      port = stoi(tp);
    }

    // parse host
    tp = request.substr(request.find("Host:") + 6);
    host = tp.substr(0, tp.find("\r\n"));
    host =
        host.find(":") == string::npos ? host : host.substr(0, host.find(":"));

    // parse content length
    start = request.find("Content-Length: ");
    if (start != string::npos) {
      start = request.find(' ', start) + 1;
      size_t end = request.find("\r\n", start);
      string tp = request.substr(start, end - start);
      content_length = stoi(tp);
    } else {
      content_length = 0;
    }

    // parse cache control info
    start = request.find("Cache-Control");
    if (start != string::npos) {
      start = request.find(' ', start) + 1;
      cache_control =
          request.substr(start, request.find("\r\n", start) - start);
    } else {
      cache_control = "";
    }
  }

public:
  Parse_Req(string req) : request(req) { complete_parse(); }
  string get_method() { return method; }
  string get_host() { return host; }
  string get_header() { return header; }
  int get_port() { return port; }
  int get_content_length() { return content_length; }
  string get_url() { return url; }
  string get_cache_control() { return cache_control; }
};

// Parse Response
class Parse_Res {
private:
  string response; // header + body
  string header;   // first line
  string status;
  string cache_control;
  string expired_time;
  string transfer_encoding;
  int content_length;
  string etag;
  string last_modified;

  // parse all wanted fields
  // calculate real expire time
  void complete_parse() {
    // parse header & status (200, 304, 404)
    // cout << response << endl;
    header = response.substr(0, response.find("\r\n"));
    size_t start = header.find(" ") + 1;
    status = header.substr(start, header.find("\r\n") - start);

    // check if need cache
    start = response.find("Cache-Control");
    if (start != string::npos) {
      start = response.find(' ', start) + 1;
      cache_control =
          response.substr(start, response.find("\r\n", start) - start);
    } else {
      cache_control = "";
    }

    // try to explain age: prior to "Expire"
    int age = INT_MAX;
    start = cache_control.find("s-maxage");
    if (start == string::npos) {
      start = cache_control.find("max-age");
    } // s-maxage prior to max-age
    if (start != string::npos) {
      start = cache_control.find('=', start) + 1;
      size_t end = cache_control.find_first_not_of("0123456789", start);
      string maxage = cache_control.substr(start, end - start);
      age = stoi(maxage);
      expired_time = calculate_expired_time(age);
    } else {
      // check expiration time
      start = response.find("Expires");
      if (start != string::npos) {
        start = response.find(' ', start) + 1;
        expired_time =
            response.substr(start, response.find("\r\n", start) - start);
      } else {
        expired_time = "";
      }
    }

    // transfer encoding
    start = response.find("Transfer-Encoding");
    if (start != string::npos) {
      start = response.find(" ", start) + 1;
      transfer_encoding =
          response.substr(start, response.find("\r\n", start) - start);
    } else {
      transfer_encoding = "";
    }

    // get content length
    start = response.find("Content-Length: ");
    if (start != string::npos) {
      start = response.find(' ', start) + 1;
      string tp = response.substr(start, response.find("\r\n", start) - start);
      content_length = stoi(tp);
    } else {
      content_length = 0;
    }

    // get etag
    start = response.find("Etag");
    if (start == string::npos) {
      start = response.find("ETag");
    }
    if (start != string::npos) {
      start = response.find(' ', start) + 1;
      etag = response.substr(start, response.find("\r\n", start) - start);
    } else {
      // cout << "ERROR: etag not found" << endl;
      etag = "";
    }

    // get last modified
    start = response.find("Last-Modified");
    if (start != string::npos) {
      start = response.find(' ', start) + 1;
      last_modified =
          response.substr(start, response.find("\r\n", start) - start);
    } else {
      last_modified = "";
    }
  }

  // return expire time = max age + Date
  string calculate_expired_time(int age) {
    // get date "Date: Wed, 21 Oct 2015 07:28:00 GMT"
    size_t start = response.find("Date");
    if (start == string::npos) {
      return string();
    }

    // convert to tm
    start = response.find(' ', start) + 1;
    size_t end = response.find("\r\n", start);
    string date = response.substr(start, end - start);
    struct tm date_t;
    strptime(date.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &date_t);
    date_t.tm_sec += age;
    mktime(&date_t);
    char buffer[50];
    strftime(buffer, 50, "%a, %d %b %Y %H:%M:%S GMT", &date_t);
    return string(buffer);
  }

public:
  Parse_Res(string res) : response(res) { complete_parse(); }
  string get_header() { return header; }
  string get_status() { return status; }
  string get_cache_control() { return cache_control; }
  string get_expire_time() { return expired_time; }
  string get_transfer_encoding() { return transfer_encoding; }
  int get_content_length() { return content_length; }
  string get_etag() { return etag; }
  string get_last_modified() { return last_modified; }
};

#endif
