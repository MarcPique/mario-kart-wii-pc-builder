#include "pc_local_session_control.h"
#include <cassert>
#include <iostream>
int main(){
    using namespace PcLocalSessionControl;
    assert(ParseReady("MKW_READY1\n12\n34\nend\n"));assert(!ParseReady("MKW_READY1\n0\n34\nend\n"));assert(!ParseReady("MKW_READY1\n12\n34\nend\nextra\n"));
    Request request{Action::Start,"0123456789abcdef0123456789abcdef",123,0x123456789abcdef0ULL,4};
    const auto decoded=ParseRequest(Encode(request));assert(decoded&&decoded->primaryWindow==request.primaryWindow&&decoded->primaryPid==123&&decoded->players==4);
    request.players=3;assert(ParseRequest(Encode(request)));request.players=2;assert(!ParseRequest(Encode(request)));request.players=4;
    request.primaryWindow=0;assert(!ParseRequest(Encode(request)));request.action=Action::Stop;assert(ParseRequest(Encode(request)));
    request.primaryPid=0;assert(!ParseRequest(Encode(request)));request.primaryPid=123;
    request.group="../outside";assert(!ParseRequest(Encode(request)));request.group="0123456789abcdef0123456789abcdef";
    const auto valid=Encode(request);assert(!ParseRequest(valid+"extra\n"));assert(!ParseRequest(valid.substr(0,valid.size()-1)));assert(!ParseRequest(std::string(1024,'x')));
    auto bad=valid;bad[0]='!';assert(!ParseRequest(bad));bad=valid;bad[5]='\0';assert(!ParseRequest(bad));
    bad=valid;const auto pid=bad.find("123\n");bad.replace(pid,3,"4294967296");assert(!ParseRequest(bad));bad=valid;bad.replace(pid,3,"-12");assert(!ParseRequest(bad));
    State state{request.group,321,1234,987,116,67,Phase::Race,4,2};auto status=ParseState(Encode(state));assert(status&&status->phase==Phase::Race&&status->localPlayers==2&&status->stamp==987);
    state.totalPlayers=13;assert(!ParseState(Encode(state)));state.totalPlayers=1;assert(!ParseState(Encode(state)));state.totalPlayers=4;state.phase=static_cast<Phase>(99);assert(!ParseState(Encode(state)));
    Reply reply{request.group,123,321,0,0};assert(ParseReply(Encode(reply)));reply.result=99;assert(!ParseReply(Encode(reply)));
    std::cout<<"Session control: exact framing, owned identifiers, 3/4-player counts, state bounds and malformed requests passed\n";
}
