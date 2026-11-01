#include "pc_match_filter.h"
#include <iostream>
int main(){
    const std::string normal="dwc_mver = 3 and dwc_pid != 7 and maxplayers = 12 and numplayers < 12 and dwc_mtype = 0 and dwc_hoststate = 2 and dwc_suspend = 0 and (rk = 'vs_10' and ev >= 0 and ev <= 9999 and p = 0)";
    const auto narrowed=PcOnlineRooms::NarrowMatchFilter(normal,"vs_10",123);
    if(!narrowed||*narrowed!="("+normal+") and (dwc_pid = 123)")return 1;
    if(PcOnlineRooms::NarrowMatchFilter("dwc_pid = 7","vs_10",123))return 2;
    if(PcOnlineRooms::NarrowMatchFilter(normal,"vs_20",123))return 3;
    if(PcOnlineRooms::NarrowMatchFilter(normal,"vs_10' or 1=1",123))return 4;
    if(PcOnlineRooms::NarrowMatchFilter(normal,"vs_10",0))return 5;
    if(PcOnlineRooms::NarrowMatchFilter(normal+std::string(255,' '),"vs_10",123))return 6;
    const std::array<uint32_t,4> members{1111111111u,2222222222u,3333333333u,4000000000u};
    auto first=PcOnlineRooms::NarrowMatchFilterBatch(normal,"vs_10",members,0);
    if(!first||first->expression.size()>=256||first->nextOffset==0)return 7;
    auto next=PcOnlineRooms::NarrowMatchFilterBatch(normal,"vs_10",members,first->nextOffset);
    if(!next||next->expression.size()>=256||next->expression==first->expression)return 8;
    const auto blocked=PcOnlineRooms::BlockUnrelatedMatches(normal);
    if(!blocked||*blocked!="("+normal+") and dwc_pid = 7")return 9;
    if(PcOnlineRooms::BlockUnrelatedMatches("dwc_pid = 7")||PcOnlineRooms::BlockUnrelatedMatches(normal+std::string(255,' ')))return 10;
    const auto route=PcOnlineRooms::RouteForRoomMode("vs_10");
    if(!route||route->mainId!=6||route->modeId!=1||PcOnlineRooms::RouteForRoomMode("vs_22"))return 11;
    if(!PcOnlineRooms::IsConnectedToSelectedRoom(true,members[1],members))return 12;
    if(PcOnlineRooms::IsConnectedToSelectedRoom(false,members[1],members)||PcOnlineRooms::IsConnectedToSelectedRoom(true,9,members)||PcOnlineRooms::IsConnectedToSelectedRoom(true,0,members))return 13;
    std::cout<<"Match filter: preserves original constraints and rejects unsupported requests\n";
}
