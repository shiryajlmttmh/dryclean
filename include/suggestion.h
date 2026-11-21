#pragma once

#include <iosfwd>
#include <string>

#include "types.h"

enum class SuggestionKind { ItemName, DefectName, MaterialName, Unit, MachineName, ClientName, EmployeeName };

class Suggestion {
public:
    Suggestion() = default;
    Suggestion(SuggestionKind kind, std::string value);

    Id id() const noexcept { return id_; }
    void set_id(Id id) noexcept { id_ = id; }
    SuggestionKind kind() const noexcept { return kind_; }
    const std::string& value() const noexcept { return value_; }
    void validate() const;

    friend std::ostream& operator<<(std::ostream& output, const Suggestion& suggestion);
    friend std::istream& operator>>(std::istream& input, Suggestion& suggestion);

private:
    Id id_ = 0;
    SuggestionKind kind_ = SuggestionKind::ItemName;
    std::string value_;
};
