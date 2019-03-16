#include "proxy.h"

// new thread: deal with one request and send back the corresponding response
void new_request(Proxy *proxy, int id, int fd, string s, Cache *cache) {
  Request req(id, fd, s);
  req.process_request();

  try {
    if (req.get_method() == "CONNECT") {
      req.call_connect();
    }

    else if (req.get_method() == "POST" || req.get_method() == "GET") {
      // -1: normal call_get_post, 0: revalidate, 1: use cache
      switch (cache->in_cache(req)) {
      case -1: {
        cout << req.get_id() << ": not in cache" << endl;
        req.call_get_post();
        Response res(req);
        res.process_response();
        // check if cacheable
        if (res.check_cacheablity()) {
          cache->save(res);
        }
        break;
      }
      case 0: {
        cout << "NOTE " << req.get_id() << ": revalidating..." << endl;
        req.call_revalidate(cache->get_response(req).get_etag());
        Response res(req);
        int status = res.process_response();
        if (status == 0) { // if 304 not modified
          cout << "NOTE " << req.get_id() << ": Not modified, send back cache"
               << endl;
          proxy->send_copy(fd, req);
        }
        // 200 ok
        else if (status == 1) {
          cout << "NOTE " << req.get_id()
               << ": Modified, send back new response, save" << endl;
          if (res.check_cacheablity()) {
            cache->save(res);
          }
        }
        break;
      }
      case 1: {
        // use cache
        // cout << req.get_id() << ": in cache, valid" << endl;
        proxy->send_copy(fd, req);
        break;
      }
      }
    }
  } catch (...) {
    close(fd);
    return;
  }
  return;
}

//*************************Proxy class functions*****************************//

// Proxy Object Constructor, set up the server functionalities for proxy
// If any setup fail, exit the whole program
Proxy::Proxy() {
  memset(&host, 0, sizeof(host));
  host.ai_family = AF_UNSPEC;
  host.ai_socktype = SOCK_STREAM;
  host.ai_flags = AI_PASSIVE;
  status = getaddrinfo(NULL, SERVERPORT, &host, &host_list);

  if (status != 0) {
    cerr << "Error: address issue" << endl;
    exit(EXIT_FAILURE);
  }

  server_sockfd = socket(host_list->ai_family, host_list->ai_socktype,
                         host_list->ai_protocol);

  if (server_sockfd == -1) {
    cerr << "Error: socket creation failed" << endl;
    exit(EXIT_FAILURE);
  }

  int yes = 1;
  status =
      setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  if (status == -1) {
    cerr << "Error: socket operation fail" << endl;
    exit(EXIT_FAILURE);
  }

  status = bind(server_sockfd, host_list->ai_addr, host_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: Binding fail" << endl;
    exit(EXIT_FAILURE);
  }

  status = listen(server_sockfd, 1000);
  if (status == -1) {
    cerr << "Error: listen fail" << endl;
    exit(EXIT_FAILURE);
  }
}

/*
  Run the proxy, accept connection from clients and start new thread
  to process requests
 */
void Proxy::Run() {
  // cout << "proxy starts..." << endl;

  int id = 0;
  while (1) {
    id++;
    sin_size = sizeof(their_addr);
    int browser_fd =
        accept(server_sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (browser_fd == -1) {
      cout << " Error: accepting connection fail" << endl;
      continue;
    }

    thread(new_request, this, id, browser_fd,
           string(inet_ntoa(((struct sockaddr_in *)&their_addr)->sin_addr)),
           &cache)
        .detach();
  }
  close(server_sockfd);
}

/*
  Send response copy from cache to clients
 */
void Proxy::send_copy(int browser_fd, Request req) {
  // just send back the cached response
  string res = cache.get_response(req).get_whole_response();
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

// main function
int main() {
  // become a daemon
  pid_t mypid = getpid();
  pid_t pid, sid;
  int out;

  // deamon
  if (mypid != 1) {

    pid = fork();
    if (pid < 0) {
      exit(EXIT_FAILURE);
    }

    if (pid > 0) {
      exit(EXIT_SUCCESS);
    }

    umask(0);

    char buf[512]; // to store the cwd
    getcwd(buf, 512);
    chdir("/var/log");
    mkdir("./erss", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    chdir(buf);

    out = open("/var/log/erss/proxy.log", O_WRONLY | O_TRUNC | O_CREAT,
               S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
    dup2(out, 1);
    close(STDIN_FILENO);
    close(STDERR_FILENO);

    // close(STDOUT_FILENO);

    sid = setsid();
    if (sid < 0) {
      exit(EXIT_FAILURE);
    }

    if ((chdir("/")) < 0) {
      exit(EXIT_FAILURE);
    }
  }
  // docker
  else {
    // mkdir("./erss", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    out = open("/var/log/erss/proxy.log", O_WRONLY | O_TRUNC | O_CREAT,
               S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
    if (out == -1) {
      cout << "cannot open file" << endl;
      exit(EXIT_FAILURE);
    }
    dup2(out, 1);

    close(STDIN_FILENO);
    close(STDERR_FILENO);
  }
  // start
  Proxy p;
  p.Run();
  close(out);
  return 0;
}
