#include "response.h"

//************************ Response Object Constructors*********************//
Response::Response(int id_, string url_, int target_sockfd_, int browser_fd_,
                   bool is_get_, string server_)
    : id(id_), url(url_), target_sockfd(target_sockfd_),
      browser_fd(browser_fd_), is_get(is_get_), server(server_) {}

Response::Response(Request &req) {
  id = req.get_id();
  url = req.get_url();
  target_sockfd = req.get_target_sockfd();
  browser_fd = req.get_browser_fd();
  is_get = req.get_method().compare("GET") == 0;
  server = req.get_target_ip();
  cache_control = req.get_cache_control();
}

//******************* Response Class Member Functions ***************//

/*
   process response
   receive response/revalidation result from server
   save as a response object
   send back response if not in cache
*/
int Response::process_response() {
  try {
    int index = 0;
    char *buff = new char[8192];
    memset(buff, 0, sizeof(*buff));
    vector<char> char_buff;
    int len_data = recv(target_sockfd, buff, 8192, 0);
    string temp(buff);
    string header = temp.substr(0, temp.find("\r\n\r\n") + 4);

    for (int i = header.size(); i < len_data; i++) {
      char_buff.push_back(buff[i]);
    }
    index = char_buff.size();

    // revalidation: valid, return
    if (header.find("304 Not Modified") != string::npos) {
      return 0;
    }

    vector<char> char_header;
    for (size_t i = 0; i < header.size(); i++) {
      char_header.push_back(header[i]);
    }

    // new response
    // parse header
    Parse_Res parse(header);
    parse_header(parse);

    // chunked case
    if (parse.get_transfer_encoding().find("chunk") != string::npos) {
      char_buff = process_chunk(char_buff, index);
    }
    // content length case
    else {
      char_buff =
          process_content_len(char_buff, index, parse.get_content_length());
    }

    // send back response to client
    response = header + string(char_buff.begin(), char_buff.end());

    /*if (send(browser_fd, header.c_str(), header.length(), 0) < 0) {
      cout << "ERROR: " << id << ": response send header error" << endl;
      return -1;
    }
    if (send(browser_fd, char_buff.data(), char_buff.size(), 0) < 0) {
      cout << "ERROR: " << id << ": response send body error" << endl;
      return -1;
      }*/
    send_all(char_header);
    send_all(char_buff);

    delete[] buff;

    close(browser_fd);
    close(target_sockfd);
    return 1;

  } catch (...) {

    close(browser_fd);
    close(target_sockfd);
    return 1;
  }
}

/*
  Response has Content-Length keyword, receive data until data length
  reach Content-Length
 */
vector<char> Response::process_content_len(vector<char> buff, int index,
                                           int content_len) {
  int data_len;

  if (content_len > 0) {
    while ((int)buff.size() < content_len) {
      buff.resize(index + BUFF_SIZE);
      data_len = recv(target_sockfd, &(buff.data()[index]), BUFF_SIZE, 0);
      index += data_len;
      if (data_len < BUFF_SIZE && data_len > 0) {
        buff.resize(index);
      }
      if (data_len <= 0) {
        return buff;
      }
    }
  }
  return buff;
}

/*
  Response has "chunked" transfer encoding, receive data until the end of
  chunks indicated by "0\r\n\r\n"
 */
vector<char> Response::process_chunk(vector<char> buff, int index) {
  int data_len = 1;
  while (data_len != 0) {
    buff.resize(index + BUFF_SIZE);
    data_len = recv(target_sockfd, &(buff.data()[index]), BUFF_SIZE, 0);

    index += data_len;
    if (data_len < BUFF_SIZE && data_len > 0) {
      buff.resize(index);
    }
    if (data_len <= 0) {
      break;
    }

    if (string(buff.begin(), buff.end()).find("0\r\n\r\n") != string::npos) {
      break;
    }
  }

  return buff;
}

/*
  Guarantee all data is sent as a single send might only send
  a portion of data
 */
void Response::send_all(vector<char> res) {
  size_t sent = 0;
  while (1) {
    if (sent + BUFF_SIZE < res.size()) {
      sent += send(browser_fd, &(res.data()[sent]), BUFF_SIZE, 0);
    } else {
      sent += send(browser_fd, &(res.data()[sent]), res.size() - sent, 0);
      break;
    }
  }
  return;
}

/*
 fill in response object with parse_res
*/
void Response::parse_header(Parse_Res &parse) {
  // save fields that may be needed later
  response = parse.get_header();
  cout << id << ": "
       << "Received \"" << response << "\" from " << server << endl;

  cout << id << ": "
       << "Responding \"" << response << "\"" << endl;

  status = parse.get_status();
  cache_control += string(" ") + parse.get_cache_control();
  expires = parse.get_expire_time();
  etag = parse.get_etag();
  last_modified = parse.get_last_modified();
  transfer_encoding = parse.get_transfer_encoding();
}

/*
 check if this new response can be cached
*/
bool Response::check_cacheablity() {
  if ((status != "200 OK") || !is_get) {
    return false;
  }

  if (cache_control.find("no-store") != string::npos) {
    cout << id << ": not cacheable because " << cache_control << endl;
    return false;
  }

  if (transfer_encoding.find("chunk") != string::npos) {
    cout << id << ": not cacheable because transfer-encoding = chunk" << endl;
    return false;
  }

  if (cache_control.find("public") != string::npos &&
      cache_control.find("age") != string::npos) {
    cout << id << ": cached, expires at " << expires << endl;
    return true;
  }

  if (cache_control.find("only-if-cached") != string::npos) {
    cout << id << ": cached, expires at " << expires << endl;
    return true;
  }

  if (!expires.empty()) {
    cout << id << ": cached, expires at " << expires << endl;
    return true;
  }

  if (!etag.empty() || !last_modified.empty() ||
      cache_control.find("revalidate") != string::npos) {
    //    cout << "NOTE: " << id << ": " << etag << endl;
    cout << id << ": cached, but requires re-validation" << endl;
    return true;
  }

  return false;
}
