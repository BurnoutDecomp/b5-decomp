// ============================================================================
// BrnTrafficEntityModule_wQ7_02.cpp  --  wave Q7 / cluster `traffic`, partfile 2 of 2
//
// THE BRING-UP THAT MAKES THE wQ7_01 CONSUMER REACHABLE.
//
// Landing BrnTrafficEntityModule_wQ7_01.cpp alone gives a code-complete but RUNTIME-DEAD
// chain, for two independent reasons that both live in this file:
//
//   B3  mpData @+0x71840 (the ResourcePtr<TrafficData> whose mTrafficLights collection
//       HandlePropModuleRequests hands to the light manager) is NULL, because nothing on
//       this build has ever run TrafficEntityModule::LoadData. That is not merely a gap: on
//       the first frame the leg runs it is an ACCESS VIOLATION -- ResourcePtr::operator->
//       asserts "Can not instance resource pointer - it is NULL" and then returns the null
//       anyway, so TrafficLightGotSmashed receives (TrafficLightCollection*)+0x68, whose own
//       non-null assert PASSES, and GetInstanceIndexForInstanceID dereferences it.
//   B2  meState @+0x300 is E_STATE_STARTING_UP (0) forever: the module storage is
//       zero-initialised, Construct/Prepare are inert gates in WorldLinkStubs.cpp, and the
//       only console writer of E_STATE_RUNNING is EnterRunningState @0x827080E8, reachable
//       only through PostPhysicsUpdate @0x8274E6D0 (581 insns) after Reset @0x8272CDA0
//       (824 insns) -- neither reconstructed.
//
// THIS FILE CONTAINS:
//   * TrafficEntityModule::Prepare  @0x8274A578 (252 insns) -- PARTIAL. Stages 0/2/5 REAL,
//     stages 1/3/4 NAMED one-shot gates (each with a hard reason, below).
//   * TrafficEntityModule::LoadData @0x82746A88 (465 insns) -- PARTIAL. Resource stages 0/1
//     REAL (post LoadTrafficLanes, bind the reply into mpData); stages 2..11 ONE named gate,
//     after which the console's own terminal state (E_RESOURCE_ACQUIRE_COUNT) is entered.
//   * ONE deliberate PC bring-up deviation, the meState latch, spelled out below.
//
// ⚠️⚠️ CONDUCTOR: MOUNT THIS FILE AS A SEPARATE DECISION FROM wQ7_01, AND BOOT-TEST IT.
//   Retiring the Prepare gate replaces `return true` (the world Prepare ladder walks straight
//   past the traffic module today) with a real multi-frame resumable ladder that returns FALSE
//   until the LoadTrafficLanes reply lands. The request/reply round trip IS proven live on
//   this build in the same Prepare context -- BrnTriggerQueryManager_Prepare.cpp:201-249 and
//   BrnDirectorWorldMap.cpp:200-226 both do LoadTrafficLanes(queue, 1, 5) and both bind their
//   handle (BrnGame.log "[TriggerQueryManager] LOADED -- trigger=1 traffic=1", "[WorldMap]
//   LOADED -- traffic=1"), and WorldModule::Prepare's own not-ready path already drains this
//   module's request pipe through the REAL BridgeTrafficResourceRequestsToOutput @0x827AD9D8
//   (BrnWorldModule.cpp:992). But if the reply never arrives on THIS route the world Prepare
//   ladder stalls at stage 7 and the game never finishes loading. The owner of this cluster is
//   NOT the run-time owner and could not drive it. The revert is one line: unmount this file
//   and restore the WorldLinkStubs.cpp Prepare gate. wQ7_01 is unaffected either way.
//
// SOURCES: X360 ARTIST asm+pseudocode (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8274A578.json,
// 0x82746A88.json, 0x827080E8.json, 0x82740220.json) for behaviour; DecFIGS DWARF for shape.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"          // BrnTraffic::TrafficData
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"     // CgsResource::ResourcePtr<T>
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // EventReceiverQueue<4096,16>
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"            // GetGameDataEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint / gxMessageFilterFlags

