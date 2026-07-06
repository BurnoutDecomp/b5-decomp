// CgsInput::InputModule -- reconstructed from BURNOUT_X360_ARTIST.XEX. See CgsInputModule.h
// for the class banner. This TU bodies the four ledger functions:
//   InputModule::Prepare             @0x828EEFD8
//   InputModule::Release             @0x828EF100
//   InputModule::Destruct            @0x828F8438
//   InputModule::ProcessMappingQueue @0x828E7098
//
// (The previous file-scope InputModule() constructor here was a fabricated pre-DWARF stub over a
// stale "SubModule"/RWMutex class shape; it is deleted -- the DWARF class has no such ctor.)

#include "CgsInputModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"

namespace CgsInput
{

// X360 0x828EEFD8. Resumable boot prepare (manager -> pads), tracking mePrepareStage.
bool InputModule::Prepare(rw::IResourceAllocator* lpAllocator)
{
    switch (mePrepareStage)
    {
        case E_INPUTPREPARESTAGE_START:
            mePrepareStage = E_INPUTPREPARESTAGE_START;
            mOutputBindResultQueue.Clear();     // this+0x1314 miLength = 0
            mOutputUnbindResultQueue.Clear();   // this+0x1380 miLength = 0
            // fall through
        case E_INPUTPREPARESTAGE_MANAGER:
            mePrepareStage = E_INPUTPREPARESTAGE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
            {
                return false;
            }
            // fall through
        case E_INPUTPREPARESTAGE_INPUTPADS:
            mePrepareStage = E_INPUTPREPARESTAGE_INPUTPADS;
            CGS_ASSERT(lpAllocator != nullptr, "No allocator available\n");
            if (!mControllers.Prepare(lpAllocator))
            {
                return false;
            }
            // fall through
        case E_INPUTPREPARESTAGE_DONE:
            meReleaseStage = E_INPUTRELEASESTAGE_START;
            mePrepareStage = E_INPUTPREPARESTAGE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid Prepare State");
            return false;
    }
}

// X360 0x828EF100. Resumable boot release (pads -> manager), tracking meReleaseStage.
bool InputModule::Release()
{
    switch (meReleaseStage)
    {
        case E_INPUTRELEASESTAGE_START:
            meReleaseStage = E_INPUTRELEASESTAGE_START;
            // fall through
        case E_INPUTRELEASESTAGE_INPUTPADS:
            meReleaseStage = E_INPUTRELEASESTAGE_INPUTPADS;
            if (mControllers.IsPrepared())
            {
                mControllers.SetPrepared(true);
            }
            if (!mControllers.IsPrepared())
            {
                return false;
            }
            // fall through
        case E_INPUTRELEASESTAGE_MANAGER:
            meReleaseStage = E_INPUTRELEASESTAGE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Release())
            {
                return false;
            }
            // fall through
        case E_INPUTRELEASESTAGE_DONE:
            mePrepareStage = E_INPUTPREPARESTAGE_START;
            mOutputBindResultQueue.Clear();     // this+0x1314 miLength = 0
            mOutputUnbindResultQueue.Clear();   // this+0x1380 miLength = 0
            meReleaseStage = E_INPUTRELEASESTAGE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid Prepare State");
            return false;
    }
}

// X360 0x828F8438. Destruct pads, clear both result queues, chain to base.
void InputModule::Destruct()
{
    mControllers.Destruct();
    mOutputBindResultQueue.Clear();     // this+0x1314 miLength = 0
    mOutputUnbindResultQueue.Clear();   // this+0x1380 miLength = 0
    CgsModule::ModuleSingleBuffered::Destruct();
}

// X360 0x828E7098. Drain the post-world pad-mapping request queue: copy each event's 112-byte
// action-mapping payload into the addressed pad (port == -1 broadcasts to all pads).
void InputModule::ProcessMappingQueue(const InputIO::PostWorldInputBuffer* lpPostWorldBuffer)
{
    const s32 liNumEvents = lpPostWorldBuffer->GetPadMappingQueue()->GetLength();

    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        const InputIO::PadMapping& lMapping =
            lpPostWorldBuffer->GetPadMappingQueue()->GetEvent(liEvent);

        // The leading 4 bytes of the record are the target port id; the following 112 bytes are the
        // action-mapping payload copied into the per-pad mapping storage.
        const s32   liPortId  = *reinterpret_cast<const s32*>(&lMapping);
        const void* lpPayload = reinterpret_cast<const u8*>(&lMapping) + 4;

        if (liPortId == -1)
        {
            for (u32 luPort = 0; luPort < CgsInput::KU_NUMBER_OF_PADS; ++luPort)
            {
                mControllers.SetActionMapping(static_cast<s32>(luPort), lpPayload);
            }
        }
        else
        {
            CGS_ASSERT(liPortId < static_cast<s32>(CgsInput::KU_NUMBER_OF_PADS), "Invalid pad ID");
            CGS_ASSERT(liPortId > -1, "Pad Id must be either positive or -1 for all pads");
            mControllers.SetActionMapping(liPortId, lpPayload);
        }
    }
}

}
