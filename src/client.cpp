#include "client.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
#include <utility>

#include "exceptions.h"
#include "normalize.h"
#include "record_io.h"
#include "validation.h"

namespace {

std::string phone_query_digits(const std::string& query) {
    std::string digits;
    for (const char c : query) {
        if (c == ' ' || c == '(' || c == ')' || c == '-' || (c == '+' && digits.empty())) {
            continue;
        }
        if (c < '0' || c > '9') {
            return {};
        }
        digits.push_back(c);
    }
    return digits;
}

}

Client::Client(std::string name, std::string phone, double discount) {
    set_name(std::move(name));
    set_phone(std::move(phone));
    set_discount(discount);
}

Id Client::id() const noexcept { return id_; }

const std::string& Client::name() const noexcept { return name_; }

const std::string& Client::phone() const noexcept { return phone_; }

double Client::discount() const noexcept { return discount_; }

const std::vector<Id>& Client::order_ids() const noexcept { return order_ids_; }

void Client::set_id(Id id) noexcept { id_ = id; }

void Client::set_name(std::string name) {
    name = normalize_text(std::move(name));
    require_text("name", name);
    name_ = std::move(name);
}

void Client::set_phone(std::string phone) { phone_ = normalize_phone(std::move(phone)); }

void Client::set_discount(double discount) {
    check_discount(discount);
    discount_ = discount;
}

void Client::add_order_id(Id order_id) {
    check_nonzero_id("order_id", order_id);
    if (std::find(order_ids_.begin(), order_ids_.end(), order_id) != order_ids_.end()) {
        throw InvalidInputException("order_id", "заказ уже привязан к клиенту");
    }
    order_ids_.push_back(order_id);
}

std::size_t Client::orders_count() const noexcept { return order_ids_.size(); }

bool Client::matches(const std::string& query) const {
    if (query.empty() || to_lower_text(name_).find(to_lower_text(query)) != std::string::npos) {
        return true;
    }
    const std::string digits = phone_query_digits(query);
    return !digits.empty() && phone_.find(digits) != std::string::npos;
}

void Client::validate() const {
    require_text("name", name_);
    if (normalize_phone(phone_) != phone_) {
        throw InvalidInputException("phone", "телефон должен храниться в нормализованном виде");
    }
    check_discount(discount_);
    check_unique_ids("order_ids", order_ids_);
}

bool Client::operator==(const Client& other) const noexcept { return id_ == other.id_; }

std::strong_ordering Client::operator<=>(const Client& other) const noexcept { return id_ <=> other.id_; }

std::ostream& operator<<(std::ostream& os, const Client& client) {
    RecordWriter writer;
    writer.put_id(client.id_);
    writer.put_string(client.name_);
    writer.put_string(client.phone_);
    writer.put_double(client.discount_);
    writer.put_count(client.order_ids_.size());
    for (const Id order_id : client.order_ids_) {
        writer.put_id(order_id);
    }
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, Client& client) {
    return read_record(is, client, [](Client& value, RecordReader& reader) {
        value.id_ = reader.get_id();
        value.name_ = reader.get_string();
        value.phone_ = reader.get_string();
        value.discount_ = reader.get_double();
        const std::size_t count = reader.get_count();
        for (std::size_t i = 0; i < count; ++i) {
            value.order_ids_.push_back(reader.get_id());
        }
    });
}
