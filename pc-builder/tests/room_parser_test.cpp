#include "pc_room_parser.h"
#include "pc_room_order.h"
#include <iostream>
#include <stdexcept>
int main(){
    auto empty=PcOnlineRooms::ParseRooms(R"({"rooms":[]})");
    if(!empty.available||!empty.checked||empty.players||!empty.rooms.empty())return 1;
    auto list=PcOnlineRooms::ParseRooms(R"({"rooms":[{"id":"TEST-A","type":"anybody","players":[{},{}]},{"id":"TEST-B","type":"private","players":[{}]}]})");
    if(list.rooms.size()!=2||list.players!=3||list.rooms[0].players!=2)return 2;
    auto selection=PcOnlineRooms::ParseRooms(R"({"rooms":[{"id":"A","type":"anybody","isPublic":true,"isJoinable":true,"isSuspended":false,"host":"2","roomType":"Retro Tracks","rk":"vs_10","players":[{"slotId":"1","pid":"40"},{"slotId":"2","pid":"123"}]},{"id":"B","type":"private","isPublic":false,"isJoinable":true,"host":"1","players":[{"slotId":"1","pid":"77"}]},{"id":"C","type":"anybody","isPublic":true,"isJoinable":true,"host":"1","players":[{"slotId":"1","pid":"12x"}]}]})");
    if(!selection.rooms[0].publicJoinable||selection.rooms[0].hostPid!=123||selection.rooms[0].memberPids.size()!=2||!selection.rooms[1].memberPids.empty()||!selection.rooms[2].memberPids.empty()||selection.rooms[0].mode!="Retro Tracks"||selection.rooms[0].roomKey!="vs_10"||selection.rooms[1].publicJoinable||selection.rooms[2].publicJoinable)return 5;
    const std::vector<std::string> oldOrder{"A","B","C"},newOrder{"C","D","A"};
    if(PcOnlineRooms::StableRoomOrder(oldOrder,newOrder)!=std::vector<std::string>{"A","B","C","D"})return 6;
    if(PcOnlineRooms::StableRoomOrder({},std::vector<std::string>{"A","A","","B"})!=std::vector<std::string>{"A","B"})return 7;
    for(auto bad:{"{",R"({"rooms":null})",R"({"rooms":[{"id":7,"players":[]}]})",R"({"rooms":[{"id":"a","players":{}}]})"}){
        bool rejected=false;try{PcOnlineRooms::ParseRooms(bad);}catch(const std::exception&){rejected=true;}if(!rejected)return 3;
    }
    nlohmann::json huge;huge["rooms"]=nlohmann::json::array();for(int i=0;i<513;++i)huge["rooms"].push_back({{"id","a"},{"players",nlohmann::json::array()}});
    bool rejected=false;try{PcOnlineRooms::ParseRooms(huge.dump());}catch(const std::exception&){rejected=true;}if(!rejected)return 4;
    std::cout<<"Room parser: valid, empty, malformed, wrong-type and oversized responses passed\n";
}
