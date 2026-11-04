#pragma once
#include "pc_local_session_control.h"

// Called on the guest menu thread; the launcher owns process/window operations.
namespace PcLocalSessions {
bool Available();
unsigned Players();
bool SetPlayers(unsigned count);
bool Begin();
void End();
void Stop();
bool Active();
unsigned Error();
void Report(unsigned section,unsigned page,PcLocalSessionControl::Phase phase,unsigned totalPlayers=0,unsigned localPlayers=0);
}
