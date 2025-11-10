// Server.hpp
#pragma once
#include <csignal>
#include <string>
#include <unordered_map>
#include <vector>

extern volatile sig_atomic_t g_stop;

class KVStore;

class Server {
public:
  Server(int listen_fd, KVStore &kv);
  ~Server() noexcept;
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  void run();

private:
  int listen_fd_;
  KVStore &kv_;

  std::vector<int> clients_;
  std::unordered_map<int, std::string> inbuf_;
  std::unordered_map<int, std::string> outbuf_;

  void accept_new_client();
  void close_and_cleanup(int client_fd, int client_index);
  bool on_write(int client_fd, int client_index);
  bool on_read(int client_fd, int client_index);
};
