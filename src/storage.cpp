#include "storage.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "exceptions.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr const char* kCurrentName = "CURRENT";
constexpr const char* kCurrentTempName = "CURRENT.tmp";
constexpr const char* kPreviousName = "PREVIOUS";
constexpr const char* kPreviousTempName = "PREVIOUS.tmp";
constexpr const char* kRecoveryPrefix = "recovery-";
constexpr const char* kLockName = "LOCK";
constexpr const char* kGenerationPrefix = "gen-";
constexpr Generation kKeptGenerations = 3;
constexpr std::size_t kMaxGenerationDigits = 18;
constexpr std::size_t kCurrentLine = 1;
constexpr int kNoDescriptor = -1;

std::optional<Generation> parse_generation_name(const std::string& name) {
    const std::string prefix = kGenerationPrefix;
    if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }
    const std::string digits = name.substr(prefix.size());
    if (digits.size() > kMaxGenerationDigits) {
        return std::nullopt;
    }
    Generation value = 0;
    const char* end = digits.data() + digits.size();
    const auto result = std::from_chars(digits.data(), end, value);
    if (result.ec != std::errc() || result.ptr != end || value == 0 || digits != std::to_string(value)) {
        return std::nullopt;
    }
    return value;
}

std::string generation_name(Generation generation) { return kGenerationPrefix + std::to_string(generation); }

std::vector<Generation> list_generations(const fs::path& root) {
    std::vector<Generation> result;
    std::error_code ec;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code type_ec;
        const auto generation = parse_generation_name(it->path().filename().string());
        if (!generation) continue;
        if (it->is_directory(type_ec)) result.push_back(*generation);
        if (type_ec) throw StorageException(it->path(), "не удалось проверить каталог снимка");
    }
    if (ec) throw StorageException(root, "не удалось прочитать список снимков");
    std::ranges::sort(result, std::greater<Generation>{});
    return result;
}

void replace_file(const fs::path& from, const fs::path& to) {
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw StorageException(to, "не удалось атомарно заменить файл");
    }
#else
    std::error_code ec;
    fs::rename(from, to, ec);
    if (ec) {
        throw StorageException(to, "не удалось атомарно заменить файл");
    }
#endif
}

std::optional<Generation> read_generation_pointer(const fs::path& path) {
    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    if (ec) throw StorageException(path, "не удалось проверить указатель снимка");
    if (!exists) return std::nullopt;
    if (!fs::is_regular_file(path, ec)) {
        if (ec) throw StorageException(path, "не удалось проверить указатель снимка");
        throw FileFormatException(path.filename().string(), kCurrentLine, "указатель снимка повреждён");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) throw StorageException(path, "не удалось прочитать указатель снимка");
    std::string text;
    std::getline(in, text);
    while (!text.empty() && (text.back() == '\r' || text.back() == ' ')) text.pop_back();
    std::string rest;
    while (std::getline(in, rest)) {
        if (rest.find_first_not_of(" \t\r\n") != std::string::npos) {
            throw FileFormatException(path.filename().string(), kCurrentLine, "указатель снимка содержит лишние данные");
        }
    }
    if (in.bad()) throw StorageException(path, "ошибка чтения указателя снимка");
    const auto generation = parse_generation_name(std::string(kGenerationPrefix) + text);
    if (!generation) throw FileFormatException(path.filename().string(), kCurrentLine, "указатель снимка повреждён");
    return generation;
}

void write_generation_pointer(const fs::path& temporary, const fs::path& path, Generation generation) {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) throw StorageException(temporary, "не удалось открыть файл для записи");
    out << generation << '\n';
    out.close();
    if (!out) throw StorageException(temporary, "ошибка записи указателя снимка");
    replace_file(temporary, path);
}

void copy_data(const fs::path& source, const fs::path& destination) {
    std::error_code ec;
    fs::copy(source, destination, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
    if (ec) throw StorageException(destination, "не удалось скопировать файлы снимка");
}

}

struct Storage::Lock {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;

    explicit Lock(const fs::path& path) {
        handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            throw StorageException(path, error == ERROR_SHARING_VIOLATION
                ? "хранилище уже используется другим экземпляром программы" : "не удалось открыть файл блокировки");
        }
    }

    ~Lock() {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
#else
    int descriptor = kNoDescriptor;

    explicit Lock(const fs::path& path) {
        descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if (descriptor < 0) {
            throw StorageException(path, "не удалось открыть файл блокировки");
        }
        if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
            ::close(descriptor);
            descriptor = kNoDescriptor;
            throw StorageException(path, "хранилище уже используется другим экземпляром программы");
        }
    }

    ~Lock() {
        if (descriptor >= 0) {
            ::close(descriptor);
        }
    }
