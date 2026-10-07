// BrnAIModule_wS34_00.cpp -- BrnAI::AIModule's release machine and its stage-enum
// post-increment (the release half of the lifecycle in BrnAIModule.cpp).

#include "GameSource/World/AI/BrnAIModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnAI
{

// BrnAI::operator++(AIModule::EReleaseStage&, int), declared in BrnAIModule.h. The release-stage
// twin of the EPrepareStage operator in BrnAIModule.cpp: caches the current stage, advances it
// by one, asserts it has not walked past E_RELEASESTAGE_DONE and returns the old value.
AIModule::EReleaseStage operator++(AIModule::EReleaseStage& leEnumIndex, int)
{
    AIModule::EReleaseStage leOldValue = leEnumIndex;
    leEnumIndex = static_cast<AIModule::EReleaseStage>(static_cast<int>(leEnumIndex) + 1);
    CGS_ASSERT(leEnumIndex <= AIModule::E_RELEASESTAGE_DONE,
               "leEnumIndex <= AIModule::E_RELEASESTAGE_DONE");
    return leOldValue;
}

// =================================================================================================
// Release (vtable slot 2), the mirror of Prepare: a 4-stage machine over meReleaseStage (+0x47F70)
// re-entered from WorldModule::Release stage eWorldReleaseAI until it returns true. Each case
// falls through into the next, so one call can finish several stages:
//
//   case 0 START     mResourceReceiverQueue.Clear() (+0x47FB0); ++stage
//   case 1 BASE      if (!ModuleSingleBuffered::Release()) return false; ++stage
//   case 2 ROUTEMAP  if (!mRouteMapModule.Release()) return false; ++stage   (vtbl+8 on +0x483D8)
//   case 3 DONE      mePrepareStage = E_PREPARESTAGE_START (+0x47F6C); return true
//   default          assert "0"; return false
// =================================================================================================
bool AIModule::Release()
{
    switch ( meReleaseStage )
    {
        case E_RELEASESTAGE_START:
        {
            mResourceReceiverQueue.Clear();
            meReleaseStage++;
        }
        // fall through

        case E_RELEASESTAGE_BASE:
        {
            if ( !CgsModule::ModuleSingleBuffered::Release() )
            {
                return false;
            }
            meReleaseStage++;
        }
        // fall through

        case E_RELEASESTAGE_ROUTEMAP:
        {
            if ( !mRouteMapModule.Release() )
            {
                return false;
            }
            meReleaseStage++;
        }
        // fall through

        case E_RELEASESTAGE_DONE:
        {
            mePrepareStage = E_PREPARESTAGE_START;
            return true;
        }

        default:
        {
            CGS_ASSERT( false, "0" );
            return false;
        }
    }
}

}
