#include "material.h"

#include <cmath>
#include <istream>
#include <ostream>
#include <utility>

#include "exceptions.h"
#include "normalize.h"
#include "record_io.h"

namespace {

void check_stock(double stock) {
    if (!std::isfinite(stock) || stock < 0.0) {
        throw InvalidInputException("stock", "остаток не может быть отрицательным");
    }
}

void check_consumption(double consumption_per_kg) {
    if (!std::isfinite(consumption_per_kg) || consumption_per_kg <= 0.0) {
        throw InvalidInputException("consumption_per_kg", "норма расхода должна быть больше нуля");
    }
}

void check_services(const std::set<ServiceType>& services) {
    if (services.empty()) {
        throw InvalidInputException("services", "нужно указать хотя бы одну услугу");
    }
    for (const ServiceType service : services) {
        if (!is_valid(service)) {
            throw InvalidInputException("services", "неизвестный вид услуги");
        }
    }
}

}

Material::Material(std::string name, std::string unit, double stock, double consumption_per_kg,
                   std::set<ServiceType> services)
    : unit_(normalize_text(std::move(unit))), stock_(stock) {
    set_name(std::move(name));
    set_consumption_per_kg(consumption_per_kg);
    set_services(std::move(services));
    validate();
}

Id Material::id() const noexcept { return id_; }

const std::string& Material::name() const noexcept { return name_; }

const std::string& Material::unit() const noexcept { return unit_; }

double Material::stock() const noexcept { return stock_; }

double Material::consumption_per_kg() const noexcept { return consumption_per_kg_; }

const std::set<ServiceType>& Material::services() const noexcept { return services_; }

void Material::set_id(Id id) noexcept { id_ = id; }

void Material::set_name(std::string name) {
    name = normalize_text(std::move(name));
    require_text("name", name);
    name_ = std::move(name);
}

void Material::set_consumption_per_kg(double consumption_per_kg) {
    check_consumption(consumption_per_kg);
    consumption_per_kg_ = consumption_per_kg;
}

void Material::set_services(std::set<ServiceType> services) {
    check_services(services);
    services_ = std::move(services);
}

void Material::add_stock(double amount) {
    if (!std::isfinite(amount) || amount <= 0.0) {
        throw InvalidInputException("amount", "количество пополнения должно быть больше нуля");
    }
    stock_ += amount;
}

void Material::write_off(double amount) {
    if (!std::isfinite(amount) || amount < 0.0) {
        throw InvalidInputException("amount", "количество списания не может быть отрицательным");
    }
    if (amount > stock_ + EPS) {
        throw OutOfStockException(name_, amount, stock_);
    }
    stock_ -= amount;
    if (stock_ < EPS) {
        stock_ = 0.0;
    }
}

double Material::required_for(double weight_kg) const {
    if (!std::isfinite(weight_kg) || weight_kg <= 0.0) {
        throw InvalidInputException("weight_kg", "масса должна быть больше нуля");
    }
    return consumption_per_kg_ * weight_kg;
}

bool Material::is_used_for(ServiceType service) const noexcept { return services_.contains(service); }

void Material::validate() const {
    require_text("name", name_);
    require_text("unit", unit_);
    check_stock(stock_);
    check_consumption(consumption_per_kg_);
    check_services(services_);
}

bool Material::operator==(const Material& other) const noexcept { return id_ == other.id_; }

std::strong_ordering Material::operator<=>(const Material& other) const noexcept { return id_ <=> other.id_; }

std::ostream& operator<<(std::ostream& os, const Material& material) {
    RecordWriter writer;
    writer.put_id(material.id_);
    writer.put_string(material.name_);
    writer.put_string(material.unit_);
    writer.put_double(material.stock_);
    writer.put_double(material.consumption_per_kg_);
    writer.put_count(material.services_.size());
    for (const ServiceType service : material.services_) {
        writer.put_token(to_string(service));
    }
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, Material& material) {
    return read_record(is, material, [](Material& value, RecordReader& reader) {
        value.id_ = reader.get_id();
        value.name_ = reader.get_string();
        value.unit_ = reader.get_string();
        value.stock_ = reader.get_double();
        value.consumption_per_kg_ = reader.get_double();
        const std::size_t count = reader.get_count();
        for (std::size_t i = 0; i < count; ++i) {
            if (!value.services_.insert(parse_ServiceType(reader.get_token())).second) {
                throw InvalidInputException("services", "список услуг содержит повторы");
            }
        }
    });
}
