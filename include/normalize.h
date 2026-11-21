#pragma once

#include <cstddef>
#include <string>
#include <string_view>

constexpr std::size_t kMaxStringBytes = 4096;

std::string normalize_phone(std::string_view phone);
std::string format_phone(std::string_view phone);
std::string normalize_login(std::string login);
std::string normalize_text(std::string text);
std::string to_lower_text(const std::string& text);
void require_text(const char* field_name, const std::string& value);
