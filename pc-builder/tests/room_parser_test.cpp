#include "pc_room_parser.h"
#include <iostream>
#include <stdexcept>
int main(){
    auto empty=PcOnlineRooms::ParseRooms(R"({"rooms":[]})");
    if(!empty.available||!empty.checked||empty.players||!empty.rooms.empty())return 1;
    auto list=PcOnlineRooms::ParseRooms(R"({"rooms":[{"id":"TEST-A","type":"anybody","players":[{},{}]},{"id":"TEST-B","type":"private","players":[{}]}]})");
    if(list.rooms.size()!=2||list.players!=3||list.rooms[0].players!=2)return 2;
    for(auto bad:{"{",R"({"rooms":null})",R"({"rooms":[{"id":7,"players":[]}]})",R"({"rooms":[{"id":"a","players":{}}]})"}){
        bool rejected=false;try{PcOnlineRooms::ParseRooms(bad);}catch(const std::exception&){rejected=true;}if(!rejected)return 3;
    }
    nlohmann::json huge;huge["rooms"]=nlohmann::json::array();for(int i=0;i<513;++i)huge["rooms"].push_back({{"id","a"},{"players",nlohmann::json::array()}});
    bool rejected=false;try{PcOnlineRooms::ParseRooms(huge.dump());}catch(const std::exception&){rejected=true;}if(!rejected)return 4;
    std::cout<<"Room parser: valid, empty, malformed, wrong-type and oversized responses passed\n";
}
