#include "storage.h"

#include <charconv>
#include <fstream>
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
constexpr const char* kLockName = "LOCK";
constexpr const char* kGenerationPrefix = "gen-";
constexpr Generation kKeptGenerations = 3;
constexpr std::size_t kMaxGenerationDigits = 18;
constexpr std::size_t kCurrentLine = 1;

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
    if (result.ec != std::errc() || result.ptr != end) {
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
        if (!it->is_directory(type_ec)) {
            continue;
        }
        if (const auto generation = parse_generation_name(it->path().filename().string())) {
            result.push_back(*generation);
        }
    }
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

}

struct Storage::Lock {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;

    explicit Lock(const fs::path& path) {
        handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            throw StorageException(path, "хранилище уже используется другим экземпляром программы");
        }
    }

    ~Lock() {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
#else
    int descriptor = -1;

    explicit Lock(const fs::path& path) {
        descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if (descriptor < 0) {
            throw StorageException(path, "не удалось открыть файл блокировки");
        }
        if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
            ::close(descriptor);
            descriptor = -1;
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
    current_generation();
    remove_uncommitted();
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
    return false;
}

std::optional<Generation> Storage::current_generation() const {
    const fs::path path = root_ / kCurrentName;
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return std::nullopt;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw StorageException(path, "не удалось прочитать указатель CURRENT");
    }
    std::string text;
    std::getline(in, text);
    while (!text.empty() && (text.back() == '\r' || text.back() == ' ')) {
        text.pop_back();
    }
    Generation value = 0;
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    if (text.empty() || text.size() > kMaxGenerationDigits || result.ec != std::errc() || result.ptr != end) {
        throw FileFormatException(kCurrentName, kCurrentLine, "указатель CURRENT повреждён");
    }
    if (!fs::is_directory(generation_path(value), ec)) {
        throw FileFormatException(kCurrentName, kCurrentLine, "указатель CURRENT ссылается на отсутствующее поколение");
    }
    return value;
}

std::optional<Generation> Storage::find_latest_generation() const {
    std::optional<Generation> latest;
    for (const Generation generation : list_generations(root_)) {
        if (!latest || generation > *latest) {
            latest = generation;
        }
    }
    return latest;
}

void Storage::remove_uncommitted() {
    std::error_code ec;
    fs::remove(root_ / kCurrentTempName, ec);
    const std::optional<Generation> current = current_generation();
    if (!current) {
        return;
    }
    for (const Generation generation : list_generations(root_)) {
        if (generation > *current) {
            fs::remove_all(generation_path(generation), ec);
        }
    }
}

Generation Storage::begin_generation() {
    const auto latest = find_latest_generation();
    const Generation next = latest ? *latest + 1 : 1;
    std::error_code ec;
    fs::create_directory(generation_path(next), ec);
    if (ec) {
        throw StorageException(generation_path(next), "не удалось создать каталог поколения");
    }
    return next;
}

void Storage::commit_generation(Generation generation) {
    if (!fs::is_directory(generation_path(generation))) {
        throw StorageException(generation_path(generation), "каталог поколения отсутствует");
    }
    const fs::path temp = root_ / kCurrentTempName;
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw StorageException(temp, "не удалось открыть файл для записи");
        }
        out << generation << '\n';
        out.flush();
        if (!out) {
            throw StorageException(temp, "ошибка записи в файл");
        }
    }
    replace_file(temp, root_ / kCurrentName);
    prune_old(generation);
}

void Storage::abort_generation(Generation generation) noexcept {
    std::error_code ec;
    fs::remove_all(generation_path(generation), ec);
}

void Storage::prune_old(Generation newest) noexcept {
    std::error_code ec;
    for (const Generation generation : list_generations(root_)) {
        if (generation + kKeptGenerations <= newest) {
            fs::remove_all(generation_path(generation), ec);
        }
    }
}
