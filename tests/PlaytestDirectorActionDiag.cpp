// Production diagnostic helper; unrelated boot actions must not consume
// Showtime140/144 evidence. Tests the opt-in, bounds and independent budgets.
#include "types.hpp"
#include <cstdlib>
#include <cstdio>
namespace CgsDev { namespace Log { void* gpDebugPrint=reinterpret_cast<void*>(1); } }
#include "director_action_diag_methods.inc"
static s32 gChecks=0,gFailures=0;
static void Check(bool pass,const char* name){++gChecks;if(!pass){++gFailures;std::printf("FAIL %s\n",name);}}
int main(int argc,char** argv)
{
    const s32 mode=argc>1?std::atoi(argv[1]):1;
    if(mode==0)_putenv_s("BRN_DIRECTOR_ACTION_DIAG","");else _putenv_s("BRN_DIRECTOR_ACTION_DIAG","1");
    if(mode==2)CgsDev::Log::gpDebugPrint=nullptr;
    if(mode!=1)
    {
        const s32 actions[]={218,140,144};
        for(s32 action:actions)Check(!DiagCall(action),"disabled or null logger");
    }
    else
    {
        s32 boot=0;for(s32 i=0;i<650;++i)boot+=DiagCall(218)?1:0;
        Check(boot==400,"boot budget bounded");Check(!DiagCall(218),"boot cap remains closed");
        Check(DiagCall(140),"first vehicle hit observed after boot cap");
        Check(DiagCall(140),"second vehicle hit observed after boot cap");
        s32 bounces=0;for(s32 i=0;i<38;++i)bounces+=DiagCall(144)?1:0;
        Check(bounces==38,"all38 relayed bounces observable");
        s32 hits=0;for(s32 i=0;i<650;++i)hits+=DiagCall(140)?1:0;
        Check(hits==398,"hit budget bounded independently");Check(!DiagCall(140),"hit cap remains closed");
        s32 remaining=0;for(s32 i=0;i<650;++i)remaining+=DiagCall(144)?1:0;
        Check(remaining==362,"bounce budget bounded independently");Check(!DiagCall(144),"bounce cap remains closed");
        s32 fallback=0;for(s32 i=0;i<650;++i)fallback+=DiagCall(-1)?1:0;
        Check(fallback==400,"invalid-index fallback bounded");Check(!DiagCall(9000),"fallback shared by out-of-range actions");
    }
    std::printf("Director diagnostics mode%d: %d checks, %d failures\n",mode,gChecks,gFailures);
    return gFailures?1:0;
}