#endif
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
};

Storage::Storage(fs::path root) : root_(std::move(root)) {
    std::error_code ec;
    fs::create_directories(root_, ec);
    if (ec) {
        throw StorageException(root_, "не удалось создать каталог хранилища");
    }
    lock_ = std::make_unique<Lock>(root_ / kLockName);
}

Storage::~Storage() = default;

const fs::path& Storage::root() const noexcept { return root_; }

fs::path Storage::generation_path(Generation generation) const { return root_ / generation_name(generation); }

bool Storage::has_data() const {
    std::error_code ec;
    for (fs::directory_iterator it(root_, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().filename().string() != kLockName) {
            return true;
        }
    }
    if (ec) throw StorageException(root_, "не удалось прочитать каталог хранилища");
    return false;
}

std::optional<Generation> Storage::current_generation() const {
    return read_generation_pointer(root_ / kCurrentName);
}

std::vector<Generation> Storage::recovery_generations() const {
    std::optional<Generation> current;
    std::optional<Generation> previous;
    try {
        current = current_generation();
    } catch (const FileFormatException&) {
    }
    try {
        previous = read_generation_pointer(root_ / kPreviousName);
    } catch (const FileFormatException&) {
    }
    auto generations = list_generations(root_);
    if (!current && !previous && !generations.empty()) current = generations.front();
    std::erase_if(generations, [&](Generation generation) {
        return (current && generation >= *current) || (previous && generation > *previous);
    });
    return generations;
}

void Storage::discard_uncommitted(std::optional<Generation> current) {
    for (const Generation generation : list_generations(root_)) {
        if (current && generation <= *current) break;
        std::error_code ec;
        fs::remove_all(generation_path(generation), ec);
        if (ec) throw StorageException(generation_path(generation), "не удалось очистить незавершённый снимок");
    }
}

Generation Storage::begin_generation() {
    const auto generations = list_generations(root_);
    const Generation next = generations.empty() ? 1 : generations.front() + 1;
    if (std::to_string(next).size() > kMaxGenerationDigits) {
        throw StorageException(root_, "исчерпаны номера снимков");
    }
    std::error_code ec;
    fs::create_directory(generation_path(next), ec);
    if (ec) {
        throw StorageException(generation_path(next), "не удалось создать каталог поколения");
    }
    return next;
}

void Storage::commit_generation(Generation generation, std::optional<Generation> previous) {
    if (previous) write_generation_pointer(root_ / kPreviousTempName, root_ / kPreviousName, *previous);
    write_generation_pointer(root_ / kCurrentTempName, root_ / kCurrentName, generation);
    prune_old(generation, previous);
}

void Storage::abort_generation(Generation generation) noexcept {
    std::error_code ec;
    fs::remove_all(generation_path(generation), ec);
}

fs::path Storage::archive_damage(Generation source, Generation replacement) const {
    auto archive = root_ / (kRecoveryPrefix + std::to_string(replacement));
    std::error_code ec;
    while (!fs::create_directory(archive, ec)) {
        if (ec) throw StorageException(archive, "не удалось создать каталог диагностики");
        archive += "-copy";
    }
    const auto pointer = root_ / kCurrentName;
    if (fs::exists(pointer, ec)) copy_data(pointer, archive / kCurrentName);
    if (ec) throw StorageException(pointer, "не удалось проверить указатель снимка");
    for (Generation generation : list_generations(root_)) {
        if (generation > source && generation < replacement) {
            copy_data(generation_path(generation), archive / generation_name(generation));
        }
    }
    return archive;
}

void Storage::copy_generation(Generation source, Generation replacement) const {
    copy_data(generation_path(source), generation_path(replacement));
}

void Storage::prune_old(Generation newest, std::optional<Generation> previous) noexcept {
    try {
        Generation kept = 1;
        for (const Generation generation : list_generations(root_)) {
            if (generation >= newest) continue;
            if (previous && generation <= *previous && kept < kKeptGenerations) {
                ++kept;
                continue;
            }
            std::error_code ec;
            fs::remove_all(generation_path(generation), ec);
        }
    } catch (const StorageException&) {
    }
}
