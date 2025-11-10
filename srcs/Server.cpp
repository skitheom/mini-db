// Server.cpp
#include "Server.hpp"
#include "KVStore.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

Server::Server(int listen_fd, KVStore &kv)
    : listen_fd_(listen_fd), kv_(kv), clients_(), inbuf_(), outbuf_() {}

Server::~Server() noexcept {
  for (int fd : clients_) {
    ::close(fd);
  }
  inbuf_.clear();
  outbuf_.clear();
}

void Server::run() {

  while (g_stop == 0) {
    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    FD_SET(listen_fd_, &rfds);
    int maxfd = listen_fd_;

    for (int fd : clients_) {
      FD_SET(fd, &rfds);
      maxfd = std::max(maxfd, fd);
    }

    for (auto it = outbuf_.begin(); it != outbuf_.end(); it++) {
      if (!it->second.empty()) {
        FD_SET(it->first, &wfds);
        maxfd = std::max(maxfd, it->first);
      }
    }

    timeval timeout{0, 200000};

    errno = 0;
    int n = ::select(maxfd + 1, &rfds, &wfds, 0, &timeout);
    if (n == -1) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error("select");
    }

    if (g_stop)
      break;
    // server fd event
    if (FD_ISSET(listen_fd_, &rfds)) {
      accept_new_client();
    }

    // client fd event
    for (size_t i = 0; i < clients_.size();) {
      int fd = clients_[i];
      bool alive = true;
      if (FD_ISSET(fd, &wfds)) {
        alive = on_write(fd, i);
      }
      if (alive && FD_ISSET(fd, &rfds)) {
        alive = on_read(fd, i);
      }
      if (alive)
        ++i;
    }
  }
}

bool Server::on_write(int client_fd, int client_index) {
  std::string &outbuf = outbuf_[client_fd];
  if (outbuf.empty()) {
    return true;
  }
  ssize_t n = ::send(client_fd, outbuf.data(), outbuf.size(), 0);
  if (n == 0) {
    return true; // Q: retry or close_and_cleanup
  }
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return true; // retry
    }
    close_and_cleanup(client_fd, client_index);
    return false;
  }
  // n > 0
  outbuf.erase(0, n);
  return true;
}

bool Server::on_read(int client_fd, int client_index) {
  char tmp[2048];
  errno = 0;
  ssize_t n = ::recv(client_fd, tmp, sizeof(tmp), 0);
  if (n == 0) {
    close_and_cleanup(client_fd, client_index);
    return false; // EOF
  }
  if (n < 0) {
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      return true; // to retry
    }
    close_and_cleanup(client_fd, client_index);
    return false; // fatal
  }
  // n > 0
  std::string &inbuf = inbuf_[client_fd];
  std::string &outbuf = outbuf_[client_fd];

  inbuf.append(tmp, n);
  if (inbuf.size() > 1000 && inbuf.find('\n') == std::string::npos) {
    close_and_cleanup(client_fd, client_index);
    return false; // error
  }

  size_t pos;
  while ((pos = inbuf.find('\n')) != std::string::npos) {

    std::string line = inbuf.substr(0, pos);
    inbuf.erase(0, pos + 1);

    if (line.empty())
      continue;

    std::string response = kv_.handle_line(line);
    outbuf.append(response);
  }
  // if optimistic send is needed
  // if (!outbuf.empty())
  //   return on_write(client_fd, client_index);
  return true;
}

void Server::accept_new_client() {
  sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  int cfd = ::accept(listen_fd_, reinterpret_cast<sockaddr *>(&addr), &len);
  if (cfd == -1)
    return;
  int fl = fcntl(cfd, F_GETFL, 0);
  if (fl == -1 || fcntl(cfd, F_SETFL, fl | O_NONBLOCK) == -1) {
    ::close(cfd);
    return;
  }
  clients_.push_back(cfd);
}

void Server::close_and_cleanup(int client_fd, int client_index) {
  ::close(client_fd);
  inbuf_.erase(client_fd);
  outbuf_.erase(client_fd);
  clients_.erase(clients_.begin() + client_index);
}
