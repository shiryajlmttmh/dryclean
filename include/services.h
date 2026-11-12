#pragma once

#include <memory>
#include <string>

#include "item.h"
#include "types.h"

class Service {
public:
    virtual ~Service() = default;

    virtual ServiceType type() const noexcept = 0;
    virtual std::string name() const = 0;
    virtual bool is_applicable(const Item& item) const = 0;
    virtual double calculate_cost(const Item& item, bool urgent, double client_discount) const = 0;
    virtual Hours calculate_duration(const Item& item, bool urgent) const = 0;

    Hours calculate_processing(const Item& item, bool urgent) const;

    static double round_money(double amount);

protected:
    double defects_surcharge(const Item& item) const;
    void require_applicable(const Item& item) const;
    double compute_cost(const Item& item, bool urgent, double client_discount, double soil_factor) const;
    Hours compute_duration(const Item& item, bool urgent) const;
};

class DryCleaningService : public Service {
public:
    ServiceType type() const noexcept override;
    std::string name() const override;
    bool is_applicable(const Item& item) const override;
    double calculate_cost(const Item& item, bool urgent, double client_discount) const override;
    Hours calculate_duration(const Item& item, bool urgent) const override;
};

class WetCleaningService : public Service {
public:
    ServiceType type() const noexcept override;
    std::string name() const override;
    bool is_applicable(const Item& item) const override;
    double calculate_cost(const Item& item, bool urgent, double client_discount) const override;
    Hours calculate_duration(const Item& item, bool urgent) const override;
};

class StainRemovalService : public Service {
public:
    ServiceType type() const noexcept override;
    std::string name() const override;
    bool is_applicable(const Item& item) const override;
    double calculate_cost(const Item& item, bool urgent, double client_discount) const override;
    Hours calculate_duration(const Item& item, bool urgent) const override;
};

class LeatherService : public Service {
public:
    ServiceType type() const noexcept override;
    std::string name() const override;
    bool is_applicable(const Item& item) const override;
    double calculate_cost(const Item& item, bool urgent, double client_discount) const override;
    Hours calculate_duration(const Item& item, bool urgent) const override;
};

std::unique_ptr<Service> make_service(ServiceType type);
