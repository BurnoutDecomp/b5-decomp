// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO_UpdateOutputBuffer.cpp
//
// Out-of-line bodies for the 46 class:BrnWorldIO::UpdateOutputBuffer accessors the
// X360 build emitted out-of-line. Each body tests the buffer status flag (write-lock
// status>>3&1 / read-lock status>>4&1) then acts on the named member:
//   - const getters assert read-lock  ("Not locked for reading")  and return &member;
//   - mutable getters / Append / Set mutators assert write-lock ("Not locked for
//     writing") and return &member or perform the store-for-store mutation.
// The X360-baked d:\p4 file/line is intentionally not reproduced -- CGS_ASSERT stamps
// __FILE__/__LINE__. Offsets are layout-derived (member-by-name), not hardcoded.
// Sibling file of BrnWorldModuleIO.cpp (the UpdateInputBuffer bodies).
#include "GameSource/World/BrnWorldModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // memcpy

namespace BrnWorldIO
{

// Pin the pointer-invariant X360-proven layout facts. The X360 places
// mVehicleOutputInterface at +16 (GetVehicleOutputInterface @0x823B58D0 returns
// this+16 after the 1-byte IOBuffer status base -- i.e. the first member is
// 16-aligned); no pointer-bearing member precedes it, so the offset holds on this
// LLP64 host too. Every later member sits behind pointer-width-dependent payloads
// (queue mpEvents pointers, serialiser pointer arrays), so their console offsets
// are documented on the member declarations instead of being pinned.
void UpdateOutputBuffer::_AssertLayout()
{
    static_assert(offsetof(UpdateOutputBuffer, mVehicleOutputInterface) == 16, "vehicleOutput@16");
}

// ---- lifecycle -----------------------------------------------------------------

// @ 0x827CA0F8 -- construct the buffer: the IOBuffer base status (X360 `*this = 1` ==
// eStatusConstructed), then the member-wise Construct/Clear chain over the ~25 output
// interfaces/queues (X360 order documented inline).
//
// PARTIAL SLICE (FLAG): the members whose committed types expose Construct/Clear run the
// REAL call; the interface interiors whose Construct/Clear bodies are not committed yet
// (VehicleOutputInterface's three interior queues + head words, the two
// RCEntityActiveRaceCarOutputInterface Clears, TriggerEntity/DirectorVehicle/RaceCarGlobal/
// VehicleManager/ContactSpy/TrafficNetwork/Crash/Deformation interfaces, the
// AICarOutputInterface 35-slot {DIST_MAX, 0x7FFF} splat, and the StatusInterface
// {0,0,0,1,1} seed whose setters are still WorldLinkStubs traps) are covered by the
// leading zero-fill stand-in below [marked deviation] and listed here so each lands with
// its type's own pass. The zero-fill starts at the first member (offset 16, pinned by
// _AssertLayout) so the IOBuffer base/lock state is never touched.
void UpdateOutputBuffer::Construct()
{
    memset(&mVehicleOutputInterface, 0,
           sizeof(UpdateOutputBuffer) - offsetof(UpdateOutputBuffer, mVehicleOutputInterface));

    CgsModule::IOBuffer::Construct();                       // X360 *this = 1

    // -- the committed queue constructs, in the X360 call order --
    mTriggerEntityOutputInterface.Construct();              // X360 VEQ<1024,16> +50816 (Construct+Clear)
    mRouteResponseQueue.Construct();                        // X360 RouteResponse<16> +60440
    mResourceRequestInterface.mRequestQueue.Construct();    // X360 VEQ<4096,16> +146660 (Construct+Clear)
    mResourceRequestInterface.mRequestQueue.Clear();
    mAttribSysVaultRequestInterface.mRequestQueue.Construct(); // X360 VEQ<2048,16> +150832 (Construct+Clear)
    mAttribSysVaultRequestInterface.mRequestQueue.Clear();
    mTrafficTypeResponseQueue.Construct();                  // X360 TrafficTypeResponse<32> +155616
    mSoundWorldLoadInterface.Construct();                   // X360 SoundWorldLoadEvent<25> +169096
    mGameEventQueue.Construct();                            // X360 VEQ<1536,16> +216116
    mPropVFXLocatorQueue.Construct();                       // X360 PropVFXLocatorEvent<10> +169360
    mAudioCarLoadedDataQueue.Construct();                   // X360 AudioCarDataLoadedEvent<16> +142632
    mPropBecamePhysicalEventQueue.Construct();              // X360 PropBecamePhysicalEvent<20> +202960
    mPropUpdateNotificationQueue.Construct();               // X360 PropUpdateNotification<200> +203296
    mGuiEventQueue.Construct();                             // X360 VEQ<32768,16> +170176 (the tail call)

    // X360 zero stores covered by the zero-fill: mPlayerVehicleControls (+150772, 60 B),
    // mEffectsEnvironmentInterface (+169072, 16 B vspltisw), mReplayRequestInterface
    // (+169308, 11 words), mTriangleCacheInterface (+216112, pointer), and
    mbWorldWantsDebugControllerFocus = false;               // X360 +217668
}

// ---- player race-car indices -------------------------------------------------

// X360 0x823B6E58 (:514 R) -- read-locked player global race-car index. The X360 body
// inlines RCEntityGlobalRaceCarOutputInterface::GetPlayerGlobalRaceCarIndex (the
// member word at interface+0x968 plus the "Player car index hasn't been set" != -1
// tripwire, baked file BrnRaceCarEntityModuleOutputInterface.h:1444); reversed here
// into the committed accessor call.
EGlobalRaceCarIndex UpdateOutputBuffer::GetPlayerGlobalRaceCarIndex() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mRaceCarGlobalOutputInterface.GetPlayerGlobalRaceCarIndex();
}

