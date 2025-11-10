// persist.hpp
#pragma once
#include <string>

class KVStore;

namespace persist {
bool load_file(const std::string &path, KVStore &kv);
void save_atomic(const std::string &path, const KVStore &kv);
} // namespace persist
