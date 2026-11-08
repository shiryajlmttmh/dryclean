#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "types.h"

class ShopException : public std::runtime_error {
public:
    explicit ShopException(const std::string& message);
    virtual std::string title() const;
};

class InvalidInputException : public ShopException {
public:
    InvalidInputException(std::string field_name, std::string message);
    const std::string& field_name() const noexcept;
    std::string title() const override;

private:
    std::string field_name_;
};

class FileFormatException : public ShopException {
public:
    FileFormatException(std::string file, std::size_t line, std::string message);
    const std::string& file() const noexcept;
    std::size_t line() const noexcept;
    std::string title() const override;

private:
    std::string file_;
    std::size_t line_;
};

class OutOfStockException : public ShopException {
public:
    OutOfStockException(std::string material_name, double required, double available);
    const std::string& material_name() const noexcept;
    double required() const noexcept;
    double available() const noexcept;
    std::string title() const override;

private:
    std::string material_name_;
    double required_;
    double available_;
};

class PlanningException : public ShopException {
public:
    explicit PlanningException(std::string message, Id item_id = 0);
    Id item_id() const noexcept;
    std::string title() const override;

private:
    Id item_id_;
};

class IntegrityException : public ShopException {
public:
    explicit IntegrityException(std::string message);
    std::string title() const override;
};

class StorageException : public ShopException {
public:
    StorageException(std::filesystem::path path, std::string message);
    const std::filesystem::path& path() const noexcept;
    std::string title() const override;

private:
    std::filesystem::path path_;
};
