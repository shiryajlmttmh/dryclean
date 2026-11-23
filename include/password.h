#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace password {

inline constexpr std::size_t kMinPasswordCharacters = 8;

void check_confirmation(std::string_view value, std::string_view confirmation);
void check_length(std::string_view value);
std::string hash(const std::string& value);
bool verify(const std::string& value, const std::string& encoded);
bool valid_hash(const std::string& encoded);

}
