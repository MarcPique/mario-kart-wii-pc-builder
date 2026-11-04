#include "pc_my_stuff.h"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
    const std::string fixture=R"(<wiidisc version="1"><id game="RMC"/>
      <patch id="RRLoadPack"><savegame external="/save"/></patch>
      <patch id="RRCTGPLoad"><folder external="/ctgpr/My Stuff" recursive="true"/></patch>
      <patch id="RRLoad"><folder external="/RetroRewind6/MyStuff" disc="/"/></patch>
      <patch id="RRCTGPLoadMusic"><folder external="/ctgpr/My Stuff" disc="/sound/strm" create="true"/></patch>
      <patch id="RRLoadMusic"><folder external="/RetroRewind6/MyStuff" disc="/sound/strm" create="true"/></patch>
    </wiidisc>)";
    if(PcMyStuff::Prepare(fixture,"RMC P01",5)||PcMyStuff::Prepare("{","RMCP01",1)||PcMyStuff::Prepare(fixture,"ABCD01",1))return 1;
    const auto disabled=PcMyStuff::Prepare(fixture,"RMCP01",0);
    if(!disabled||disabled->runtimeFlag||!disabled->patch.folderPatches.empty())return 2;
    for(unsigned mode=1;mode<=4;++mode){
        const auto plan=PcMyStuff::Prepare(fixture,"RMCP01",mode);
        if(!plan||plan->patch.folderPatches.size()!=1||!plan->patch.savegamePatches.empty()||plan->runtimeFlag!=(mode>=3?2:1))return 3;
        if(mode>=3&&plan->patch.folderPatches[0].disc!="/sound/strm")return 4;
    }
    const std::string unsafe=R"(<wiidisc version="1"><id game="RMC"/><patch id="RRLoad"><savegame external="/save"/></patch></wiidisc>)";
    if(PcMyStuff::Prepare(unsafe,"RMCP01",2))return 5;
    if(argc==2){std::ifstream file(argv[1]);const std::string xml{std::istreambuf_iterator<char>(file),{}};
        for(unsigned mode=0;mode<=4;++mode){const auto p=PcMyStuff::Prepare(xml,"RMCP01",mode);if(!p)return 6;std::cout<<"Mode "<<mode<<": "<<p->patch.filePatches.size()<<" files, "<<p->patch.folderPatches.size()<<" folders, no save redirect\n";}}
    std::cout<<"My Stuff plans: four modes, disabled, invalid input and save isolation passed\n";
}
