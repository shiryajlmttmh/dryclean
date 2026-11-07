#pragma once

#include "batch.h"
#include "client.h"
#include "employee.h"
#include "item.h"
#include "machine.h"
#include "material.h"
#include "order.h"

template <class T>
struct EntityName;

template <>
struct EntityName<Client> {
    static constexpr const char* value = "clients";
};

template <>
struct EntityName<Item> {
    static constexpr const char* value = "items";
};

template <>
struct EntityName<Order> {
    static constexpr const char* value = "orders";
};

template <>
struct EntityName<Batch> {
    static constexpr const char* value = "batches";
};

template <>
struct EntityName<Material> {
    static constexpr const char* value = "materials";
};

template <>
struct EntityName<Employee> {
    static constexpr const char* value = "employees";
};

template <>
struct EntityName<Machine> {
    static constexpr const char* value = "machines";
};
