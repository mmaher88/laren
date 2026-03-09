#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace laren::util {

// RAII wrapper around mmap for read-only memory-mapped files
class MmapFile {
public:
    MmapFile() = default;
    ~MmapFile();

    MmapFile(const MmapFile&) = delete;
    MmapFile& operator=(const MmapFile&) = delete;
    MmapFile(MmapFile&& other) noexcept;
    MmapFile& operator=(MmapFile&& other) noexcept;

    // Open and mmap a file. Returns false on failure.
    bool open(const std::string& path);

    // Close the mapping
    void close();

    // Access the mapped data
    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }
    bool is_open() const { return data_ != nullptr; }

    std::span<const uint8_t> span() const { return {data_, size_}; }

    template <typename T>
    const T* as() const {
        return reinterpret_cast<const T*>(data_);
    }

private:
    uint8_t* data_ = nullptr;
    size_t size_ = 0;
    int fd_ = -1;
};

} // namespace laren::util
