#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string_view>

namespace pc {
inline std::filesystem::path pathFromUtf8(std::string_view value) {
    return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
std::string assetKey(std::string name);

class Assets {
public:
    Assets() = default;
    explicit Assets(const std::filesystem::path& root) { add(root); }
    void add(const std::filesystem::path& source);
    // Files read in place from ranges of one file (the ROM, the .obb).
    void addRanges(const std::filesystem::path& file, const std::map<std::string, std::pair<std::uint64_t, std::uint64_t>>& ranges);
    std::vector<std::uint8_t> read(const std::string& name) const;
    bool contains(const std::string& name) const;
    std::size_t count() const { return files_.size(); }
    std::vector<std::string> names() const;
private:
    struct Entry { std::filesystem::path path; std::uint64_t offset=0,size=0,hash=0; bool packed=false,ranged=false; };
    std::unordered_map<std::string, Entry> files_;
};
}