#include <cstddef>   // size_t

namespace BrnTraffic
{
namespace
{
    // Same opaque-field helper form BrnTrafficEntityModule.cpp:38-48 establishes for this
    // class; see the header banner for why the module object has no reconstructable layout.
    template <typename T>
    inline T& FieldAt(void* lpBase, size_t luByteOffset)
    {
        return *reinterpret_cast<T*>(reinterpret_cast<u8*>(lpBase) + luByteOffset);
    }

    typedef CgsModule::EventReceiverQueue<4096, 16> TrafficReceiverQueue;

    // Is this queue's backing buffer bound yet? `mpBuffer` is PROTECTED on
    // CgsModule::BaseEventReceiverQueue (CgsBaseEventReceiverQueue.h) and that committed
    // shared type exposes no accessor for it -- so it is read through a derived-class
    // pointer-to-member, the one form of protected access the language grants without
    // growing a type this cluster does not own. Zero-initialised module storage has
    // mpBuffer == 0; BaseEventReceiverQueue::Construct binds it to the embedded maBuffer,
    // and Clear() never touches it. This is therefore a PER-OBJECT "already Constructed?"
    // test, not a process-wide one.
    struct ReceiverQueueBinding : public CgsModule::BaseEventReceiverQueue
    {
        static bool IsBound( const CgsModule::BaseEventReceiverQueue& lrQueue )
        {
            u8* CgsModule::BaseEventReceiverQueue::* const lpBufferMember =
                &ReceiverQueueBinding::mpBuffer;
            return (lrQueue.*lpBufferMember) != 0;
        }
    };

    // ---- attested module offsets used by this file -------------------------------------
    // mePrepareStage  (Prepare's own `lwz r11,0x2F0(r26)` switch; Construct `stw 0,0x2F0`).
    const size_t KU_OFFSET_ME_PREPARE_STAGE      = 0x2F0;
    // the word Prepare stage 5 clears (`stw r31,0x2F4`); Construct seeds it with 2.
    const size_t KU_OFFSET_ME_RELEASE_STAGE      = 0x2F4;
    // meResourceStage (DWARF; LoadData's `a1[190]` switch == byte 760).
    const size_t KU_OFFSET_ME_RESOURCE_STAGE     = 0x2F8;
    // meState / meStartingUpState / meRunningState / meRunningStateToUseAfterStartup
    // (DWARF :607-:610; EnterRunningState @0x827080E8 writes all four).
    const size_t KU_OFFSET_ME_STATE                          = 0x300;
    const size_t KU_OFFSET_ME_STARTING_UP_STATE              = 0x304;
    const size_t KU_OFFSET_ME_RUNNING_STATE                  = 0x308;
    const size_t KU_OFFSET_ME_RUNNING_STATE_AFTER_STARTUP    = 0x30C;
    // mReceiverQueue (DWARF :613 `EventReceiverQueue<4096,16>`; Prepare stage 0
    // `addi r3,r26,0x314 ; bl BaseEventReceiverQueue::Clear`; Construct inlines its
    // Construct here -- see the stage-0 comment).
    const size_t KU_OFFSET_RECEIVER_QUEUE        = 0x314;
    // mpData (DWARF :752).
    const size_t KU_OFFSET_DATA                  = 0x71840;
    // The two fields Prepare stage 0 zeroes alongside the queue Clear. Their DWARF names are
    // not pinned by this cluster (they sit in the un-homed run of members after
    // mbPlayingShowtimeMode), so they are addressed by their attested offsets only:
    //   `std r31,0(this+0x713A0)`   -- an 8-byte zero
    //   `stbx r31,this,0x71B32`     -- a 1-byte zero
    const size_t KU_OFFSET_STAGE0_ZERO_QWORD     = 0x713A0;
    const size_t KU_OFFSET_STAGE0_ZERO_BYTE      = 0x71B32;

