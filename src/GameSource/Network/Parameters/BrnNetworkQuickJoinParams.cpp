#include "BrnNetworkQuickJoinParams.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Network/Parameters/BrnNetworkParameterData.h"     // MatchmakingContext, AmendRankedContexts

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::QuickJoinParams::`vector deleting destructor'  @ 0x8255EB38
//
// The vector deleting destructor stores the class vptr (off_8207C91C) at this+0
// and conditionally calls operator delete. That is precisely the thunk a virtual
// destructor compiles to; reconstructed here as the out-of-line virtual dtor.
//
// The search-context list of the platform record is a list of { id, value } pairs with the
// count after it; the game-side context helpers take it as a MatchmakingContext array and an
// s32 count, so the two platform members are handed over through that view.

namespace BrnNetwork
{
    QuickJoinParams::~QuickJoinParams()
    {
    }

    // Platform prepare (clears the parameter list and the context list), then publish the
    // default contexts: game type 1, game mode 0.
    bool QuickJoinParams::Prepare()
    {
        if (!CgsNetwork::ServerInterfaceQuickJoinParams::Prepare())
        {
            return false;
        }

        MatchmakingContext* lpaContexts = reinterpret_cast<MatchmakingContext*>(maX360Payload);

        muX360Field_9C = 0;
        lpaContexts[muX360Field_9C].muContextId = KU_CONTEXT_RANKED;
        lpaContexts[muX360Field_9C].muValue     = 1;
        ++muX360Field_9C;
        lpaContexts[muX360Field_9C].muContextId = KU_CONTEXT_UNRANKED;
        lpaContexts[muX360Field_9C].muValue     = 0;
        ++muX360Field_9C;
        return true;
    }

    void QuickJoinParams::SetMatchmakingParameter(EBrnQuickJoinParameters leParameter, s32 liValue)
    {
        CGS_ASSERT(static_cast<s32>(leParameter) >= 0 && leParameter < E_QUICKJOIN_COUNT,
                   "(int32_t)leParameter >= 0 && leParameter < E_QUICKJOIN_COUNT");

        maiQuickJoinParams[leParameter] = liValue;
        ++miNumParameters;
        CGS_ASSERT(miNumParameters <= CgsNetwork::KI_MAX_QUICK_JOIN_PARAMS, "miNumParameters <= 16");
    }

    void QuickJoinParams::SetMatchmakingParameter(EBrnQuickJoinParameters leParameter, bool lbValue)
    {
        CGS_ASSERT(static_cast<s32>(leParameter) >= 0 && leParameter < E_QUICKJOIN_COUNT,
                   "(int32_t)leParameter >= 0 && leParameter < E_QUICKJOIN_COUNT");

        SetMatchmakingParameter(leParameter, static_cast<s32>(lbValue ? 1 : 0));

        if (leParameter == E_QUICKJOIN_RANKED)
        {
            AmendRankedContexts(static_cast<char>(lbValue), reinterpret_cast<s32*>(&muX360Field_9C),
                                reinterpret_cast<MatchmakingContext*>(maX360Payload));
            SetRanked(lbValue);
        }
    }
}
