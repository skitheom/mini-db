// main.cpp
#include "KVStore.hpp"
#include "Server.hpp"
#include "Socket.hpp"
#include "persist.hpp"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

volatile sig_atomic_t g_stop = 0;

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

  KVStore kv;
  int port = parse_port(argv[1]);
  std::string save_path = argv[2];

  if (port < 0 || !persist::load_file(save_path, kv)) {
    return 1;
  }

  // install_sigint_flag_handler();
  ::signal(SIGPIPE, SIG_IGN);
  ::signal(SIGINT, on_sigint);

  try {
    Socket sock(port);
    std::cout << "ready\n" << std::flush;
    Server server(sock.fd(), kv);
    server.run();
    persist::save_atomic(save_path, kv);
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
