#pragma once

#include <compare>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "types.h"

class Client {
public:
    Client() = default;
    Client(std::string name, std::string phone, double discount = 0);

    Id id() const noexcept;
    const std::string& name() const noexcept;
    const std::string& phone() const noexcept;
    double discount() const noexcept;
    const std::vector<Id>& order_ids() const noexcept;

    void set_id(Id id) noexcept;
    void set_name(std::string name);
    void set_phone(std::string phone);
    void set_discount(double discount);
    void add_order_id(Id order_id);
    void remove_order_id(Id order_id);

    std::size_t orders_count() const noexcept;
    bool matches(const std::string& query) const;
    void validate() const;

    bool operator==(const Client& other) const noexcept;
    std::strong_ordering operator<=>(const Client& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Client& client);
    friend std::istream& operator>>(std::istream& is, Client& client);

private:
    Id id_ = 0;
    std::string name_;
    std::string phone_;
    double discount_ = 0.0;
    std::vector<Id> order_ids_;
};
