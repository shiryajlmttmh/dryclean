#pragma once

#include <algorithm>
#include <fstream>
#include <istream>
#include <limits>
#include <locale>
#include <ostream>
#include <set>
#include <string>

#include "exceptions.h"
#include "record_io.h"

template <class T, class Pred>
std::vector<T> find_by_predicate(const std::vector<T>& items, Pred pred) {
    std::vector<T> result;
    std::copy_if(items.begin(), items.end(), std::back_inserter(result), pred);
    return result;
}

template <class T>
void save_collection(std::ostream& os, const std::vector<T>& items, Id next_id) {
    os << kFormatMagic << ' ' << kFormatVersion << ' ' << EntityName<T>::value << ' ' << items.size() << ' '
       << next_id << '\n';
    for (const T& item : items) {
        os << item << '\n';
    }
}

template <class T>
std::pair<std::vector<T>, Id> load_collection(std::istream& is) {
    std::string header;
    if (!std::getline(is, header)) {
        throw FileFormatException("", 1, "файл пуст");
    }
    std::size_t count = 0;
    Id next_id = 0;
    try {
        RecordReader reader(std::move(header));
        if (reader.get_token() != kFormatMagic) {
            throw InvalidInputException("header", "неизвестный формат файла");
        }
        if (reader.get_count() != kFormatVersion) {
            throw InvalidInputException("header", "неподдерживаемая версия формата");
        }
        if (reader.get_token() != EntityName<T>::value) {
            throw InvalidInputException("header", "файл содержит данные другого типа");
        }
        count = reader.get_count(kMaxRecords);
        next_id = reader.get_id();
        reader.finish();
    } catch (const InvalidInputException& error) {
        throw FileFormatException("", 1, error.what());
    }
    if (next_id == 0) {
        throw FileFormatException("", 1, "счётчик идентификаторов должен быть положительным");
    }

    std::vector<T> items;
    std::set<Id> seen;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t line = kFirstRecordLine + i;
        T item;
        if (!(is >> item)) {
            throw FileFormatException("", line, "запись отсутствует или повреждена");
        }
        if (item.id() == 0) {
            throw FileFormatException("", line, "у сохранённой записи нулевой идентификатор");
        }
        if (item.id() >= next_id) {
            throw FileFormatException("", line, "идентификатор записи не меньше счётчика идентификаторов");
        }
        if (!seen.insert(item.id()).second) {
            throw FileFormatException("", line, "повторяющийся идентификатор записи");
        }
        items.push_back(std::move(item));
    }

    std::string rest;
    while (std::getline(is, rest)) {
        if (rest.find_first_not_of(" \t\r\n") != std::string::npos) {
            throw FileFormatException("", kFirstRecordLine + count, "после последней записи есть лишние данные");
        }
    }
    return {std::move(items), next_id};
}

template <class T>
Id Repository<T>::next_id() const noexcept {
    return next_id_;
}

template <class T>
Id Repository<T>::add(T item) {
    if (item.id() != 0) {
        throw InvalidInputException("id", "новый элемент должен иметь нулевой идентификатор");
    }
    if (next_id_ == std::numeric_limits<Id>::max()) {
        throw InvalidInputException("id", "запас идентификаторов исчерпан");
    }
    if (items_.size() >= kMaxRecords) {
        throw InvalidInputException("count", "достигнут предел числа записей");
    }
    item.validate();
    const Id id = next_id_;
    item.set_id(id);
    items_.push_back(std::move(item));
    ++next_id_;
    return id;
}

template <class T>
void Repository<T>::remove(Id id) {
    const auto it = std::find_if(items_.begin(), items_.end(), [id](const T& item) { return item.id() == id; });
    if (it == items_.end()) {
        throw InvalidInputException("id", "элемент не найден");
    }
    items_.erase(it);
}

template <class T>
T* Repository<T>::find_by_id(Id id) noexcept {
    const auto it = std::find_if(items_.begin(), items_.end(), [id](const T& item) { return item.id() == id; });
    return it == items_.end() ? nullptr : &*it;
}

template <class T>
const T* Repository<T>::find_by_id(Id id) const noexcept {
    const auto it = std::find_if(items_.begin(), items_.end(), [id](const T& item) { return item.id() == id; });
    return it == items_.end() ? nullptr : &*it;
}

template <class T>
T& Repository<T>::get(Id id) {
    T* item = find_by_id(id);
    if (item == nullptr) {
        throw InvalidInputException("id", "элемент не найден");
    }
    return *item;
}

template <class T>
const T& Repository<T>::get(Id id) const {
    const T* item = find_by_id(id);
    if (item == nullptr) {
        throw InvalidInputException("id", "элемент не найден");
    }
    return *item;
}

template <class T>
bool Repository<T>::contains(Id id) const noexcept {
    return find_by_id(id) != nullptr;
}

template <class T>
std::size_t Repository<T>::size() const noexcept {
    return items_.size();
}

template <class T>
bool Repository<T>::empty() const noexcept {
    return items_.empty();
}

template <class T>
void Repository<T>::clear() noexcept {
    items_.clear();
    next_id_ = 1;
}

template <class T>
void Repository<T>::swap(Repository& other) noexcept {
    items_.swap(other.items_);
    std::swap(next_id_, other.next_id_);
}

template <class T>
typename Repository<T>::iterator Repository<T>::begin() noexcept {
    return items_.begin();
}

template <class T>
typename Repository<T>::iterator Repository<T>::end() noexcept {
    return items_.end();
}

template <class T>
typename Repository<T>::const_iterator Repository<T>::begin() const noexcept {
    return items_.begin();
}

template <class T>
typename Repository<T>::const_iterator Repository<T>::end() const noexcept {
    return items_.end();
}

template <class T>
typename Repository<T>::const_iterator Repository<T>::cbegin() const noexcept {
    return items_.cbegin();
}

template <class T>
typename Repository<T>::const_iterator Repository<T>::cend() const noexcept {
    return items_.cend();
}

template <class T>
template <class Pred>
std::vector<T> Repository<T>::find_if(Pred pred) const {
    return find_by_predicate(items_, pred);
}

template <class T>
template <class Cmp>
void Repository<T>::sort(Cmp cmp) {
    std::sort(items_.begin(), items_.end(), cmp);
}

template <class T>
void Repository<T>::save(const std::filesystem::path& path) const {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw StorageException(path, "не удалось открыть файл для записи");
    }
    out.imbue(std::locale::classic());
    save_collection(out, items_, next_id_);
    out.flush();
    if (!out) {
        throw StorageException(path, "ошибка записи в файл");
    }
    out.close();
    if (out.fail()) {
        throw StorageException(path, "не удалось закрыть файл после записи");
    }
}

template <class T>
void Repository<T>::load(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw StorageException(path, "не удалось открыть файл для чтения");
    }
    in.imbue(std::locale::classic());
    try {
        auto loaded = load_collection<T>(in);
        items_ = std::move(loaded.first);
        next_id_ = loaded.second;
    } catch (const FileFormatException& error) {
        if (in.bad()) {
            throw StorageException(path, "ошибка чтения файла");
        }
        throw FileFormatException(path.filename().string(), error.line(), error.what());
    }
}
