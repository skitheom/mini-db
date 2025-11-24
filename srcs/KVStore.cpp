// KVStore.cpp
#include "KVStore.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

KVStore::KVStore() {}

KVStore::~KVStore() {}

void KVStore::load_file(const std::string &path) {
  std::ifstream in(path.c_str());
  if (!in)
    return;

  std::string k, v;
  while (in >> k >> v) {
    post(k, v);
  }
}

void KVStore::save_file(const std::string &path) {
  std::ofstream out(path.c_str());
  if (!out) {
    return;
  }
  for (auto it = _db.begin(); it != _db.end(); ++it) {
    out << it->first << ' ' << it->second << '\n';
  }
}

std::string KVStore::process_line(const std::string &line) {

  std::istringstream iss(line);
  std::string op, k, v, extra;

  if (!(iss >> op)) {
    return ""; // ignore empty request
  }
  if (op == "POST" && (iss >> k >> v) && !(iss >> extra)) {
    post(k, v);
    return "0\n";
  }
  if (op == "GET" && (iss >> k) && !(iss >> extra)) {
    std::string out;
    return get(k, out) ? "0 " + out + "\n" : "1\n";
  }
  if (op == "DELETE" && (iss >> k) && !(iss >> extra)) {
    return erase(k) ? "0\n" : "1\n";
  }
  return "2\n";
}

void KVStore::post(const std::string &k, const std::string &v) {
  if (k.empty() || v.empty()) {
    return;
  }
  _db[k] = v;
}

bool KVStore::get(const std::string &k, std::string &out) const {
  auto it = _db.find(k);
  if (it == _db.end()) {
    return false;
  }
  out = it->second;
  return true;
}

bool KVStore::erase(const std::string &k) { return (_db.erase(k) > 0); }
