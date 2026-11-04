#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Local launcher coordination only. No file paths or arbitrary commands are
// accepted from a game request. Real gamepad traffic uses pc_local_input.
namespace PcLocalSessionControl {
inline bool ValidGroup(std::string_view value){
    if(value.size()!=32)return false;
    for(auto c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
enum class Action {Start,Stop};
struct Request {Action action=Action::Start;std::string group;uint32_t primaryPid=0;uint64_t primaryWindow=0;unsigned players=4;};
enum class Phase:unsigned {Boot,Menu,Selection,Online,Lobby,Race,Error};
struct State {std::string group;uint32_t pid=0;uint64_t window=0,stamp=0;unsigned section=0,page=0;Phase phase=Phase::Boot;unsigned totalPlayers=0,localPlayers=0;};
struct Reply {std::string group;uint32_t primaryPid=0,followerPid=0;unsigned result=0,error=0;};
struct Ready {uint32_t launcherPid=0,primaryPid=0;};
template<size_t N> inline std::optional<std::array<std::string_view,N>> Lines(std::string_view text,std::string_view magic){
    if(text.empty()||text.size()>512||text.back()!='\n'||text.find('\0')!=text.npos)return std::nullopt;
    std::array<std::string_view,N> lines{};
    for(auto& line:lines){const auto end=text.find('\n');if(end==text.npos)return std::nullopt;line=text.substr(0,end);text.remove_prefix(end+1);}
    if(!text.empty()||lines[0]!=magic)return std::nullopt;
    return lines;
}
template<class T> inline bool Number(std::string_view value,T& number){
    if(value.empty())return false;
    const auto result=std::from_chars(value.data(),value.data()+value.size(),number);
    return result.ec==std::errc{}&&result.ptr==value.data()+value.size();
}
inline std::optional<Ready> ParseReady(std::string_view text){
    const auto lines=Lines<4>(text,"MKW_READY1");if(!lines)return std::nullopt;
    Ready value;if(!Number((*lines)[1],value.launcherPid)||!value.launcherPid||!Number((*lines)[2],value.primaryPid)||!value.primaryPid||(*lines)[3]!="end")return std::nullopt;
    return value;
}
inline std::optional<Request> ParseRequest(std::string_view text){
    const auto lines=Lines<7>(text,"MKW_SESSION1");if(!lines)return std::nullopt;
    const auto& l=*lines;Request value;
    if(l[1]=="start")value.action=Action::Start;else if(l[1]=="stop")value.action=Action::Stop;else return std::nullopt;
    if(!ValidGroup(l[2])||!Number(l[3],value.primaryPid)||!value.primaryPid||!Number(l[4],value.primaryWindow)||!Number(l[5],value.players)||l[6]!="end")return std::nullopt;
    if((value.players!=3&&value.players!=4)||(value.action==Action::Start&&!value.primaryWindow))return std::nullopt;
    value.group=l[2];return value;
}
inline std::string Encode(const Request& r){return std::string("MKW_SESSION1\n")+(r.action==Action::Start?"start\n":"stop\n")+r.group+'\n'+std::to_string(r.primaryPid)+'\n'+std::to_string(r.primaryWindow)+'\n'+std::to_string(r.players)+"\nend\n";}
inline std::optional<State> ParseState(std::string_view text){
    const auto lines=Lines<11>(text,"MKW_STATE1");if(!lines)return std::nullopt;
    const auto& l=*lines;State value;unsigned phase=0;
    if(!ValidGroup(l[1])||!Number(l[2],value.pid)||!value.pid||!Number(l[3],value.window)||!Number(l[4],value.stamp)||!Number(l[5],value.section)||!Number(l[6],value.page)||!Number(l[7],phase)||!Number(l[8],value.totalPlayers)||!Number(l[9],value.localPlayers)||l[10]!="end")return std::nullopt;
    if(phase>unsigned(Phase::Error)||value.section>4096||value.page>4096||value.totalPlayers>12||value.localPlayers>4||value.localPlayers>value.totalPlayers)return std::nullopt;
    value.group=l[1];value.phase=static_cast<Phase>(phase);return value;
}
inline std::string Encode(const State& s){return "MKW_STATE1\n"+s.group+'\n'+std::to_string(s.pid)+'\n'+std::to_string(s.window)+'\n'+std::to_string(s.stamp)+'\n'+std::to_string(s.section)+'\n'+std::to_string(s.page)+'\n'+std::to_string(unsigned(s.phase))+'\n'+std::to_string(s.totalPlayers)+'\n'+std::to_string(s.localPlayers)+"\nend\n";}
inline std::optional<Reply> ParseReply(std::string_view text){
    const auto lines=Lines<7>(text,"MKW_REPLY1");if(!lines)return std::nullopt;
    const auto& l=*lines;Reply value;
    if(!ValidGroup(l[1])||!Number(l[2],value.primaryPid)||!value.primaryPid||!Number(l[3],value.followerPid)||!Number(l[4],value.result)||value.result>2||!Number(l[5],value.error)||l[6]!="end")return std::nullopt;
    value.group=l[1];return value;
}
inline std::string Encode(const Reply& r){return "MKW_REPLY1\n"+r.group+'\n'+std::to_string(r.primaryPid)+'\n'+std::to_string(r.followerPid)+'\n'+std::to_string(r.result)+'\n'+std::to_string(r.error)+"\nend\n";}
}
