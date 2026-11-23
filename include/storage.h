#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

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
    bool has_data() const;
    std::vector<Generation> recovery_generations() const;
    void discard_uncommitted(std::optional<Generation> current);

    Generation begin_generation();
    void commit_generation(Generation generation, std::optional<Generation> previous);
    void abort_generation(Generation generation) noexcept;
    std::filesystem::path archive_damage(Generation source, Generation replacement) const;
    void copy_generation(Generation source, Generation replacement) const;

private:
    struct Lock;

    void prune_old(Generation newest, std::optional<Generation> previous) noexcept;

    std::filesystem::path root_;
    std::unique_ptr<Lock> lock_;
};
