#pragma once

#include <string>

namespace password {

std::string hash(const std::string& value);
bool verify(const std::string& value, const std::string& encoded);
bool valid_hash(const std::string& encoded);

}
