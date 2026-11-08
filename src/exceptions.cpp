#include "exceptions.h"

#include <utility>

ShopException::ShopException(const std::string& message) : std::runtime_error(message) {}

std::string ShopException::title() const { return "Ошибка"; }

InvalidInputException::InvalidInputException(std::string field_name, std::string message)
    : ShopException(message), field_name_(std::move(field_name)) {}

const std::string& InvalidInputException::field_name() const noexcept { return field_name_; }

std::string InvalidInputException::title() const { return "Некорректные данные"; }

FileFormatException::FileFormatException(std::string file, std::size_t line, std::string message)
    : ShopException(message), file_(std::move(file)), line_(line) {}

const std::string& FileFormatException::file() const noexcept { return file_; }

std::size_t FileFormatException::line() const noexcept { return line_; }

std::string FileFormatException::title() const { return "Ошибка формата файла"; }

OutOfStockException::OutOfStockException(std::string material_name, double required, double available)
    : ShopException("Недостаточно материала «" + material_name + "»: требуется " + std::to_string(required) +
                    ", доступно " + std::to_string(available)),
      material_name_(std::move(material_name)),
      required_(required),
      available_(available) {}

const std::string& OutOfStockException::material_name() const noexcept { return material_name_; }

double OutOfStockException::required() const noexcept { return required_; }

double OutOfStockException::available() const noexcept { return available_; }

std::string OutOfStockException::title() const { return "Недостаточно материала"; }

PlanningException::PlanningException(std::string message, Id item_id)
    : ShopException(message), item_id_(item_id) {}

Id PlanningException::item_id() const noexcept { return item_id_; }

std::string PlanningException::title() const { return "Ошибка планирования"; }

StorageException::StorageException(std::filesystem::path path, std::string message)
    : ShopException(message), path_(std::move(path)) {}

const std::filesystem::path& StorageException::path() const noexcept { return path_; }

std::string StorageException::title() const { return "Ошибка хранилища"; }

IntegrityException::IntegrityException(std::string message) : ShopException(message) {}

std::string IntegrityException::title() const { return "Нарушена целостность данных"; }