// ---- triangle cache ------------------------------------------------------------

// X360 0x8279BAF8 (:518) -- adopt the vehicle input's triangle-cache interface.
// NO lock-bit test in the binary (the only lock-free mutator in this buffer): a
// pointer-null tripwire on the argument, then the inlined
// TriangleCacheInterface::Append (its own manager-null tripwire + the single
// pointer store into mTriangleCacheInterface, X360 this+216112).
void UpdateOutputBuffer::AppendTriangleCacheInterface(const OutTriangleCacheInterface* lpTriangleCacheInterface)
{
    CGS_ASSERT(lpTriangleCacheInterface != nullptr, "lpTriangleCacheInterface != NULL");
    mTriangleCacheInterface.Append(lpTriangleCacheInterface);
}

// ---- resource request interface -------------------------------------------------

// X360 0x823B5780 (:520 R, "GetResour") -- const resource-request accessor (+146660).
const UpdateOutputBuffer::WorldResourceRequestInterface* UpdateOutputBuffer::GetResourceRequestResourceInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mResourceRequestInterface;
}

// X360 0x827A4440 (:521 W) -- mutable resource-request accessor (+146660).
UpdateOutputBuffer::WorldResourceRequestInterface* UpdateOutputBuffer::GetResourceRequestResourceInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mResourceRequestInterface;
}

// X360 0x827AD0E8 (:522 W) -- merge a source resource-request interface into ours.
// The X360 dispatches CgsModule::VariableEventQueue<4096,16>::Append<4096,16> on the
// interface's leading request queue (the interface IS its queue at offset 0).
void UpdateOutputBuffer::AppendResourceRequestInterface(const WorldResourceRequestInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mResourceRequestInterface.mRequestQueue.Append<KI_WORLD_EVENT_QUEUE_MAX_SIZE, 16>(lpInterface->mRequestQueue);
}

// ---- attrib-sys vault request interface -----------------------------------------

// X360 0x827BCAD0 (:525 W, "GetA") -- mutable attrib-sys vault request accessor (+150832).
UpdateOutputBuffer::AttribSysVaultRequestInterface* UpdateOutputBuffer::GetAttribSysVaultRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mAttribSysVaultRequestInterface;
}

// ---- vehicle output interfaces ---------------------------------------------------

// X360 0x823B58D0 (:528 R, "GetVehicleOut") -- const vehicle-output accessor (+16).
const UpdateOutputBuffer::VehicleOutputInterface* UpdateOutputBuffer::GetVehicleOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleOutputInterface;
}

// X360 0x827A44E8 (:529 W, "GetVehicleOutputInt") -- mutable vehicle-output accessor (+16).
UpdateOutputBuffer::VehicleOutputInterface* UpdateOutputBuffer::GetVehicleOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mVehicleOutputInterface;
}

