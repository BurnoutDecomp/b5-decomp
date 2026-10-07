// BrnTrafficEntityModule_wS34_00.cpp -- TrafficEntityModule's release machine and teardown
// (the Release / Destruct half of the lifecycle in BrnTrafficEntityModule.cpp).

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficDebugComponent.h"   // mpDebugComponent->Destruct()

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnTraffic
{
    // ========================================================================
    // TrafficEntityModule::Release (vtable slot 2, 47 insns). Re-entered from
    // WorldModule::Release stage eWorldReleaseTrafficEntityModule until it returns true.
    //
    //   START    meReleaseStage = MANAGER (+0x2F4), fall into MANAGER
    //   MANAGER  if (!ModuleSingleBuffered::Release()) return false;
    //            mReceiverQueue.Clear() (+0x314);
    //            maTrafficPhysicsInfoListBits <- 0 (one 64-bit store at +0x713A0);
    //            meReleaseStage = DONE, fall into DONE
    //   DONE     mePrepareStage = E_PREPARESTAGE_START (+0x2F0); return true
    //   other    assert "0"; return false
    // ========================================================================
    bool TrafficEntityModule::Release()
    {
        switch ( meReleaseStage )
        {
            case E_RELEASESTAGE_START:
            {
                meReleaseStage = E_RELEASESTAGE_MANAGER;
            }
            // fall through

            case E_RELEASESTAGE_MANAGER:
            {
                if ( !CgsModule::ModuleSingleBuffered::Release() )
                {
                    return false;
                }

                mReceiverQueue.Clear();
                maTrafficPhysicsInfoListBits.UnSetAll();

                meReleaseStage = E_RELEASESTAGE_DONE;
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

    // ========================================================================
    // TrafficEntityModule::Destruct (vtable slot 3, 26 insns).
    //
    //   mReceiverQueue.Clear()                               (+0x314)
    //   if (mpDebugComponent) mpDebugComponent->Destruct()   (pointer at +0x727B0)
    //   mStreamer.Destruct()                                 (+0x72B58)
    //   <module debug render stream reader>.Destruct()       (see the FLAG below)
    //   ModuleSingleBuffered::Destruct()
    // ========================================================================
    void TrafficEntityModule::Destruct()
    {
        mReceiverQueue.Clear();

        if ( mpDebugComponent != nullptr )
        {
            mpDebugComponent->Destruct();
        }

        mStreamer.Destruct();

        // [FLAG PC bring-up] the console destructs the module's file-scope
        // CgsDev::DebugRenderStreamReader here (the same object Construct builds with
        // DebugRenderStreamReader::Construct(allocator, 500, 0x8000) and UpdateVehicles brackets
        // with Begin/End). That object has no home in this tree, so Construct never builds it
        // and there is nothing to destruct; its Destruct folds to an empty body on the console.
        // DELETE-WHEN the stream reader lands with Construct's leg.

        CgsModule::ModuleSingleBuffered::Destruct();
    }
}
