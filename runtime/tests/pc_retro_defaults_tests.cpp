#include "pc_retro_defaults.h"
#include <array>
#include <cassert>
#include <cstring>

int main() {
    std::array<uint8_t,0x80> file{};
    std::memcpy(file.data(),"PULP",4); file[7]=7; file[11]=0x80;
    file[15]=4; file[19]=0x20;
    std::memcpy(file.data()+0x20,"SETT",4); file[0x27]=1; file[0x2b]=0x50;
    const auto pristine=file;
    assert(PcRetroDefaults::EnableAllTracks(file.data(),file.size()));
    assert(file[0x4c]==2);
    for (size_t i=0;i<file.size();++i) if(i!=0x4c) assert(file[i]==pristine[i]);
    assert(!PcRetroDefaults::EnableAllTracks(file.data(),file.size()));
    file=pristine; file[7]=8;
    assert(!PcRetroDefaults::EnableAllTracks(file.data(),file.size()));
    file=pristine; file[11]=0x7f;
    assert(!PcRetroDefaults::EnableAllTracks(file.data(),file.size()));
    file=pristine; file[0x20]='X';
    assert(!PcRetroDefaults::EnableAllTracks(file.data(),file.size()));
    assert(!PcRetroDefaults::EnableAllTracks(file.data(),0));
}
