#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

using Generation = std::uint64_t;

class Storage {
public:
    explicit Storage(std::filesystem::path root);
    ~Storage();

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    const std::filesystem::path& root() const noexcept;
    std::optional<Generation> current_generation() const;
    std::filesystem::path generation_path(Generation generation) const;

    Generation begin_generation();
    void commit_generation(Generation generation);
    void abort_generation(Generation generation) noexcept;

private:
    struct Lock;

    std::optional<Generation> find_latest_generation() const;
    void remove_uncommitted();
    void prune_old(Generation newest) noexcept;

    std::filesystem::path root_;
    std::unique_ptr<Lock> lock_;
};
