
#include "GameSource/Network/X360/BrnServerInterfaceX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePrepareParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"  // CgsNetwork::E_COMPONENTS_GAMES

namespace BrnNetwork
{
    bool BrnServerInterfaceX360::Prepare( CgsNetwork::ServerInterfacePrepareParams * lpPrepareParams )
    {
        // Unconditional, and BEFORE the switch: hand the Prepare chain the one
        // component this platform layer adds. The X360 is
        // `addi r30, r31, 0xC2C ; stw r30, 4(r4)` -- the +4 destination is slot 1 of the
        // params' component array, i.e. mapComponents[E_COMPONENTS_GAMES]. An earlier
        // draft named that storage as a scalar mpComponentToPrepare; the DWARF has no such
        // member and types it as ServerInterfaceComponent*[12], so the slot is addressed
        // by its enumerator here rather than by the console's byte offset.
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_GAMES] = &mGames;

        switch ( mePrepareStage )
        {
        case E_PREPARESTAGE_START:
            // Re-store of the stage the switch just matched (asm case 0). Redundant
            // on the value, but emitted -- it is what makes each arm a self-contained
            // resume point.
            mePrepareStage = E_PREPARESTAGE_START;
            // fall through

        case E_PREPARESTAGE_BASECLASS:
            mePrepareStage = E_PREPARESTAGE_BASECLASS;
            if ( !BrnServerInterfaceBase::Prepare( lpPrepareParams ) )
            {
                return false;
            }
            // fall through

        case E_PREPARESTAGE_GAMES_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_GAMES_COMPONENT;
            if ( !mGames.Prepare( this ) )
            {
                return false;
            }
            // fall through

        case E_PREPARESTAGE_DONE:
            // Arm the MIRROR machine for the next Release (measured: stw of the same
            // zero register to +0x14C0, before the mePrepareStage store).
            meReleaseStage = E_RELEASESTAGE_START;
            mePrepareStage = E_PREPARESTAGE_DONE;
            return true;

        default:
            // BrnServerInterfaceX360.cpp:120 -- expression string "0" verbatim.
            CGS_ASSERT( false, "0" );
            return false;
        }
    }
}
