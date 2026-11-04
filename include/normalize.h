#pragma once

#include <cstddef>
#include <string>

constexpr std::size_t kMaxStringBytes = 4096;

std::string normalize_phone(std::string phone);
std::string normalize_login(std::string login);
std::string normalize_text(std::string text);
void require_text(const char* field_name, const std::string& value);
