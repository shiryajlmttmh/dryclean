#include "suggestion.h"

#include <array>
#include <utility>

#include "normalize.h"
#include "record_io.h"

namespace {

constexpr std::array kKinds{"item_name", "defect_name", "material_name", "unit", "machine_name",
                            "client_name", "employee_name"};

}

Suggestion::Suggestion(SuggestionKind kind, std::string value) : kind_(kind), value_(std::move(value)) {}

void Suggestion::validate() const {
    if (static_cast<std::size_t>(kind_) >= kKinds.size()) {
        throw InvalidInputException("suggestion", "неизвестный справочник подсказок");
    }
    require_text("suggestion", value_);
    if (normalize_text(value_) != value_) {
        throw InvalidInputException("suggestion", "подсказка должна храниться в нормализованном виде");
    }
}

std::ostream& operator<<(std::ostream& output, const Suggestion& suggestion) {
    RecordWriter writer;
    writer.put_id(suggestion.id_);
    writer.put_token(kKinds.at(static_cast<std::size_t>(suggestion.kind_)));
    writer.put_string(suggestion.value_);
    return output << writer.str();
}

std::istream& operator>>(std::istream& input, Suggestion& suggestion) {
    return read_record(input, suggestion, [](Suggestion& value, RecordReader& reader) {
        value.id_ = reader.get_id();
        const auto token = reader.get_token();
        for (std::size_t index = 0; index < kKinds.size(); ++index) {
            if (token == kKinds[index]) {
                value.kind_ = static_cast<SuggestionKind>(index);
                value.value_ = reader.get_string();
                return;
            }
        }
        throw InvalidInputException("suggestion", "неизвестный справочник подсказок");
    });
}