    // The resource-request ids the console bakes into LoadData case 0 / case 1
    // (`LoadTrafficLanes(&mReceiverQueue, 1, 5)`, `cmpwi r10,0x37`, `cmpwi ...,1`). Same
    // three values, same names, as BrnDirectorWorldMap.cpp:51-52 and
    // BrnTriggerQueryManager_Prepare.cpp:71 -- which is the corroboration that 0x37 is the
    // traffic-lanes response and not some other data reply.
    const s32 KI_DATA_ACQUIRE_REQUEST       = 1;
    const s32 KI_LANE_DATA_POOL_ID          = 5;
    const s32 KI_RESPONSE_GET_TRAFFIC_LANES = 55;   // 0x37

    // PARTIAL-pattern leg gate -- one NAMED one-shot line per console leg with no body in the
    // tree. Same gating condition as the WorldLinkStubs.cpp boot gates.
    // [DIAG] NOT IN THE X360 BINARY.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[Q7-traffic-leg] TrafficEntityModule stage NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    // HOST-LAYOUT DIVERGENCE, PINNED (gotcha 1). mReceiverQueue is placed at the console's
    // +0x314 inside the opaque blob, but on x64 its mpBuffer widens 4->8, so the host object
    // runs 0x314..0x1334 where the console's runs 0x314..0x131C -- it overruns the console
    // positions of whatever follows it by 24 bytes. That is SAFE for everything this cluster
    // touches: every other field used here is either BELOW the queue (+0x2F0..+0x310) or far
    // above it (+0x53790, +0x713A0, +0x71840, +0x71B32, +0x729FC). Pinned so a future grow of
    // the queue cannot silently walk into mTrafficLightManager.
    // (Deliberate divergence inside an opaque blob -- documented, not accommodated.)
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::LoadData  @ 0x82746A88  (465 insns)   *** PARTIAL ***
//
// DWARF :1266  bool LoadData(OutputBuffer_Prepare*)
//
// The module's resource-acquire ladder, a resumable switch on meResourceStage @+0x2F8
// (EResourceAcquireStage, header :503). The console walks twelve stages: traffic lanes,
// the vehicle list, the per-vehicle-type bundles, physics, attribs, wheels. THIS PARTIAL
// LANDS STAGES 0 AND 1 -- the ones that seat mpData -- and then enters the console's OWN
// terminal state (E_RESOURCE_ACQUIRE_COUNT == 12, which case 12 answers with `return true`),
// with one NAMED gate naming the ten skipped stages.
//
// WHY 2..11 ARE GATED AND NOT LANDED: every one of them reaches module storage far beyond the
// modelled opaque blob (the per-vehicle-type ResourcePtr array at `&a1[8*id + 120534]` ==
// +0x75B58.. and the VehicleTypeRuntime array at `&a1[32*id + 121056]` == +0x76380..,
// both past sizeof(mOpaque) == 0x73000) and calls VehicleTypeRuntime::Prepare /
// FindVehicleTypeAttribKey_EXPENSIVE, neither of which has a body. Landing them would be a
// fabricated layout AND an out-of-bounds write.
//
// The traffic-LIGHT data this wave needs is entirely inside the stage-1 payload: the reply
// binds the whole TrafficData resource, and TrafficData::mTrafficLights is a BY-VALUE member
// of it (BrnTrafficDataResourceType.h:75), already relocated by TrafficData::FixUp. Nothing
// in stages 2..11 touches it.
// ============================================================================
bool TrafficEntityModule::LoadData( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    TrafficReceiverQueue& lrReceiverQueue =
        FieldAt<TrafficReceiverQueue>( this, KU_OFFSET_RECEIVER_QUEUE );
    CgsResource::ResourcePtr<TrafficData>& lrData =
        FieldAt< CgsResource::ResourcePtr<TrafficData> >( this, KU_OFFSET_DATA );

    switch ( FieldAt<s32>( this, KU_OFFSET_ME_RESOURCE_STAGE ) )
    {
    case E_RESOURCE_LOAD_BASEDATA_NOT_STARTED:
        // 0x82746Axx case 0: LockForWrite(lpOutput); RequestInterface<4096>::LoadTrafficLanes
        // @0x827468C0 (event id 1, pool 5); UnlockForWrite. Then straight into case 1 -- the
        // console does NOT return here.
        lpOutputBuffer->LockForWrite();
        lpOutputBuffer->GetResourceRequestInterface()->LoadTrafficLanes(
            &lrReceiverQueue, KI_DATA_ACQUIRE_REQUEST, KI_LANE_DATA_POOL_ID );
        lpOutputBuffer->UnlockForWrite();
        // fall through -- the console falls into LABEL_3 on the same tick.

    case E_RESOURCE_LOAD_BASEDATA_REQUESTED:
    {
        // LABEL_3 sets the stage FIRST (it is the fall-through landing point as well as
        // case 1's own entry), then tests the reply count.
        FieldAt<s32>( this, KU_OFFSET_ME_RESOURCE_STAGE ) = E_RESOURCE_LOAD_BASEDATA_REQUESTED;

        if ( lrReceiverQueue.GetCount() != 1 )
        {
            if ( lrReceiverQueue.GetCount() >= 1 )
            {
                // More than one reply queued -- a non-gating tripwire, then bail.
                CGS_ASSERT( lrReceiverQueue.GetCount() < 1,
                            "mReceiverQueue.GetLength() < 1" );          // baked line 1064
            }
            return false;
        }

        // `v13 = mReceiverQueue.miStartOffset + mReceiverQueue.mpBuffer` then `*v13 == 55`
        // (the record's TYPE word) and `v14 = v13 + 8` (the payload). Expressed through the
        // queue's own accessor, exactly as BrnTriggerQueryManager_Prepare.cpp:224 and
        // BrnDirectorWorldMap.cpp:214 already do for the identical reply.
        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        const s32 liType = lrReceiverQueue.GetFirstEvent( &lpEvent, &liSize );

        if ( liType != KI_RESPONSE_GET_TRAFFIC_LANES )
        {
            CGS_ASSERT( liType == KI_RESPONSE_GET_TRAFFIC_LANES,
                        "TrafficEntityModule::LoadData has received a resource with an ID that wasn't requested" ); // line 1051
        }

        const BrnResource::GameDataIO::GetGameDataEvent* lpAcquire =
            static_cast<const BrnResource::GameDataIO::GetGameDataEvent*>( lpEvent );

        CGS_ASSERT( lpAcquire->miEventId == KI_DATA_ACQUIRE_REQUEST,
                    "lpAcquire->GetEventId() == KI_DATA_ACQUIRE_REQUEST" );   // baked line 1055

        // ---- THE SEAT: `CgsResource::BaseResourcePtr::CreateFromHandle(a1+116240, v14+8)`
        // == mpData = lpAcquire->mHandle. The console reads the handle at payload +0x20; the
        // host handle is 16 bytes where the console's is 8, so it is read BY MEMBER.
        lrData = lpAcquire->mHandle;

        {
            // `SetAssetList(&mTrafficCarStreamer @+0x72B58, mpData->muNumVehicleAssets,
            //  mpData->mpaVehicleAssets)` -- ARGUMENT ORDER MEASURED at 0x82746C1C..0x82746C2C:
            // `addis r3,r23,7 ; addi r3,r3,0x2B58` (r3 = &mTrafficCarStreamer), then
            // `lbz r4,0x18(TrafficData)` (r4 = the u8 COUNT) and `mr r5,r31` where r31 came
            // from `lwz r31,0x34(TrafficData)` (r5 = the asset-array POINTER). So the count is
            // the FIRST value argument and the pointer the second.
            // BrnTrafficCarStreamer.h declares no SetAssetList, so calling it would be an
            // unresolved external the per-TU gate cannot see (gotcha 12). The streamer is not
            // on this wave's path.
            static bool sbLogged = false;
            LogMissingLeg( sbLogged,
                "LoadData stage 1 leg TrafficCarStreamer::SetAssetList(mpData->muNumVehicleAssets, "
                "mpData->mpaVehicleAssets) -- no declaration/body in tree" );
        }

        lrReceiverQueue.Clear();

        {
            static bool sbLogged = false;
            LogMissingLeg( sbLogged,
                "LoadData resource stages 2..11 (vehicle list / vehicle bundles / physics / "
                "attribs / wheels) -- they reach module storage past sizeof(mOpaque) and call "
                "VehicleTypeRuntime::Prepare + FindVehicleTypeAttribKey_EXPENSIVE, neither bodied" );
        }

        // The console's own terminal state (LABEL_49 / case 12): stage 12, return true.
        FieldAt<s32>( this, KU_OFFSET_ME_RESOURCE_STAGE ) = E_RESOURCE_ACQUIRE_COUNT;
        return true;
    }

    case E_RESOURCE_ACQUIRE_COUNT:
        // 0x82746xxx case 12 -> LABEL_49: `result = 1; meResourceStage = 12;`
        FieldAt<s32>( this, KU_OFFSET_ME_RESOURCE_STAGE ) = E_RESOURCE_ACQUIRE_COUNT;
        return true;

    default:
        // The console's default arm. Stages 2..11 are unreachable in this partial (stage 1
        // jumps straight to 12), so anything landing here is a genuine "weird state".
        CGS_ASSERT( false, "TrafficEntityModule::LoadData in a weird state" );  // baked line 1320
        return false;
    }
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::Prepare  @ 0x8274A578  (252 insns)     *** PARTIAL ***
//
// DWARF :1079 `virtual bool Prepare(OutputBuffer_Prepare*)` -- non-virtual here (the module is
// an opaque blob with no base sub-object; retyping would change the mangled name and orphan
// BrnWorldModule.cpp:973 and the WorldLinkStubs.cpp gate).
//
// A resumable six-stage ladder on mePrepareStage @+0x2F0 (EPrepareStage, header :486), driven
// once per frame by WorldModule::Prepare stage eWorldPrepareTrafficEntityModule
// (BrnWorldModule.cpp:966-999), which drains this module's request pipe on every FALSE.
//
// REAL: stage 0 (receiver-queue reset + the two zero stores), stage 2 (LoadData), stage 5
// (the done arm), the console's fall-through chaining, and the default assert.
//
// NAMED GATES, each with a hard reason:
//   stage 1  CgsModule::ModuleSingleBuffered::Prepare(this)  -- the in-tree TrafficEntityModule
//            is `u8 mOpaque[0x73000]` with NO base sub-object and NO vtable pointer, so there
//            is no ModuleSingleBuffered to call: a reinterpret_cast would virtual-dispatch
//            CreateInput/OutputDataStructure through a NULL vptr in zero-initialised storage.
//            Landing it needs the module's real base chain, i.e. TrafficEntityModule::Construct
//            @0x82740220 (1368 insns).
//   stage 3  the per-traffic-light-instance rw::collision::BoxVolume + InSceneUpdateInterface::
//            AddDynamicVolume loop. It walks `this + 0x76390` with a 128-byte stride --
//            PAST sizeof(mOpaque) == 0x73000 -- so running it is an out-of-bounds write, and
//            the 0.49f shrink (flt_8200D540, dumped) feeds a VMX bbox path with three
//            lBBoxHalfSize asserts over an un-homed sub-aggregate.
//   stage 4  DebugComponent::Register(*(this + 0x727B0)) + rw::IResourceAllocator::
//            AllocateMemoryResource(GetDebugAllocator(), 2560, 16, 0) stored into
//            `this + 0x7286C`, then an 8-iteration init loop over `this + 0x79388` /
//            `this + 0x7938C` with a 136-byte (0x88) stride -- again past the end of the
//            modelled storage. Debug-UI only.
//            OFFSETS RE-MEASURED FROM THE ASM (0x8274A8A0..0x8274A918), not inherited:
//              0x8274A8A4/A8  `ori r11,r11,0x27B0` + `lwzx r3,r26,r11` -- Register's argument
//                             is the POINTER LOADED FROM this + 0x727B0, not that address.
//              0x8274A8B4..C0 `li r4,0xA00 ; li r5,0x10 ; li r6,0 ; bl AllocateMemoryResource`
//              0x8274A8C8/D8  `ori r10,r11,0x286C` + `stwx r3,r26,r10` -- the allocation
//                             result lands at this + 0x7286C. The neighbouring
//                             `ori r9,r11,0x2870` + `stwx r31,r26,r9` (0x8274A8D0/E8) is a
//                             SEPARATE zero store into this + 0x72870.
//              0x8274A8E0/F0  `ori r29,r10,0x9388` / `ori r30,r10,0x938C` -- the loop walks
//                             this + 0x79388 (the iteration index) and this + 0x7938C (zero),
//                             `mulli r10,r11,0x88` stride, `cmpwi r11,8` bound.
//
// ⚠️ NOTE stage 3 RETURNS FALSE after doing its work (`result = 0; mePrepareStage = 4`), so
//    the console itself always needs at least one extra frame here. The gated form keeps that
//    shape: the ladder still costs the same number of pumps.
// ============================================================================
bool TrafficEntityModule::Prepare( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    // The queue object is placed over the module's raw storage at the attested offset; see
    // the host-layout divergence note in the anonymous namespace above.
    static_assert( KU_OFFSET_RECEIVER_QUEUE + sizeof(TrafficReceiverQueue) < 0x53790,
                   "mReceiverQueue must not reach mTrafficLightManager @+0x53790" );

    TrafficReceiverQueue& lrReceiverQueue =
        FieldAt<TrafficReceiverQueue>( this, KU_OFFSET_RECEIVER_QUEUE );

    switch ( FieldAt<s32>( this, KU_OFFSET_ME_PREPARE_STAGE ) )
    {
    case E_PREPARESTAGE_START:
    {
        // ---- 0x8274A5D4 ------------------------------------------------------------
        // [FLAG PC bring-up] EventReceiverQueue<4096,16>::Construct, RELOCATED, NOT INVENTED.
        // The console runs it inside TrafficEntityModule::Construct @0x82740220 -- measured at
        // 0x827407E0..0x82740844: `addi r11,r31,0x314`, mpBuffer = r11+0x18, miCapacity =
        // 0x1000, miAlignment = 0x10, miCount = 0, then the same alignment math Clear() does.
        // That Construct is an inert gate on this build (WorldLinkStubs.cpp:744-750), so the
        // queue's mpBuffer is NULL and miCapacity 0. Clear() alone (which is all the console's
        // Prepare stage 0 does) does NOT bind the buffer, so the GameData module's reply would
        // memcpy through a NULL pointer. Construct is therefore called here, ONCE, before the
        // console's own Clear.
        //
        // THE GUARD IS PER-OBJECT, NOT A FUNCTION-LOCAL STATIC. The console's Construct runs
        // once per INSTANCE; a process-wide `static bool` would skip it for a second
        // TrafficEntityModule (a re-created WorldModule on a world reload), leaving mpBuffer
        // NULL -- and because Clear() has its own null guard and GetCount() would then stay 0
        // forever, Prepare would return false every frame: a silent load hang instead of the
        // crash this deviation exists to prevent. The queue's own unbound state is the guard
        // (see ReceiverQueueBinding above), so this is idempotent per object.
        // DELETE-WHEN: TrafficEntityModule::Construct @0x82740220 lands for real.
        {
            if ( !ReceiverQueueBinding::IsBound( lrReceiverQueue ) )
            {
                lrReceiverQueue.Construct();

                // The LOG stays one-shot per process -- it is a bring-up tell, not behaviour.
                static bool sbLoggedReceiverQueueConstruct = false;
                if ( !sbLoggedReceiverQueueConstruct
                     && (CgsDev::Message::gxMessageFilterFlags & 1) != 0
                     && CgsDev::Log::gpDebugPrint != 0 )
                {
                    sbLoggedReceiverQueueConstruct = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[Q7-traffic-leg] TrafficEntityModule::Prepare stage 0: "
                           "EventReceiverQueue<4096,16>::Construct relocated here from the gated "
                           "TrafficEntityModule::Construct @0x82740220 [FLAG PC bring-up latch]\n";
                }
            }
        }

        // The console's own stage-0 body.
        lrReceiverQueue.Clear();                                    // BaseEventReceiverQueue::Clear
        FieldAt<u64>( this, KU_OFFSET_STAGE0_ZERO_QWORD ) = 0;      // `std r31,0(this+0x713A0)`
        FieldAt<u8>(  this, KU_OFFSET_STAGE0_ZERO_BYTE )  = 0;      // `stbx r31,this,0x71B32`
        FieldAt<s32>( this, KU_OFFSET_ME_PREPARE_STAGE )  = E_PREPARESTAGE_MANAGER;
    }
    // fall through -- 0x8274A600 is the case-1 entry AND the case-0 fall-through target.

    case E_PREPARESTAGE_MANAGER:
    {
        // 0x8274A604 `bl CgsModule::ModuleSingleBuffered::Prepare` -- gated, see the banner.
        // Treated as "the manager is ready" so the ladder advances; the module's single-buffered
        // input/output DataStructures are unused by every leg this build reaches.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "Prepare stage 1 CgsModule::ModuleSingleBuffered::Prepare(this) -- the module has no "
            "modelled base sub-object/vptr on this build; needs TrafficEntityModule::Construct "
            "@0x82740220" );

        FieldAt<s32>( this, KU_OFFSET_ME_PREPARE_STAGE ) = E_PREPARESTAGE_LOADINGWORLD;
    }
    // fall through -- 0x8274A61C.

    case E_PREPARESTAGE_LOADINGWORLD:
        // 0x8274A624 -- the resource ladder. REAL (partial body above).
        if ( !LoadData( lpOutputBuffer ) )
        {
            return false;
        }
        FieldAt<s32>( this, KU_OFFSET_ME_PREPARE_STAGE ) = E_PREPARESTAGE_VOLUMES;
        // fall through -- 0x8274A63C.

    case E_PREPARESTAGE_VOLUMES:
    {
        // 0x8274A63C..0x8274A880 -- gated, see the banner. The console's lock bracket is kept
        // (it is the buffer contract, not the skipped work) and, like the console, this stage
        // ends the frame: it advances the stage and returns FALSE.
        lpOutputBuffer->LockForWrite();
        {
            static bool sbLogged = false;
            LogMissingLeg( sbLogged,
                "Prepare stage 3 per-traffic-light BoxVolume + InSceneUpdateInterface::"
                "AddDynamicVolume loop -- walks this+0x76390 (past sizeof(mOpaque)==0x73000) "
                "over an un-homed sub-aggregate" );
        }
        lpOutputBuffer->UnlockForWrite();

        FieldAt<s32>( this, KU_OFFSET_ME_PREPARE_STAGE ) = E_PREPARESTAGE_DEBUG;
        return false;                                    // console: `result = 0` at 0x8274A880
    }

    case E_PREPARESTAGE_DEBUG:
    {
        // 0x8274A8A0..0x8274A918 -- gated, see the banner.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "Prepare stage 4 DebugComponent::Register(*(this+0x727B0)) + "
            "AllocateMemoryResource(2560,16,0) -> this+0x7286C + the 8-slot init loop at "
            "this+0x79388/0x7938C (0x88 stride) -- past sizeof(mOpaque)==0x73000; debug UI only" );

        FieldAt<s32>( this, KU_OFFSET_ME_PREPARE_STAGE ) = E_PREPARESTAGE_DONE;
    }
    // fall through -- LABEL_21.

    case E_PREPARESTAGE_DONE:
        // LABEL_21: `result = 1; *(this+0x2F4) = 0;`
        FieldAt<s32>( this, KU_OFFSET_ME_RELEASE_STAGE ) = E_RELEASESTAGE_START;

        // -------------------------------------------------------------------------------
        // [FLAG PC bring-up latch] NOT IN THE X360 BINARY AS A CALL SITE. The four STORES
        // below are TrafficEntityModule::EnterRunningState @0x827080E8 verbatim (measured:
        // `v2 = +0x30C; +0x300 = 1; +0x308 = v2; +0x30C = 0; +0x304 = -1`); only the POSITION
        // is ours, because the console reaches EnterRunningState from PostPhysicsUpdate
        // @0x8274E6D0's starting-up ladder (581 insns) after Reset @0x8272CDA0 (824 insns),
        // neither of which is reconstructed. Without it meState stays E_STATE_STARTING_UP
        // forever and a FAITHFUL PrePhysicsUpdate never reaches HandlePropModuleRequests.
        //
        // EnterRunningState itself is NOT called: its entry assert is
        // `(meState == E_STATE_STARTING_UP) && (meStartingUpState == E_STARTINGUPSTATE_LAST)`
        // (baked line 1721), and meStartingUpState is 0 on a build that never ran the
        // starting-up ladder -- calling it would fire a NEW assert. The stores are what the
        // console does; the precondition is what this build cannot supply. Said out loud
        // rather than papered over.
        //
        // BLAST RADIUS, MEASURED: the ONLY in-tree reader of +0x300 is PrePhysicsUpdate
        // (BrnTrafficEntityModule_wQ7_01.cpp) -- grep shows nothing calls
        // TrafficEntityModule::IsPaused / ShouldBeHollywoodAction /
        // NeedToTakeActionAgainstJunctionFUP anywhere. The observable effect is that the eight
        // PrePhysicsUpdate leg gates each log once, and the prop->traffic light rings get
        // consumed. Nothing else.
        //
        // ORDERING GUARANTEE: this is the LAST thing Prepare does, so the running state can
        // never be latched before LoadData seated mpData -- which is what keeps
        // HandlePropModuleRequests off a NULL ResourcePtr.
        //
        // DELETE-WHEN: TrafficEntityModule::PostPhysicsUpdate @0x8274E6D0 + Reset @0x8272CDA0
        // land and drive EnterRunningState themselves.
        // -------------------------------------------------------------------------------
        {
            static bool sbLatchedRunningState = false;
            if ( !sbLatchedRunningState )
            {
                sbLatchedRunningState = true;

                const s32 liRunningStateToUse =
                    FieldAt<s32>( this, KU_OFFSET_ME_RUNNING_STATE_AFTER_STARTUP );

                FieldAt<s32>( this, KU_OFFSET_ME_STATE )                       = E_STATE_RUNNING;
                FieldAt<s32>( this, KU_OFFSET_ME_RUNNING_STATE )               = liRunningStateToUse;
                FieldAt<s32>( this, KU_OFFSET_ME_RUNNING_STATE_AFTER_STARTUP ) = E_RUNNINGSTATE_NORMAL;
                FieldAt<s32>( this, KU_OFFSET_ME_STARTING_UP_STATE )           = E_STARTINGUPSTATE_INVALID;

                if ( (CgsDev::Message::gxMessageFilterFlags & 1) != 0
                     && CgsDev::Log::gpDebugPrint != 0 )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[Q7-traffic-leg] TrafficEntityModule: meState latched to E_STATE_RUNNING "
                           "(EnterRunningState @0x827080E8 store-set, relocated to the tail of "
                           "Prepare) -- mpData seated="
                        << ( FieldAt< CgsResource::ResourcePtr<TrafficData> >( this, KU_OFFSET_DATA )
                                 .HasMemoryResource() ? 1 : 0 )
                        << " [FLAG PC bring-up latch]\n";
                }
            }
        }
        return true;

    default:
        CGS_ASSERT( false, "0" );                        // baked line 1003
        return false;
    }
}

}
