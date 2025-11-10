// includes/Socket.hpp
#pragma once

class Socket {
public:
  explicit Socket(int port);
  ~Socket() noexcept;
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  int fd() const;

private:
  int listen_fd_{-1};

  int make_listen(int port);
};
