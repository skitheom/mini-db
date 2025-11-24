#include "KVStore.hpp"
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

volatile sig_atomic_t g_stop = 0;

// 配布コードを基にした main.cpp
class Socket {
private:
  int _sockfd;
  struct sockaddr_in _servaddr;

public:
  Socket(int port) : _sockfd(socket(AF_INET, SOCK_STREAM, 0)) {
    if (_sockfd == -1) {
      throw std::runtime_error("Socket creation failed");
    }
    ///////////////////////////////////////////////////////////////
    // Added
    int fl = fcntl(_sockfd, F_GETFL);
    if (fl == -1 || fcntl(_sockfd, F_SETFL, fl | O_NONBLOCK) == -1) {
      ::close(_sockfd);
      throw std::runtime_error("fcntl");
    }

    int yes = 1;
    if (::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) ==
        -1) {
      ::close(_sockfd);
      throw std::runtime_error("setsockopt");
    }
    ///////////////////////////////////////////////////////////////
    memset(&_servaddr, 0, sizeof(_servaddr));
    _servaddr.sin_family = AF_INET;
    _servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    _servaddr.sin_port = htons(port);
  }

  ~Socket() {
    if (_sockfd != -1) {
      close(_sockfd);
    }
  }
  void bindAndListen() {
    if (bind(_sockfd, (struct sockaddr *)&_servaddr, sizeof(_servaddr)) < 0) {
      throw std::runtime_error("Socket bind failed");
    }

    if (listen(_sockfd, 10) < 0) {
      throw std::runtime_error("Socket listen failed");
    }
  }
  ///////////////////////////////////////////////////////////////
  // Added
  int fd() { return _sockfd; }

  // 修正: throw std::runtime_error を投げない + fcntl O_NONBLOCK
  int accept(sockaddr_in &cli) {
    socklen_t len = sizeof(cli);
    int cfd = ::accept(_sockfd, (sockaddr *)&cli, &len);
    if (cfd < 0) {
      return -1;
    }
    int fl = fcntl(cfd, F_GETFL);
    if (fl == -1 || fcntl(cfd, F_SETFL, fl | O_NONBLOCK) == -1) {
      ::close(cfd);
      return -1;
    }
    return cfd;
  }
  ///////////////////////////////////////////////////////////////
  /*
  // 配布コード
    int accept(struct sockaddr_in &clientAddr) {
    socklen_t clientLen = sizeof(clientAddr);
    int clientSocketFd =
        ::accept(_sockfd, (struct sockaddr *)&clientAddr, &clientLen);
    if (clientSocketFd < 0) {
      throw std::runtime_error("Failed to accept connection");
    }
    return clientSocketFd;
  }
  */
  std::string pullMessage() { return ("Totaly not pulled message"); }
};

class Server {
private:
  Socket _listeningSocket;
  ///////////////////////////////////////////////////////////////
  // Added
  KVStore &_kv;
  std::vector<int> _clients;
  std::map<int, std::string> _inbuf;
  std::map<int, std::string> _outbuf;
  ///////////////////////////////////////////////////////////////

public:
  Server(int port, KVStore &kv)
      : _listeningSocket(port), _kv(kv), _clients(), _inbuf(), _outbuf() {}

  ///////////////////////////////////////////////////////////////
  // Added
  ~Server() {
    for (size_t i = 0; i < _clients.size(); ++i) {
      ::close(_clients[i]);
    }
  }

  void accept_client() {
    for (;;) { // blocking運用の場合はloop不要
      sockaddr_in addr;
      int cfd = _listeningSocket.accept(addr);
      if (cfd < 0)
        return;
      if (cfd >= FD_SETSIZE) {
        ::close(cfd);
        return;
      }
      _clients.push_back(cfd);
      _inbuf[cfd].clear();
      _outbuf[cfd].clear();
    }
  }

  void remove_client(int cfd) {
    ::close(cfd);
    for (size_t i = 0; i < _clients.size(); ++i) {
      if (_clients[i] == cfd) {
        _clients[i] = _clients.back();
        _clients.pop_back();
        break;
      }
    }
    _inbuf.erase(cfd);
    _outbuf.erase(cfd);
  }