// X360 0x823B5978 (:531 R, "GetVeh") -- const vehicle-manager-output accessor (+27680).
const UpdateOutputBuffer::VehicleManagerOutputInterface* UpdateOutputBuffer::GetVehicleManagerOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleManagerOutputInterface;
}

// ---- director vehicle input interface --------------------------------------------

// X360 0x823B5A20 (:535 R, "GetDirector") -- const director-vehicle-input accessor (+51856).
const UpdateOutputBuffer::DirectorVehicleInputInterface* UpdateOutputBuffer::GetDirectorVehicleInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDirectorVehicleInputInterface;
}

// X360 0x827AD1A0 (:536 W) -- replace-merge the source's pending new-vehicle events
// (+51856). The X360 inlines BrnDirectorVehicleInputInterface::Append: queue length
// reset (`stw 0, +8`) then the out-of-line EventQueue<NewVehicleEvent,50>::Append.
void UpdateOutputBuffer::SetDirectorVehicleInputInterface(const DirectorVehicleInputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mDirectorVehicleInputInterface.Append(lpInterface);
}

// ---- race-car entity / trigger output interfaces ---------------------------------

// X360 0x827A4638 (:539 W) -- copy the source global race-car interface (+52672).
// The X360 inlines the interface's flat copy as XMemCpy(dst, src, 0x970 == 2416);
// reversed into the committed RCEntityGlobalRaceCarOutputInterface::operator=.
void UpdateOutputBuffer::SetRaceCarGlobalOutputInterface(const RCEntityGlobalOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mRaceCarGlobalOutputInterface = *lpInterface;
}

// X360 0x827A46F0 (:542 W) -- copy the source trigger-entity output (+50816).
// The X360 emits memcpy(dst, src, 0x410 == 1040) == the whole VariableEventQueue<1024,16>
// (the committed TriggerEntityModuleOutputInterface typedef); a plain block assignment.
void UpdateOutputBuffer::SetTriggerEntityOutputInterface(const TriggerEntityModuleOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTriggerEntityOutputInterface = *lpInterface;
}

// X360 0x827A47A8 (:545 W) -- copy the source active-race-car interface (+29856).
// X360 inlines the flat copy as XMemCpy(dst, src, 0x28F0 == 10480); reversed into the
// committed RCEntityActiveRaceCarOutputInterface::operator=.
void UpdateOutputBuffer::SetActiveRaceCarOutputInterface(const RCEntityActiveRaceCarOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mActiveRaceCarOutputInterface = *lpInterface;
}

// X360 0x827A4860 (:548 W) -- copy the source replay active-race-car interface (+40336;
// same XMemCpy(0x28F0) flat copy as :545, on the replay member).
void UpdateOutputBuffer::SetReplayActiveRaceCarOutputInterface(const RCEntityActiveRaceCarOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mReplayActiveRaceCarOutputInterface = *lpInterface;
}

// ---- contact spy ------------------------------------------------------------------

// X360 0x823B5D68 (:550 R, "GetContactSpy") -- const contact-spy accessor (+146656).
const UpdateOutputBuffer::ContactSpyInterface* UpdateOutputBuffer::GetContactSpyInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mContactSpyInterface;
}

// X360 0x827A4918 (:551 W, "GetContactSpyInterf") -- mutable contact-spy accessor (+146656).
UpdateOutputBuffer::ContactSpyInterface* UpdateOutputBuffer::GetContactSpyInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mContactSpyInterface;
}

// ---- AI car output ----------------------------------------------------------------

// X360 0x823B5E10 (:554 R, "GetAICarOutputInt") -- const AI-car-output accessor (+55088).
const UpdateOutputBuffer::AICarOutputInterface* UpdateOutputBuffer::GetAICarOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mAICarOutputInterface;
}

// X360 0x827A49C0 (:555 W) -- copy the source AI-car-output interface (+55088).
// X360 emits memcpy(dst, src, 0x14E8 == 5352) -- the interface's flat (trivially
// copyable) image; a plain assignment reproduces the block copy.
void UpdateOutputBuffer::SetAICarOutputInterface(const AICarOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mAICarOutputInterface = *lpInterface;
}

// ---- player vehicle controls --------------------------------------------------------

