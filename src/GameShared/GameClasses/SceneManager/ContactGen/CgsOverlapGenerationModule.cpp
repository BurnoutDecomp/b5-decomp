// ===========================================================================
// CgsSceneManager::OverlapGenerationModule -- broad-phase overlap-generation stage.
//
// Reconstructed from the BURNOUT_X360_ARTIST.XEX (Jan-2008 "Breaker" build). The six
// functions owned by this TU:
//   Construct              @ 0x828D0460
//   Prepare                @ 0x828CB6B0
//   Release                @ 0x828CB798
//   ProcessAddBodyQueue    @ 0x828C1D18
//   ProcessUpdateBodyQueue @ 0x828C1DE8
//   ProcessRemoveBodyQueue @ 0x828C1E90
//
// Prepare/Release are resumable multi-stage state machines (advanced one stage per
// call via the DWARF post-increment operator++ on the EPrepareStage / EReleaseStage
// enums) so the module spreads its alloc/free work across frames. The Process*BodyQueue
// passes replay the per-frame body mutations decoded from the input buffer into the
// SceneSweeper and keep the per-culling-group live-body counters in step.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

namespace CgsSceneManager
{

// ---------------------------------------------------------------------------------
// Post-increment the resumable state-machine stage enums (DWARF
// CgsOverlapGenerationModule.h:167/168 -- operator++(EStage&, int)). The X360 build
// emits these as out-of-line helpers (sub_828AA218 / sub_828AA278); each advances the
// stage by one and returns the pre-increment value.
// ---------------------------------------------------------------------------------
static OverlapGenerationModule::EPrepareStage operator++(OverlapGenerationModule::EPrepareStage& lreStage, int)
{
    const OverlapGenerationModule::EPrepareStage eOld = lreStage;
    lreStage = static_cast<OverlapGenerationModule::EPrepareStage>(static_cast<int>(lreStage) + 1);
    return eOld;
}

static OverlapGenerationModule::EReleaseStage operator++(OverlapGenerationModule::EReleaseStage& lreStage, int)
{
    const OverlapGenerationModule::EReleaseStage eOld = lreStage;
    lreStage = static_cast<OverlapGenerationModule::EReleaseStage>(static_cast<int>(lreStage) + 1);
    return eOld;
}

// ---------------------------------------------------------------------------------
// Construct @ 0x828D0460
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    // Construct sets the release stage to DONE and the prepare stage to START -- the
    // module starts "released, not yet prepared".
    meReleaseStage = E_RELEASESTAGE_DONE;
    mePrepareStage = E_PREPARESTAGE_START;

    // Build + register + clear the sweep-and-prune broadphase (the X360 carries this
    // sweeper construction inlined: debug-component register, interval-pair queue
    // construct, defaults, Clear).
    mSweeper.Construct();

    // Reset the per-culling-group live-body counters.
    for (s32 liType = 0; liType < KI_NUM_ENTITY_TYPES; ++liType)
    {
        mau16NumEntitiesPerType[liType] = 0;
    }

    // Mark this as a freshly-constructed module (CgsModule::Module::mbIsNewModule).
    mbIsNewModule = true;
}

// ---------------------------------------------------------------------------------
// Prepare @ 0x828CB6B0 -- resumable, one stage per call.
//   lpCullingGroupManager / lpCullingTable forward to SceneSweeper::Prepare.
// ---------------------------------------------------------------------------------
bool OverlapGenerationModule::Prepare(void* lpCullingGroupManager, void* lpCullingTable)
{
    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_DONE:
        // A re-prepare after a finished cycle restarts the machine.
        mePrepareStage = E_PREPARESTAGE_START;
        // fall through
    case E_PREPARESTAGE_START:
        mePrepareStage++;  // START -> MANAGER
        // fall through
    case E_PREPARESTAGE_MANAGER:
        if (!CgsModule::ModuleSingleBuffered::Prepare())
        {
            return false;
        }
        mePrepareStage++;  // MANAGER -> SWEEPER
        // fall through
    case E_PREPARESTAGE_SWEEPER:
        if (!mSweeper.Prepare(lpCullingGroupManager, lpCullingTable))
        {
            return false;
        }
        mePrepareStage++;  // SWEEPER -> DONE
        meReleaseStage = E_RELEASESTAGE_START;
        return true;

    default:
        CGS_ASSERT(false, "Unknown prepare state");
        return false;
    }
}

