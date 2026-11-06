#include "validation.h"

#include <algorithm>
#include <cmath>

#include "exceptions.h"

namespace {

constexpr double kMinDiscount = 0.0;
constexpr double kMaxDiscount = 1.0;

}

void check_nonzero_id(const char* field_name, Id id) {
    if (id == 0) {
        throw InvalidInputException(field_name, "идентификатор не может быть нулевым");
    }
}

void check_unique_ids(const char* field_name, const std::vector<Id>& ids) {
    std::vector<Id> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    if (!sorted.empty()) {
        check_nonzero_id(field_name, sorted.front());
    }
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
        throw InvalidInputException(field_name, "список содержит повторы");
    }
}

void check_discount(double discount) {
    if (!std::isfinite(discount) || discount < kMinDiscount || discount > kMaxDiscount) {
        throw InvalidInputException("discount", "скидка должна быть в пределах от 0 до 1");
    }
}