// X360 0x823B5EB8 (:557 R) -- const player-vehicle-controls accessor (+150772).
const PlayerVehicleControls* UpdateOutputBuffer::GetPlayerVehicleControls() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mPlayerVehicleControls;
}

// X360 0x827BCB78 (:558 W) -- copy the source player-vehicle-controls (60 bytes) into
// +150772 (memcpy(dst, src, 0x3C) -- same shape as the UpdateInputBuffer setter).
void UpdateOutputBuffer::SetPlayerVehicleControls(const PlayerVehicleControls* lpControls)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    std::memcpy(&mPlayerVehicleControls, lpControls, sizeof(PlayerVehicleControls));
}

// ---- AI route responses -------------------------------------------------------------

// X360 0x827AA5A0 (:561 W) -- merge a source route-response queue into ours (+60440;
// dispatches the committed BaseEventQueue<RouteResponse>::Append).
void UpdateOutputBuffer::AppendRouteResponseQueue(const RouteResponseQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mRouteResponseQueue.Append(*lpQueue);
}

// ---- traffic network / sound / director ----------------------------------------------

// X360 0x823B6008 (:563 R, "G") -- const traffic-network-output accessor (+152896).
const UpdateOutputBuffer::TrafficNetworkOutputInterface* UpdateOutputBuffer::GetTrafficNetworkOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTrafficNetworkOutputInterface;
}

// X360 0x827AD258 (:564 W) -- copy the source traffic-network output (+152896).
// The X360 calls the out-of-line compiler-emitted copy-assignment
// (BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface::operator= @ 0x823C8FD8,
// ledger-truncated to "TrafficNetwor"); a plain assignment reproduces it.
void UpdateOutputBuffer::SetTrafficNetworkOutputInterface(const TrafficNetworkOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTrafficNetworkOutputInterface = *lpInterface;
}

// X360 0x823B60B0 (:566 R, "Get") -- const traffic-sound-output accessor (+153040).
const UpdateOutputBuffer::TrafficSoundOutputInterface* UpdateOutputBuffer::GetTrafficSoundOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTrafficSoundOutputInterface;
}

// X360 0x827A4A78 (:567 W) -- copy the source traffic-sound output (+153040) via the
// out-of-line compiler-emitted copy-assignment (TrafficSoundOutputInterface::operator=
// @ 0x823A7F18, ledger-truncated to "TrafficSoundOut").
void UpdateOutputBuffer::SetTrafficSoundOutputInterface(const TrafficSoundOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTrafficSoundOutputInterface = *lpInterface;
}

// X360 0x823B6158 (:569 R, ledger name fully truncated) -- const traffic-director-output
// accessor (+143040).
const UpdateOutputBuffer::TrafficDirectorOutputInterface* UpdateOutputBuffer::GetTrafficDirectorOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTrafficDirectorOutputInterface;
}

// X360 0x827A8AB0 (:570 W) -- copy the source traffic-director output (+143040).
// The X360 emits the memberwise copy inline: `lhz/sth` of mu16EntityCount (+0) then the
// compiler-emitted Array<TrafficDirectorEntity,32u> copy helper (sub_823B2368) on the
// +16 entity array == this struct's implicit copy-assignment.
void UpdateOutputBuffer::SetTrafficDirectorOutputInterface(const TrafficDirectorOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTrafficDirectorOutputInterface = *lpInterface;
}

// ---- crash network ---------------------------------------------------------------------

// X360 0x823B6200 (:572 R, "GetCrashNetwork") -- const crash-network-output accessor (+156144).
const UpdateOutputBuffer::CrashNetworkOutputInterface* UpdateOutputBuffer::GetCrashNetworkOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mCrashNetworkOutputInterface;
}

// X360 0x827AD310 (:573 W) -- replace-merge the source's pending crashing-traffic updates
// (+156144). The X360 inlines NetworkOutputInterface::Append: queue length reset
// (`stw 0, +8`) then the out-of-line EventQueue<CrashingTrafficUpdateEvent,24>::Append.
void UpdateOutputBuffer::SetCrashNetworkOutputInterface(const CrashNetworkOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mCrashNetworkOutputInterface.Append(lpInterface);
}

// ---- game event queue --------------------------------------------------------------------

