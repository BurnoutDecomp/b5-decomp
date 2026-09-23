#ifndef BRN_NETWORK_QUICK_JOIN_PARAMS_H
#define BRN_NETWORK_QUICK_JOIN_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.h" // CgsNetwork::ServerInterfaceQuickJoinParams (base)

// ===========================================================================
// BrnNetwork::QuickJoinParams
//   Home: GameSource/Network/Parameters/BrnNetworkQuickJoinParams.{h,cpp}
//
// Game-side "quick join" parameter object: the platform quick-join record (count, the
// matchmaking parameter list, ranked / join-userset flags, and the Xbox LIVE search-context
// list) plus the game's matchmaking parameters. Its vector deleting destructor
// stores the class vptr and conditionally frees -- the codegen of a virtual destructor.
// The leaf adds no data: the matchmaking quick-join action's stack frame places the next
// local 0xA0 bytes on, the size of the platform leaf.
// ===========================================================================

namespace BrnNetwork
{
    // The matchmaking parameters a quick-join request carries (maiQuickJoinParams index).
    // The console asserts the index below 5 and the quick-join action sets index 4, one past
    // the reference list.
    enum EBrnQuickJoinParameters
    {
        E_QUICKJOIN_SKILL_LEVEL      = 0,
        E_QUICKJOIN_RANKED           = 1,
        E_QUICKJOIN_FIREWALL_SETTING = 2,
        E_QUICKJOIN_FREEBURN         = 3,
        E_QUICKJOIN_PARAMETER_4      = 4,   // FLAG: name unrecovered (the action stores 2)
        E_QUICKJOIN_COUNT            = 5,
    };

    class QuickJoinParams : public CgsNetwork::ServerInterfaceQuickJoinParams
    {
    public:
        virtual ~QuickJoinParams();

        // Platform prepare, then publish the default Xbox LIVE search contexts (game type 1,
        // game mode 0).
        virtual bool Prepare() override;

        // Store one matchmaking parameter and count it (at most 16).
        void SetMatchmakingParameter(EBrnQuickJoinParameters leParameter, s32 liValue);
        // Store a flag parameter as 0 / 1; the ranked flag also republishes the ranked
        // search context and the platform ranked flag.
        void SetMatchmakingParameter(EBrnQuickJoinParameters leParameter, bool lbValue);
    };
}

#endif // BRN_NETWORK_QUICK_JOIN_PARAMS_H
