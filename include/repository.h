#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <utility>
#include <vector>

#include "entity_names.h"
#include "types.h"

constexpr std::size_t kMaxRecords = 100000;
constexpr std::size_t kFormatVersion = 1;
constexpr std::size_t kHeaderLine = 1;
constexpr std::size_t kFirstRecordLine = kHeaderLine + 1;
inline constexpr const char* kFormatMagic = "DCLEAN";

template <class T, class Pred>
std::vector<T> find_by_predicate(const std::vector<T>& items, Pred pred);

template <class T>
void save_collection(std::ostream& os, const std::vector<T>& items, Id next_id);

template <class T>
std::pair<std::vector<T>, Id> load_collection(std::istream& is);

template <class T>
class Repository {
public:
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    Id next_id() const noexcept;
    Id add(T item);
    void remove(Id id);

    T* find_by_id(Id id) noexcept;
    const T* find_by_id(Id id) const noexcept;
    T& get(Id id);
    const T& get(Id id) const;
    bool contains(Id id) const noexcept;

    std::size_t size() const noexcept;
    bool empty() const noexcept;
    void clear() noexcept;
    void swap(Repository& other) noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    template <class Pred>
    std::vector<T> find_if(Pred pred) const;

    template <class Cmp>
    void sort(Cmp cmp);

    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);

private:
    std::vector<T> items_;
    Id next_id_ = 1;
};

#include "repository.tpp"