// X360 0x823B62A8 (:575 R, "GetGameEv") -- const game-event-queue accessor (+216116).
const UpdateOutputBuffer::GameEventQueue* UpdateOutputBuffer::GetGameEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mGameEventQueue;
}

// X360 0x827A4B30 (:576 W, "GetGameEventQue") -- mutable game-event-queue accessor (+216116).
UpdateOutputBuffer::GameEventQueue* UpdateOutputBuffer::GetGameEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mGameEventQueue;
}

// X360 0x827AD3C8 (:577 W) -- merge a source game-event queue into ours (+216116;
// dispatches VariableEventQueue<1536,16>::Append<1536,16>).
void UpdateOutputBuffer::AppendGameEventQueue(const GameEventQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mGameEventQueue.Append(*lpQueue);
}

// ---- deformation ----------------------------------------------------------------------------

// X360 0x823B6350 (:579 R, "GetDe") -- const deformation-output accessor (+158080).
const UpdateOutputBuffer::DeformationOutputInterface* UpdateOutputBuffer::GetDeformationOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDeformationOutputInterface;
}

// X360 0x827AA658 (:580 W) -- copy the source deformation output (+158080) via the
// hand-written DeformationOutputInterface::operator= (DWARF :154; X360 @ 0x823C8900,
// ledger-truncated to "DeformationOutputI"; bodied by its own TU).
void UpdateOutputBuffer::SetDeformationOutputInterface(const DeformationOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mDeformationOutputInterface = *lpInterface;
}

// ---- traffic type responses -----------------------------------------------------------------

// X360 0x827AA710 (:586 W) -- merge a source traffic-type-response queue into ours (+155616;
// dispatches the committed BaseEventQueue<TrafficTypeResponse>::Append).
void UpdateOutputBuffer::AppendTrafficTypeResponseQueue(const TrafficTypeResponseQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTrafficTypeResponseQueue.Append(*lpQueue);
}

// ---- effects environment --------------------------------------------------------------------

// X360 0x823B64A0 (:588 R, "GetEffectsEnviron") -- const effects-environment accessor (+169072).
const UpdateOutputBuffer::EffectsEnvironmentInterface* UpdateOutputBuffer::GetEffectsEnvironmentInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mEffectsEnvironmentInterface;
}

// X360 0x827BCC30 (:589 W, "GetEffectsEnvironmentIn") -- mutable effects-environment accessor (+169072).
UpdateOutputBuffer::EffectsEnvironmentInterface* UpdateOutputBuffer::GetEffectsEnvironmentInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mEffectsEnvironmentInterface;
}

// ---- world entity status --------------------------------------------------------------------

// X360 0x823B6548 (:591 R, "GetWorldEntitySt") -- const world-entity-status accessor (+169088).
const UpdateOutputBuffer::StatusInterface* UpdateOutputBuffer::GetWorldEntityStatusInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mWorldEntityStatusInterface;
}

// X360 0x827A4BD8 (:592 W) -- copy the source world-entity status (+169088). The X360
// emits the memberwise copy as a 5-iteration byte loop (the interface is exactly the
// five status bools) == this struct's implicit copy-assignment.
void UpdateOutputBuffer::SetWorldEntityStatusInterface(const StatusInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mWorldEntityStatusInterface = *lpInterface;
}

// ---- sound world load -----------------------------------------------------------------------

// X360 0x827AA7C8 (:595 W) -- merge a source sound-world-load queue into ours (+169096;
// dispatches the committed BaseEventQueue<SoundWorldLoadEvent>::Append).
void UpdateOutputBuffer::AppendSoundWorldLoadInterface(const SoundWorldLoadInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mSoundWorldLoadInterface.Append(*lpInterface);
}

// ---- replay requests ------------------------------------------------------------------------

// X360 0x823B6698 (:597 R, "GetReplayRequestIn") -- const replay-request accessor (+169308).
const UpdateOutputBuffer::ReplayRequestInterface* UpdateOutputBuffer::GetReplayRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mReplayRequestInterface;
}

// X360 0x827A4CA8 (:598 W) -- merge the source's registered serialisers into ours
// (+169308; dispatches the committed BrnReplays::ReplayIO::RequestInterface::Append
// @ 0x823A6868).
void UpdateOutputBuffer::AppendReplayRequestInterface(const ReplayRequestInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mReplayRequestInterface.Append(lpInterface);
}

