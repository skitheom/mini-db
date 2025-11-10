#include "persist.hpp"
#include "KVStore.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace persist {

bool load_file(const std::string &path, KVStore &kv) {
  if (!std::filesystem::exists(path))
    return true;
  if (!std::filesystem::is_regular_file(path))
    return false;

  std::ifstream in(path.c_str());
  if (!in)
    return false;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line.size() > 1000)
      continue;

    std::istringstream iss(line);
    std::string k, v, extra;
    if (!(iss >> k) || !(iss >> v) || (iss >> extra))
      continue;
    kv.post(k, v);
  }
  return true;
}

void save_atomic(const std::string &path, const KVStore &kv) {
  const std::string tmp = path + ".tmp";
  std::ofstream out(tmp.c_str());
  if (!out)
    throw std::runtime_error("save failed");

  for (KVStore::ConstIt it = kv.begin(); it != kv.end(); ++it) {
    out << it->first << ' ' << it->second << '\n';
  }
  out.close();

  if (std::rename(tmp.c_str(), path.c_str()) != 0) {
    std::remove(tmp.c_str());
    throw std::runtime_error("save failed");
  }
  std::remove(tmp.c_str());
}
}; // namespace persist
