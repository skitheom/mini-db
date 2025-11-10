// KVStore.cpp
#include "KVStore.hpp"
#include <sstream>

void KVStore::post(const std::string &k, const std::string &v) { db_[k] = v; }

bool KVStore::get(const std::string &k, std::string &out) const {
  auto it = db_.find(k);
  if (it == db_.end()) {
    return false;
  }
  out = it->second;
  return true;
}

bool KVStore::erase(const std::string &k) { return db_.erase(k) > 0; }

size_t KVStore::size() const { return db_.size(); }

KVStore::ConstIt KVStore::begin() const { return db_.begin(); }

KVStore::ConstIt KVStore::end() const { return db_.end(); }

std::string KVStore::handle_line(const std::string &line) {

  std::istringstream iss(line);
  std::string op, k, v;

  iss >> op >> k >> v >> std::ws;

  if (op.empty()) {
    return ""; // ignore empty request
  }
  if (iss.peek() != std::char_traits<char>::eof()) {
    return "2\n"; // or "" ? unclear
  }
  if (op == "POST" && !k.empty() && !v.empty()) {
    post(k, v);
    return "0\n";
  }
  if (op == "GET" && !k.empty() && v.empty()) {
    std::string out;
    return get(k, out) ? "0 " + out + "\n" : "1\n";
  }
  if (op == "DELETE" && !k.empty() && v.empty()) {
    return erase(k) ? "0\n" : "1\n";
  }
  return "2\n";
}
