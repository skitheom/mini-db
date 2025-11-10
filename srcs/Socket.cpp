// Socket.cpp
#include "Socket.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

Socket::Socket(int port) : listen_fd_(make_listen(port)) {}

Socket::~Socket() noexcept {
  if (listen_fd_ != -1)
    ::close(listen_fd_);
}

int Socket::fd() const { return listen_fd_; }

int Socket::make_listen(int port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);

  if (fd == -1)
    throw std::runtime_error("socket");
  int fl = fcntl(fd, F_GETFL);
  if (fl == -1 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) == -1) {
    ::close(fd);
    throw std::runtime_error("fcntl");
  }

  int yes = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
    ::close(fd);
    throw std::runtime_error("setsockopt");
  }

  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
  addr.sin_port = htons(port);

  if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1) {
    ::close(fd);
    throw std::runtime_error("bind");
  }

  if (::listen(fd, SOMAXCONN) == -1) {
    ::close(fd);
    throw std::runtime_error("listen");
  }
  return fd;
}
