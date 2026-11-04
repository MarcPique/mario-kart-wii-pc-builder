#pragma once
#include <array>
#include <string_view>
#include <optional>
#include <string>
namespace PcAspectRatio {
struct Ratio {std::string_view code;std::u16string_view name;unsigned width,height;};
inline constexpr std::array ratios{
    Ratio{"auto",u"Auto",0,0},Ratio{"4:3",u"4:3",4,3},Ratio{"5:4",u"5:4",5,4},
    Ratio{"3:2",u"3:2",3,2},Ratio{"16:10",u"16:10",16,10},Ratio{"16:9",u"16:9",16,9},
    Ratio{"1.85:1",u"1.85:1",37,20},Ratio{"2:1",u"2:1",2,1},Ratio{"21:9",u"21:9",21,9},
    Ratio{"43:18",u"43:18",43,18},Ratio{"64:27",u"64:27",64,27},Ratio{"32:10",u"32:10",32,10},
    Ratio{"32:9",u"32:9",32,9},Ratio{"48:9",u"48:9",48,9},Ratio{"1:1",u"1:1",1,1},
    Ratio{"9:16",u"9:16",9,16}};
inline std::optional<unsigned> Find(std::string_view code){for(unsigned i=0;i<ratios.size();++i)if(ratios[i].code==code)return i;return {};}
inline unsigned Resolve(const std::optional<std::string>& code,bool wide,bool force){if(code)if(auto i=Find(*code))return *i;return force?5:wide?0:1;}
}
