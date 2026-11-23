#pragma once

#include <cstddef>
#include <string>

namespace password {

inline constexpr std::size_t kMinPasswordCharacters = 8;

std::string hash(const std::string& value);
bool verify(const std::string& value, const std::string& encoded);
bool valid_hash(const std::string& encoded);

}