// ---- prop VFX locators ----------------------------------------------------------------------

// X360 0x827AA880 (:600 W) -- replace-merge the source's pending prop-VFX locator events
// (+169360): queue length reset (`stw 0, +8` == Clear) then the out-of-line
// EventQueue<PropVFXLocatorEvent,10>::Append. Both ops are on our own member queue,
// so the clear+merge lives here (contrast the director/crash interfaces, whose queues
// are private behind their interface types).
void UpdateOutputBuffer::SetPropVFXLocatorQueue(const PropVFXLocatorQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mPropVFXLocatorQueue.Clear();
    mPropVFXLocatorQueue.Append(*lpQueue);
}

// ---- prop physical/update notifications ------------------------------------------------------

// X360 0x827AA938 (:614 W) -- merge a source prop-became-physical queue into ours (+202960;
// dispatches the committed BaseEventQueue<PropBecamePhysicalEvent>::Append).
void UpdateOutputBuffer::AppendPropBecamePhysicalEventQueue(const PropBecamePhysicalEventQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mPropBecamePhysicalEventQueue.Append(*lpQueue);
}

// X360 0x827AA9F0 (:618 W) -- merge a source prop-update-notification queue into ours
// (+203296; dispatches the committed BaseEventQueue<PropUpdateNotification>::Append
// @ 0x823C3DA8).
void UpdateOutputBuffer::AppendPropUpdateNotificationQueue(const PropUpdateNotificationQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mPropUpdateNotificationQueue.Append(*lpQueue);
}

// ---- ADDITIVE GROW (wave33: consumer-side const read accessors) --------------------------------

// X360 0x823B5828 (:524 R, IDA "UpdateOutputBuffer") -- const attrib-sys vault request accessor
// (+150832). The read-lock const overload paired with the committed mutable :525 getter (0x827BCAD0).
// (Slot-mapping corrected by the verifier: this body returns &mAttribSysVaultRequestInterface, not
// the trigger-entity member.)
const UpdateOutputBuffer::AttribSysVaultRequestInterface* UpdateOutputBuffer::GetAttribSysVaultRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mAttribSysVaultRequestInterface;
}

// X360 0x823B5AC8 (:538 R, IDA "UpdateO") -- const global race-car output accessor (+52672).
const UpdateOutputBuffer::RCEntityGlobalOutputInterface* UpdateOutputBuffer::GetRaceCarGlobalOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mRaceCarGlobalOutputInterface;
}

// X360 0x823B5B70 (:541 R, IDA "UpdateOut") -- const trigger-entity output accessor (+50816). The
// mutable-side operation for this member is the committed Set (:542 W, 0x827A46F0); this is the
// const read accessor. (Slot-mapping corrected by the verifier: this body returns
// &mTriggerEntityOutputInterface, not the attrib-sys member.)
const UpdateOutputBuffer::TriggerEntityModuleOutputInterface* UpdateOutputBuffer::GetTriggerEntityOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTriggerEntityOutputInterface;
}

// X360 0x823B5F60 (:560 R, IDA "Update") -- const AI route-response queue accessor (+60440).
const UpdateOutputBuffer::RouteResponseQueue* UpdateOutputBuffer::GetRouteResponseQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mRouteResponseQueue;
}

// X360 0x823B65F0 (:694 R, IDA "Upd") -- const sound-world-load-interface accessor (+169096).
const UpdateOutputBuffer::SoundWorldLoadInterface* UpdateOutputBuffer::GetSoundWorldLoadInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mSoundWorldLoadInterface;
}

// X360 0x823B6740 (:712 R, IDA "U") -- const prop-VFX-locator-queue accessor (+169360).
const UpdateOutputBuffer::PropVFXLocatorQueue* UpdateOutputBuffer::GetPropVFXLocatorQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mPropVFXLocatorQueue;
}

// X360 0x823B6A88 (:739 R, IDA "Up") -- const prop-update-notification-queue accessor (+203296).
const UpdateOutputBuffer::PropUpdateNotificationQueue* UpdateOutputBuffer::GetPropUpdateNotificationQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mPropUpdateNotificationQueue;
}

}   // namespace BrnWorldIO
