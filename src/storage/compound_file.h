#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace cerfcore {

class CompoundFile {
public:
    bool Parse(std::vector<uint8_t> bytes);
    bool ReadStream(std::initializer_list<const char*> path, std::vector<uint8_t>& out);

    const std::string& Error() const { return error_; }

private:
    struct Entry {
        std::u16string name;
        uint8_t        type     = 0;
        uint32_t       left     = 0;
        uint32_t       right    = 0;
        uint32_t       child    = 0;
        uint32_t       start    = 0;
        uint64_t       size     = 0;
    };

    bool Fail(const std::string& reason);
    bool SectorOffset(uint32_t sector, size_t& offset);
    bool ReadFat();
    bool Chain(const std::vector<uint32_t>& table, uint32_t start,
               std::vector<uint32_t>& out);
    bool ReadDirectory();
    bool ReadMiniStream();
    bool FindChild(uint32_t parent, const char* name, uint32_t& found);
    bool NameMatches(const std::u16string& name, const char* ascii) const;

    std::vector<uint8_t>  bytes_;
    uint32_t              major_       = 0;
    uint32_t              sector_size_ = 0;
    std::vector<uint32_t> fat_;
    std::vector<uint32_t> mini_fat_;
    std::vector<Entry>    entries_;
    std::vector<uint8_t>  mini_stream_;
    std::string           error_;
};

}
