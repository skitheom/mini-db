// KVStore.hpp
#pragma once
#include <string>
#include <unordered_map>

class KVStore {
public:
  KVStore() = default;

  void post(const std::string &k, const std::string &v);
  bool get(const std::string &k, std::string &out) const;
  bool erase(const std::string &k);

  using Map = std::unordered_map<std::string, std::string>;
  using ConstIt = Map::const_iterator;

  // for persist
  size_t size() const;
  ConstIt begin() const;
  ConstIt end() const;

  std::string handle_line(const std::string &line);

private:
  Map db_;
};
