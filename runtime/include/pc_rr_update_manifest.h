#pragma once
#include <array>
#include <charconv>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace PcRetroUpdate {
using Version = std::array<unsigned,3>;
inline std::optional<Version> ParseVersion(std::string_view value) {
    Version result{};
    for (size_t i=0;i<3;++i) {
        const auto end=i==2?value.size():value.find('.');
        if(end==std::string_view::npos || end==0 || end>6) return std::nullopt;
        const auto part=value.substr(0,end);
        const auto parsed=std::from_chars(part.data(),part.data()+part.size(),result[i]);
        if(parsed.ec!=std::errc{} || parsed.ptr!=part.data()+part.size()) return std::nullopt;
        if(i<2)value.remove_prefix(end+1); else value.remove_prefix(end);
    }
    return value.empty()?std::optional<Version>(result):std::nullopt;
}
inline std::optional<Version> Latest(std::string_view manifest) {
    std::optional<Version> latest;
    std::istringstream stream{std::string(manifest)};
    std::string line;
    while(std::getline(stream,line)) {
        const auto space=line.find_first_of(" \t\r");
        const auto version=ParseVersion(std::string_view(line).substr(0,space));
        if(version && (!latest || *version>*latest)) latest=*version;
    }
    return latest;
}
inline std::string Format(const Version& v) {
    return std::to_string(v[0])+"."+std::to_string(v[1])+"."+std::to_string(v[2]);
}
}
