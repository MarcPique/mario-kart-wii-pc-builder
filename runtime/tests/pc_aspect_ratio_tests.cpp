#include "pc_aspect_ratio.h"
#include <cassert>
#include <set>
#include <string>
#include <iostream>
int main(){
    // Existing profiles retain their presentation until the new selector is used.
    assert(PcAspectRatio::Resolve({},true,false)==0);
    assert(PcAspectRatio::Resolve({},false,false)==1);
    assert(PcAspectRatio::Resolve({},false,true)==5);
    assert(PcAspectRatio::Resolve(std::string("32:9"),false,false)==12);
    assert(PcAspectRatio::Resolve(std::string("invalid"),true,false)==0);
    assert(!PcAspectRatio::Find("0:0"));
    std::set<std::string_view> names;
    for(const auto& ratio:PcAspectRatio::ratios){
        assert(names.insert(ratio.code).second);
        if(ratio.code=="auto")assert(ratio.width==0&&ratio.height==0);
        else assert(ratio.height&&float(ratio.width)/ratio.height>=0.25f&&float(ratio.width)/ratio.height<=6.f);
    }
    const auto& cinema=PcAspectRatio::ratios[*PcAspectRatio::Find("1.85:1")];
    assert(cinema.width==37&&cinema.height==20);
    const auto& ultrawide=PcAspectRatio::ratios[*PcAspectRatio::Find("43:18")];
    assert(ultrawide.width*1440==ultrawide.height*3440);
    std::cout<<"Aspect ratios: legacy profiles, explicit selection, invalid settings and common monitor geometry passed\n";
}
