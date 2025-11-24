// KVStore.hpp
#pragma once
#include <map>
#include <string>

class KVStore {
public:
  KVStore();
  ~KVStore();

  void load_file(const std::string &path);
  void save_file(const std::string &path);

  std::string process_line(const std::string &line);

  using Map = std::map<std::string, std::string>;
  using ConstIt = Map::const_iterator;

private:
  Map _db;

  void post(const std::string &k, const std::string &v);
  bool get(const std::string &k, std::string &out) const;
  bool erase(const std::string &k);
};