  void process_buffer(int cfd) {
    std::string &inbuf = _inbuf[cfd];
    std::string &outbuf = _outbuf[cfd];
    static const size_t kMaxLine = 1000;

    if (inbuf.size() > kMaxLine && inbuf.find('\n') == std::string::npos) {
      outbuf += "2\n";
      inbuf.clear();
      return;
    }

    size_t pos;
    while ((pos = inbuf.find('\n')) != std::string::npos) {

      std::string line = inbuf.substr(0, pos);
      inbuf.erase(0, pos + 1);
      if (line.size() > kMaxLine) {
        outbuf += "2\n";
        continue;
      }
      if (line.empty())
        continue;

      std::string response = _kv.process_line(line);
      outbuf.append(response);
    }
  }

  bool on_read(int cfd) {
    char buf[4096];
    ssize_t n = ::recv(cfd, buf, sizeof(buf), 0);
    if (n > 0) {
      _inbuf[cfd].append(buf, n);
      process_buffer(cfd);
      return true;
    }
    if (n == 0) {
      remove_client(cfd);
      return false;
    }
    // 本番はいらないと思う（一応メモ）
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      return true;
    }
    remove_client(cfd);
    return false;
  }

  bool on_write(int cfd) {
    std::string &outbuf = _outbuf[cfd];

    if (outbuf.empty()) {
      return true;
    }
    ssize_t n = ::send(cfd, outbuf.data(), outbuf.size(), 0);
    if (n > 0) {
      outbuf.erase(0, n);
      return true;
    }
    if (n == 0) {
      return true; // retry
    }
    // 本番では多分いらないけど一応メモ
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return true; // retry
    }
    remove_client(cfd);
    return false;
  }

  void select_loop() {

    fd_set rfds, wfds;
    int listenfd = _listeningSocket.fd();

    while (g_stop == 0) {
      FD_ZERO(&rfds);
      FD_ZERO(&wfds);

      // listening fd
      FD_SET(listenfd, &rfds);
      int maxfd = listenfd;

      // client fds
      for (size_t i = 0; i < _clients.size(); ++i) {
        int cfd = _clients[i];
        FD_SET(cfd, &rfds);
        maxfd = (cfd > maxfd) ? cfd : maxfd;
        if (!_outbuf[cfd].empty()) {
          FD_SET(cfd, &wfds);
        }
      }

      int n = ::select(maxfd + 1, &rfds, &wfds, 0, 0);
      if (n == -1) {
        if (errno == EINTR) {
          continue;
        }
        throw std::runtime_error("select");
      }
      if (g_stop)
        break;
      // server fd event
      if (FD_ISSET(listenfd, &rfds)) {
        accept_client();
      }
      // client fd event
      for (size_t i = 0; i < _clients.size();) {
        int cfd = _clients[i];
        bool advance = true;

        if (FD_ISSET(cfd, &rfds))
          advance = on_read(cfd);
        if (advance && FD_ISSET(cfd, &wfds))
          advance = on_write(cfd);
        if (advance) {
          ++i;
        }
      }
    }
  }
  ///////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////
  // Modified
  void run() {
    _listeningSocket.bindAndListen();
    std::cout << "ready\n" << std::flush;
    select_loop();
  }
  ///////////////////////////////////////////////////////////////
};

static void on_sigint(int) { g_stop = 1; }

static int parse_port(const char *s) {
  char *end = 0;
  errno = 0;
  long v = std::strtol(s, &end, 10);
  if (!s || *s == '\0' || *end != '\0' || v < 1 || v > 65535 || errno != 0) {
    return -1;
  }
  return static_cast<int>(v);
}

int main(int argc, char *argv[]) {
  if (argc != 3 || !argv[1][0] || !argv[2][0]) {
    return 1;
  }

  int port = parse_port(argv[1]);
  if (port < 0)
    return 1;

  ::signal(SIGPIPE, SIG_IGN);
  ::signal(SIGINT, on_sigint);

  KVStore kv; // throw を投げない
  kv.load_file(argv[2]);

  try {
    Server server(port, kv);
    server.run();
  } catch (const std::exception &e) {
    (void)e; // error msg に関する指定がない
    return 1;
  }
  if (g_stop) {
    kv.save_file(argv[2]);
  }
  return 0;
}
