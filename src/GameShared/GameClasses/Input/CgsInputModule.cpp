// CgsInput::InputModule -- reconstructed from BURNOUT_X360_ARTIST.XEX. See CgsInputModule.h
// for the class banner. This TU bodies the ledger functions:
//   InputModule::Construct             @0x828F83D0   (FX-RUMBLE3 2026-09-24)
//   InputModule::Prepare               @0x828EEFD8
//   InputModule::Release               @0x828EF100
//   InputModule::Destruct              @0x828F8438
//   InputModule::PreWorldUpdate        @0x82903328   (FX-RUMBLE3 2026-09-24, export hole)
//   InputModule::ProcessRumbleRequests @0x828FFE50   (FX-RUMBLE3 2026-09-24, export hole)
//   InputModule::ProcessMappingQueue   @0x828E7098
//
// (The previous file-scope InputModule() constructor here was a fabricated pre-DWARF stub over a
// stale "SubModule"/RWMutex class shape; it is deleted -- the DWARF class has no such ctor.)
//
// ⭐ FX-RUMBLE3 2026-09-24 (crash parity G10-D4): this TU is now the one the game module runs.
// BrnGameModule.hpp used to declare its own empty `class InputModule : public ModuleSingleBuffered {}`
// (an ODR stub), so mInputModule had no PreWorldUpdate to call and the rumble requests the game
// state queued had no consumer at all; the header now includes this class instead.

#include "CgsInputModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h"   // FLAG PC seat: InputPadsPC::UpdatePadDevices (PreWorldUpdate)
#include "rw/rwcore_structs.h"

namespace CgsInput
{

// X360 0x828F83D0, store for store:
//   bl ModuleSingleBuffered::Construct ; stb 1,4(this) (mbIsNewModule) ; bl InputPads::Construct(+0x230) ;
//   bl EventQueue<BindResult,8>::Construct(+0x130C) ; bl EventQueue<UnBindResult,8>::Construct(+0x1378) ;
//   stw 0 -> +0x1314 / +0x1380 (both lengths) ; stw 0 -> +0x228 (mePrepareStage START) ;
//   stw 3 -> +0x22C (meReleaseStage DONE) ; stw 0 -> +0x13E4 (mpAllocator)
void InputModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();
    mbIsNewModule = true;
    mControllers.Construct();
    mOutputBindResultQueue.Construct();
    mOutputUnbindResultQueue.Construct();
    mOutputBindResultQueue.Clear();
    mOutputUnbindResultQueue.Clear();
    mePrepareStage = E_INPUTPREPARESTAGE_START;
    meReleaseStage = E_INPUTRELEASESTAGE_DONE;
    mpAllocator    = nullptr;
}

// X360 0x82903328 (export hole; ppcdis), the input module's vtable slot 17 (0x820CF648 + 0x44):
//   0x82903348  lpOutputBuffer->LockForWrite()          0x82903350  lpPreWorldInputBuffer->LockForRead()
//   0x82903360  ProcessRumbleRequests(pre, lbUpdatePads) 0x82903368  lpPreWorldInputBuffer->UnlockForRead()
//   0x8290336C..0x82903380  if (lbUpdatePads) InputPads::Update(&mControllers, lpOutputBuffer) @0x828F8690
//   0x82903384..0x829033A0  GetBindResultQueue() (0x828E6D28)->Append(mOutputBindResultQueue) ;
//                           GetUnbindResultQueue() (0x828E6DD0)->Append(mOutputUnbindResultQueue)
//   0x829033A4..0x829033B0  both module queue lengths 0 ; lpOutputBuffer->UnlockForWrite()
// r4 / r5 (the two IO buffer stacks) are never read.
void InputModule::PreWorldUpdate(CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                 CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                 const InputIO::PreWorldInputBuffer* lpPreWorldInputBuffer,
                                 InputIO::OutputBuffer* lpOutputBuffer,
                                 bool lbUpdatePads)
{
    lpOutputBuffer->LockForWrite();
    lpPreWorldInputBuffer->LockForRead();
    ProcessRumbleRequests(lpPreWorldInputBuffer, lbUpdatePads);
    lpPreWorldInputBuffer->UnlockForRead();

    if (lbUpdatePads)
    {
        // FLAG PC-platform leaf (the console's `mControllers.Update(lpOutputBuffer)`, InputPads::Update
        // @0x828F8690 -- the ManagerX360 device scan + bind, DeviceX360Pad::Update, then the per-port
        // fill through gaDefaultGameInputMapping). That body is not reconstructed. On this build its
        // FILL half is InputPadsPC::UpdatePlayer0, which BrnGameModule's GUI leg runs later in the same
        // sub-step (one of the controller bridges' documented stand-ins); its DEVICE half -- the only
        // part the rumble engine reads (DeviceX360Pad::IsConnected / mePort / meType) -- is this
        // PC-leaf scan of the host XInput pad. DELETE-WHEN InputPads::Update lands: call it here.
        InputPadsPC::UpdatePadDevices(&mControllers);
    }

    lpOutputBuffer->GetBindResultQueue()->Append(mOutputBindResultQueue);
    lpOutputBuffer->GetUnbindResultQueue()->Append(mOutputUnbindResultQueue);
    mOutputBindResultQueue.Clear();
    mOutputUnbindResultQueue.Clear();
    lpOutputBuffer->UnlockForWrite();
}

