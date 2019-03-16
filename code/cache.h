#ifndef __CACHE__H__
#define __CACHE__H__
#include "response.h"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>

#define CACHE_SIZE 100
mutex cache_mtx; // lock map to avoid data race

class Cache {
private:
  // key = url; value = class Response
  unordered_map<string, Response> data;
  size_t size; // max size

public:
  Cache() : size(CACHE_SIZE) {}

  // get cached response use url inside the new request object
  Response get_response(Request &req) {
    return data.find(req.get_url())->second;
  }

  // save response into cache, kick the oldest one when the cache is full
  void save(Response &res) {
    lock_guard<mutex> lock(cache_mtx);
    unordered_map<string, Response>::iterator it = data.find(res.get_url());
    if (it != data.end()) {
      it->second = res;
      return;
    }

    // cache full, kick the oldest one
    if (data.size() >= size) {
      it = data.begin();
      unordered_map<string, Response>::iterator min_res;
      int min_id = INT_MAX;
      for (; it != data.end(); ++it) {
        if (it->second.get_id() < min_id) {
          min_res = it;
          min_id = it->second.get_id();
        }
      }
      data.erase(min_res);
    }

    data.emplace(res.get_url(), res);
  }

  // check: 1. if the request allows the proxy to use cache
  //        2. if the response has been cached before
  //        3. if the proxy has to revalidate it whenever the expire time
  //        4. if the cached reponse is expired -> revalidate
  int in_cache(Request &req) {
    lock_guard<mutex> lock(cache_mtx);

    int req_says = req.can_use_cache();
    if (req_says == -1) {
      cout << "NOTE " << req.get_id() << ": request says cannot use cache"
           << endl;
      return -1; // request says cannot use cache
    }

    unordered_map<string, Response>::iterator it = data.find(req.get_url());
    if (it == data.end()) {
      // cout << "NOTE: " << req.get_id() << ": not in cache" << endl;
      return -1; // not in cache: cannot use cache anyway
    }

    if (req_says == 1) {
      cout << req.get_id() << " in cache & validation not required" << endl;
      // cout << req.get_id() << ": in cache, valid" << endl;
      return 1; // in cache & validation not required
    }

    // response says have to revalidte
    if (it->second.get_cache_control().find("no-cache") != string::npos ||
        it->second.get_cache_control().find("revalidate") != string::npos) {
      if (it->second.get_etag().empty() &&
          it->second.get_last_modified().empty()) {
        data.erase(it);
        cout << "NOTE: " << req.get_id()
             << ": need revalidate but no etag info, delete cache" << endl;
        return -1; // if there is no etag info -> impossible to revalidate
      }
      cout << req.get_id() << ": in cache, requires validation" << endl;
      return 0;
    }

    if (!it->second.get_expire_time().empty()) {
      // validate age
      time_t now_t;
      time(&now_t);
      struct tm *now = gmtime(&now_t);
      struct tm expire;
      strptime(it->second.get_expire_time().c_str(),
               "%a, %d %b %Y %H:%M:%S GMT", &expire);

      if (difftime(mktime(&expire), mktime(now)) > 0) {
        char buffer[50];
        memset(buffer, 0, 50);
        strftime(buffer, 50, "%a, %d %b %Y %H:%M:%S GMT", now);
        cout << "NOTE " << req.get_id()
             << ": valid, expired at: " << it->second.get_expire_time() << endl;
        cout << "NOTE " << req.get_id() << ": now = " << buffer << endl;
        return 1; // valid, use it
      } else {
        cout << req.get_id() << ": in cache, but expired at "
             << it->second.get_expire_time() << endl;
        return 0; // expired, revalidate
      }
    }

    if (!it->second.get_etag().empty()) {
      cout << req.get_id() << ": in cache, requires validation" << endl;
      return 0;
    } else {
      cout << req.get_id() << ": in cache, valid" << endl;
      return 1;
    }
  }

  // unordered_map getter
  unordered_map<string, Response> &get_data() { return data; }
};

#endif
