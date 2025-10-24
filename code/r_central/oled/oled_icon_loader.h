#pragma once
#include <vector>
#include <map>
#include <dirent.h>
#include <string>
#include <cstdint>

struct OLEDIcon {
    int width;
    int height;
    std::vector<std::vector<bool>> pixels;
    std::vector<uint8_t> packed;
};

class OLEDIconLoader {
private:
    std::map<std::string, OLEDIcon> icon_cache_;

    std::vector<uint8_t> pack_pixels(const std::vector<std::vector<bool>>& pixels) const;

    OLEDIcon parse_bmp(const std::string& filename) const;

public:
    int load_icons(const std::string& directory);

    const OLEDIcon& get_icon(const std::string& name) const;

    static OLEDIconLoader& get_instance();

    static bool string_starts_with(const std::string& str, const std::string& prefix);
    static bool string_ends_with(const std::string& str, const std::string& suffix);
};