// X360 0x828FFE50 (export hole; ppcdis). Every queue is re-fetched through its read-lock accessor
// inside its loop, the length once before it (the console's order: jolt, stop, play, volume):
//   0x828FFE70..0x828FFEAC  GetPlayJoltEffectEventQueue (0x828E6740), GetEvent const (0x828DFEB0,
//                           :272/:274/:275) -> mControllers.PlayJoltEvent (0x828DC128)
//   0x828FFEB0..0x828FFEF0  GetStopRumbleEffectEventQueue (0x828E6938), GetEvent const (0x828DFF58)
//                           -> StopRumbleEvent (0x828DC3C0)
//   0x828FFEF4..0x828FFFC4  GetPlayRumbleEffectEventQueue (0x828E67E8), the const GetEvent inlined with
//                           its three asserts (0x110/0x112/0x113) -> PlayRumbleEvent (0x828DC208)
//   0x828FFFC8..0x82900078  GetChangeVolumeRumbleEffectEventQueue (0x828E6890), inlined the same way
//                           -> ChangeVolumeRumbleEvent (0x828DC318)
//   0x8290007C..0x82900094  lfTimeStep = GetTimerStatusInt()'s GAME status [+8] * [+4]
//                           (TimerStatus::GetCurrentTimeStep, mfTimeStepMultiplier * mfBaseTimeStep)
//   0x82900084..0x829000AC  pause = buffer.mbPauseRumble (+0x394) || !lbUpdatePads
//   0x829000B0..0x829000F0  InputPads::UpdateRumble inlined (DWARF cpp :688): the three flags, then
//                           UpdatePadRumble(port, maPorts[port], lfTimeStep) for the four ports
void InputModule::ProcessRumbleRequests(const InputIO::PreWorldInputBuffer* lpPreWorldInputBuffer, bool lbUpdatePads)
{
    s32 liNumEvents = lpPreWorldInputBuffer->GetPlayJoltEffectEventQueue()->GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        mControllers.PlayJoltEvent(lpPreWorldInputBuffer->GetPlayJoltEffectEventQueue()->GetEvent(liEvent));
    }

    liNumEvents = lpPreWorldInputBuffer->GetStopRumbleEffectEventQueue()->GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        mControllers.StopRumbleEvent(lpPreWorldInputBuffer->GetStopRumbleEffectEventQueue()->GetEvent(liEvent));
    }

    liNumEvents = lpPreWorldInputBuffer->GetPlayRumbleEffectEventQueue()->GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        mControllers.PlayRumbleEvent(lpPreWorldInputBuffer->GetPlayRumbleEffectEventQueue()->GetEvent(liEvent));
    }

    liNumEvents = lpPreWorldInputBuffer->GetChangeVolumeRumbleEffectEventQueue()->GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        mControllers.ChangeVolumeRumbleEvent(
            lpPreWorldInputBuffer->GetChangeVolumeRumbleEffectEventQueue()->GetEvent(liEvent));
    }

    const f32 lfTimeStep =
        lpPreWorldInputBuffer->GetTimerStatusInt()->GetGameTimerStatus()->GetCurrentTimeStep();
    mControllers.UpdateRumble(lfTimeStep,
                              lpPreWorldInputBuffer->GetRumblePaused() || !lbUpdatePads,
                              lpPreWorldInputBuffer->GetRumbleEnabled(),
                              lpPreWorldInputBuffer->GetWheelForceFeedbackEnabled());
}

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