// ---------------------------------------------------------------------------------
// Release @ 0x828CB798 -- resumable, one stage per call (mirror of Prepare).
// ---------------------------------------------------------------------------------
bool OverlapGenerationModule::Release()
{
    switch (meReleaseStage)
    {
    case E_RELEASESTAGE_START:
        meReleaseStage++;  // START -> SWEEPER
        // fall through
    case E_RELEASESTAGE_SWEEPER:
        mSweeper.Clear();
        meReleaseStage++;  // SWEEPER -> MANAGER
        // fall through
    case E_RELEASESTAGE_MANAGER:
        if (!CgsModule::ModuleSingleBuffered::Release())
        {
            return false;
        }
        meReleaseStage++;  // MANAGER -> DONE
        // fall through
    case E_RELEASESTAGE_DONE:
        mePrepareStage = E_PREPARESTAGE_START;
        return true;

    default:
        CGS_ASSERT(false, "Unrecognised release state\n");
        return false;
    }
}

// ---------------------------------------------------------------------------------
// ProcessAddBodyQueue @ 0x828C1D18 -- replay each queued add-body request into the
// sweeper and bump the owning culling group's live-body counter.
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::ProcessAddBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");

    // The X360 reads the add-body queue out of the input structure (sub_828AFEE8) and
    // iterates its decoded events by GetLength()/GetEvent(index).
    const OverlapGenerationIO::InAddBodyQueue& lrQueue = lpInputBuffer->GetAddBodyQueue();

    const s32 liNumEvents = lrQueue.GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        const OverlapGenerationIO::InAddBodyEvent& lrEvent = lrQueue.GetEvent(liEvent);

        mSweeper.AddObject(lrEvent.muObjectIndex, &lrEvent, lrEvent.muVolumeHandle, lrEvent.mu64Body);

        // The culling group is the high byte of the high word of the packed body id.
        const u32 luCullingGroup = static_cast<u32>(lrEvent.mu64Body >> 56);
        ++mau16NumEntitiesPerType[luCullingGroup];
    }
}

// ---------------------------------------------------------------------------------
// ProcessUpdateBodyQueue @ 0x828C1DE8 -- replay each queued update-body request into
// the sweeper.
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::ProcessUpdateBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");

    const OverlapGenerationIO::InUpdateBodyQueue& lrQueue = lpInputBuffer->GetUpdateBodyQueue();

    const s32 liNumEvents = lrQueue.GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        const OverlapGenerationIO::InUpdateBodyEvent& lrEvent = lrQueue.GetEvent(liEvent);

        mSweeper.UpdateObject(lrEvent.muObjectIndex, &lrEvent, lrEvent.mu64Body, lrEvent.mvPosition);
    }
}

// ---------------------------------------------------------------------------------
// ProcessRemoveBodyQueue @ 0x828C1E90 -- replay each queued remove-body request into
// the sweeper and decrement the owning culling group's live-body counter.
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::ProcessRemoveBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");

    const OverlapGenerationIO::InRemoveBodyQueue& lrQueue = lpInputBuffer->GetRemoveBodyQueue();

    const s32 liNumEvents = lrQueue.GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        const OverlapGenerationIO::InRemoveBodyEvent& lrEvent = lrQueue.GetEvent(liEvent);

        mSweeper.RemoveObject(lrEvent.muObjectIndex);

        const u32 luCullingGroup = static_cast<u32>(lrEvent.mu64Body >> 56);
        --mau16NumEntitiesPerType[luCullingGroup];
    }
}

}
