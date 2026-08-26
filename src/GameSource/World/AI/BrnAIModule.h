#pragma once

#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"
#include "GameSource/World/AI/BrnAICar.h"                              // AICar (maAICars[35], BY VALUE)
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"   // ResetOnTrackManager
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // the module base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // EventReceiverQueue<1024,16>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle

#include <eathread/eathread_rwmutex.h>

// ⚠️ CLASS-KEY CORRECTED 2026-08-25 (aimodule wave): AllocatorList was forward-declared here
// as a `struct`, but BrnGameDataAllocatorList.h defines it as a `class`. MSVC mangles the class
// key into the parameter type (V vs U), so BrnAIModule.cpp emitted
// ?Prepare@...PEAUAllocatorList... while every caller (compiled against the real header) asked
// for ...PEAVAllocatorList... -- an LNK2019 the moment the body left WorldLinkStubs.cpp, where it
// had always been compiled against the real header. A silent trap for any TU that only ever saw
// this line.
namespace BrnResource { namespace GameDataIO { template <int N> class AllocatorListT; class AllocatorList; } }

namespace CgsModule { struct IOBufferStack; }

namespace BrnAI
{
namespace AIModuleIO { struct OutputBuffer; struct InputBuffer; struct InputBuffer_PostPhysics; }

struct Route;
struct AICar;
struct AISectionsData;

// ⭐⭐ 2026-08-25 (aimodule wave): AIModule NOW DERIVES FROM
// CgsModule::ModuleSingleBuffered, which is what the X360 has done all along --
// AIModule::Construct @0x82794D08 opens with
// `bl CgsModule::ModuleSingleBuffered::Construct(this)`, Prepare stage 1 is
// `ModuleSingleBuffered::Prepare(this)`, Release stage 1 is its Release and Destruct opens with
// its Destruct. The previous model stood a bare `u32 muBaseVTable = 0x820D0D98` in for the base
// and could not call any of them. This is the SAME defect class the crash-exit wave found one
// level up (a MODULE THAT DECLARES NONE OF ITS LIFECYCLE, so every lifecycle call lands on a
// base default that does nothing for it) -- here it was worse, because there was no base to
// bind to at all.
//
// The base's vtable slot order is this tree's (a virtual dtor at slot 0 -- see the banner in
// CgsModule.h). Nothing here indexes a module vtable numerically; the three console indirect
// calls this class reproduces (RouteMapModule slot 16 == its own Prepare(handle), slots 2/3 ==
// its Release/Destruct) are written BY NAME.
class AIModule : public CgsModule::ModuleSingleBuffered
{
public:
        // X360 0x82794D08. Reached by the wired WorldModule::Construct @0x827CF540 fleet cascade.
        void Construct() override;
        // ---- ADDITIVE (attested by WorldModule::DestructWorld @0x827BD0F0) ----
        // Still a boot gate in WorldLinkStubs.cpp -- see the note there.
        void Destruct() override;
        // ---- ADDITIVE (attested by WorldModule::ReleaseWorld @0x827BCE58) ----
        // Still a boot gate in WorldLinkStubs.cpp -- see the note there.
        bool Release() override;

        // X360 0x82798070 -- the 6-stage prepare machine. NOT the base's virtual Prepare()
        // (different signature): the console calls this one directly from WorldModule::Prepare
        // stage 11 and it calls the base's inside its own stage 1.
        bool Prepare( BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                      AIModuleIO::OutputBuffer* lpOutputBuffer );

        // ---- ADDITIVE (WorldModule::Update @0x827D63E8; DWARF BrnAIModule.h
        //      :232/:235). Declaration-only; bodies gated in WorldLinkStubs.cpp
        //      until this module's own TU lands. ----
        void Update( CgsModule::IOBufferStack* lpInputBufferStack,
                     CgsModule::IOBufferStack* lpOutputBufferStack,
                     const AIModuleIO::InputBuffer* lpInputBuffer,
                     AIModuleIO::OutputBuffer* lpOutputBuffer,
                     BrnUpdateSet lUpdateSet );
        void PostPhysicsUpdate( const AIModuleIO::InputBuffer_PostPhysics* lpInputBuffer );

        // ---- THE RESET-ON-TRACK PUMP, AI half (resetpump wave 2026-08-26). Bodies in
        //      BrnAIModule_ResetPump.cpp, alongside the Update slice that calls them. ----

        // X360 @0x8278A7A8 (185 insns). Drain the input buffer's ResetOnTrackRequest queue into
        // the manager's own 35-deep pending array.
        void ProcessRequestInterface( const AIModuleIO::InputBuffer* lpInputBuffer,
                                      AIModuleIO::OutputBuffer* lpOutputBuffer,
                                      BrnUpdateSet lUpdateSet );

        // X360 @0x8279ABB0 (192 insns). Run ResetOnTrackManager::Update for this frame and
        // apply its results to the AI cars.
        void UpdateResetOnTrackManager( AIModuleIO::AIModuleResultInterface* lpResults,
                                        f32 lfTime );

    AIModule();

    // The AI road network, once LoadMapData + RouteMapModule::Prepare have resolved it.
    // Non-null only from prepare stage 4 onward. (The X360's own GetAISectionsData
    // @0x8277BC00 builds a temporary ResourcePtr from mMapDataHandle and returns its
    // memory resource; this is that read, without the temporary's list churn.)
    const AISectionsData* GetLoadedAISectionsData() const;

    // Has the module finished its own prepare machine (stage 5)?
    bool IsPrepared() const { return mePrepareStage == E_PREPARESTAGE_DONE; }

    // DWARF-authoritative nested enum (BrnAIModule.h:83): the multi-frame prepare state
    // machine AIModule::Prepare steps through (advanced by the free post-increment operator++).
    enum EPrepareStage
    {
        E_PREPARESTAGE_START     = 0,
        E_PREPARESTAGE_MANAGER   = 1,
        E_PREPARESTAGE_RESOURCES = 2,
        E_PREPARESTAGE_ROUTEMAP  = 3,
        E_PREPARESTAGE_AICARS    = 4,
        E_PREPARESTAGE_DONE      = 5,
    };

    // The AI module's OWN release stage machine (X360 +294768, four states, read by
    // AIModule::Release @0x8276E270 and seeded to DONE by Construct exactly as
    // CrashModule::Construct seeds its own). Distinct from -- and shadowing nothing of --
    // the base's private EManagerReleaseStage.
    enum EReleaseStage
    {
        E_RELEASESTAGE_START    = 0,
        E_RELEASESTAGE_BASE     = 1,
        E_RELEASESTAGE_ROUTEMAP = 2,
        E_RELEASESTAGE_DONE     = 3,
    };

    // LoadMapData's own sub-state machine (X360 +294772, five states; the default arm's
    // baked text is "AIModule::LoadMapData in a weird state", BrnAIModule.cpp:491).
    enum ELoadMapDataStage
    {
        E_LOADMAPDATA_REQUEST_BUNDLE  = 0,
        E_LOADMAPDATA_AWAIT_BUNDLE    = 1,
        E_LOADMAPDATA_REQUEST_MAPDATA = 2,
        E_LOADMAPDATA_AWAIT_MAPDATA   = 3,
        E_LOADMAPDATA_DONE            = 4,
    };

    // Declared-only accessors needed by RouteMapDebugComponent::RenderHUD (its own TU). Bodies live
    // in the AIModule TU. X360 offsets (read in RouteMapDebugComponent::RenderHUD @0x827940A0):
    //   - the master route is a Route embedded at this+0x46B60 (289632); GetMasterRoute returns it.
    //   - the default A* distance-function enum is at this+0x424A8 (271528); the X360 indexes the
    //     KAPC_ASTAR_DISTANCE_FUNCTION_NAMES table with it -- GetDefaultDistanceFunctionName returns
    //     the matching name string.
    const Route* GetMasterRoute() const;                  // this + 0x46B60
    const char*  GetDefaultDistanceFunctionName() const;  // KAPC_..._NAMES[*(this + 0x424A8)]

    // Declared-only accessors needed by RaceBalancingDebugComponent (its own TU). Bodies live in the
    // AIModule TU (X360 standalone symbols BrnAI::AIModule::GetAICar @0x8276A5C8-region and
    // GetAISectionsData). GetAICar returns the per-slot AI car (FindAICar @0x827676F8 walks indices
    // 0..34); GetAISectionsData returns the loaded section data the speed read-outs query.
    AICar*          GetAICar(u32 luIndex) const;          // BrnAI::AIModule::GetAICar(this, index)
    AISectionsData* GetAISectionsData() const;            // BrnAI::AIModule::GetAISectionsData(this)

    // The current AI difficulty scalar (0..1), read by RaceBalancingDebugComponent::RenderHUD as the
    // float at this+0x4EBA4 (322404). Declared-only; body lives in the AIModule TU.
    f32             GetDifficulty() const;                // *(this + 0x4EBA4)

private:
    // X360 0x82795340 (167 insns) -- stage 2 of Prepare. LoadBundle("AI.dat") then acquire
    // "WorldMapData"; drains the reply into mMapDataHandle.
    bool LoadMapData( AIModuleIO::OutputBuffer* lpOutputBuffer );

    struct RouteRequestSlot
    {
        s32 miRouteId;
        u8  mPad0[32];
    };

    EA::Thread::RWMutex mInputMutex;
    EA::Thread::RWMutex mOutputMutex;
    u8                  mPad0[252520];
    s32                 miActiveRouteRequest;
    u8                  mPad1[18088];
    s32                 miPendingRouteRequest;
    u8                  mPad2[12];
    u32                 muRouteRequestVTable;
    u8                  mPad3[56];
    RouteRequestSlot    maRouteRequestSlots[15];
    u8                  mPad4[8];
    u32                 muOpenListVTable;
    u8                  mPad5[68];
    u32                 muClosedListVTable;
    u8                  mPad6[7244];
    u32                 muScratchListVTable;
    u8                  mPad7[7832];
    s32                 miLastRouteId;
    u8                  mPad8[300];
    u32                 muAnchorState;
    u32                 muAnchorPrev;
    u32                 muAnchorNext;
    void*               mpAnchorHead;
    void*               mpAnchorTail;
    void*               mpAnchorCursor;
    u32                 muAnchorFlags;
    u8                  mPad9[452];
    u32                 muAllocatorVTable;
    u8                  mPad10[8420];
    RouteMapModule      mRouteMapModule;
    s32                 miWorldRouteRequest;

    // ================================================================================
    // ---- NAMED MEMBERS (aimodule wave 2026-08-25) ----------------------------------
    // ================================================================================
    // HOST LAYOUT IS NOT OFFSET-FAITHFUL AND NEVER WAS. The pad spine above was derived
    // from console WORD indices; on this LLP64 host EA::Thread::RWMutex, every pointer and
    // every embedded object is a different size, so no member of this class sits at its
    // X360 byte offset. The X360 offset recorded against each member below is therefore the
    // IDENTITY authority (it is what proves WHICH field a console body is touching), not a
    // placement instruction. Nothing outside this class addresses the module by offset --
    // verified by grep for `mAIModule` before the change.
    //
    // These are appended rather than carved out of the pads on purpose: carving would move
    // every existing pad-relative neighbour for no gain, since the offsets are already not
    // reproducible. The pads stay exactly as committed.

    // X360 +294764 (0x47F6C). The 6-stage prepare machine cursor.
    EPrepareStage       mePrepareStage;
    // X360 +294768 (0x47F70). The module's own release machine cursor; Construct seeds it
    // to DONE(3), Prepare's success arm resets it to START(0).
    EReleaseStage       meReleaseStage;
    // X360 +294772 (0x47F74). LoadMapData's sub-stage.
    ELoadMapDataStage   meLoadMapDataStage;

    // X360 +294832 (0x47FB0), backing buffer at +294856 with capacity 1024 / alignment 16
    // (Construct stores `*(this+294848) = 1024`, `*(this+294852) = 16`,
    // `*(this+294832) = this+294856` -- i.e. exactly EventReceiverQueue<1024,16>::Construct,
    // whose base Construct writes {buffer, capacity, alignment} then Clear()s. The three
    // console stores land at base+16 / base+20 / base+0, which is that layout exactly).
    CgsModule::EventReceiverQueue<1024, 16> mResourceReceiverQueue;

    // X360 +295880 (0x483C8). The 8-byte resource handle LoadMapData copies out of the
    // "WorldMapData" acquire reply and Prepare stage 3 hands to RouteMapModule::Prepare
    // (`ld r4, 0(this+295880)` -- ONE 64-bit load of the console's two 32-bit pointers,
    // i.e. the whole ResourceHandle passed by value in a single GPR).
    CgsResource::ResourceHandle mMapDataHandle;

    // X360 +286128 (0x45DB0). The reset-on-track manager. EMBEDDED BY VALUE, and its ONLY
    // construction site in the whole image is this module's Prepare stage 3.
    ResetOnTrackManager mResetOnTrackManager;

    // ⭐⭐⭐ X360 +560 (0x230). THE 35-ENTRY AI-CAR ARRAY (aicar_reset wave 2026-08-26).
    //
    // This is the member AIModule::Prepare stage 3 hands the reset-on-track manager as its
    // `lpaAICars`, and until this wave it handed it a NULL. It is not polish:
    // ResetOnTrackManager::Update walks all 35 entries EVERY frame (`for (i = 0; i < 191520;
    // i += 5472)` -- the console bakes the array's byte extent and the 5472 stride straight into
    // the loop) and ComputeInitialCoordinatesStandard dereferences GetAICar's result on the
    // FIRST request. A null array is an access violation the moment the pump runs.
    //
    // ⭐ THE STRIDE MATCHES BY MEASUREMENT, NOT BY HOPE: sizeof(AICar) == 5472 == 0x1560 on this
    // host, pinned by a static_assert in BrnAICar.h. 35 * 5472 == 191520, the console's own loop
    // bound. If that assert ever fires, GetAICar's console constant must become &maAICars[index].
    //
    // ⛔ IT IS NOT FULLY CONSTRUCTED -- SEE AIModule::Construct. AICar::Construct @0x82792620 is
    // an ARTIST export HOLE (no JSON; it is named only through AIModule::Construct's xrefs_from),
    // so Construct reproduces ONLY the one initialisation the reset-on-track path reads, from the
    // attested AICar::Reset @0x82792800. Everything else is the console's own pre-Construct .bss
    // zero. Flagged there, loudly.
    AICar maAICars[35];

    // X360 +322040 (0x4EB78) / +322044 (0x4EB7C) -- the two player-car cursors AIModule::Update
    // latches at the top of its body and every later leg reads back.
    //
    //   mePlayerActiveRaceCarIndex  <- lpInputBuffer->GetRaceCarAIInterface()
    //                                    ->GetPlayerActiveRaceCarIndex()  (asm 0x8279B5F8)
    //   mePlayerGlobalRaceCarIndex  <- the AI DRIVER chain: only written when the player's slot
    //                                  HAS an AIDriver whose +7529 flag is set, as
    //                                  `driver->mpAICar ? driver->mpAICar->miRaceCarIndex : -1`
    //                                  (asm 0x8279B640..0x8279B6A0).
    //
    // ⛔ [FLAG PC bring-up] NOTHING ON THIS BUILD WRITES THE SECOND ONE. AIModule::GetAIDriver
    // and the eight AIDriver objects are absent (AIDriver::Prepare is Prepare stage 4's parked
    // leg), so the console's writer arm cannot run. Its resting value here is the CONSOLE'S OWN
    // resting value: the module lives in .bss and the console never stores anything else into
    // +322044 on a free-burn drive, so E_GLOBAL_RACE_CAR_INDEX_0 (== 0) is what the console
    // reads too. It feeds only ResetOnTrackManager::Update's two range asserts and its
    // "player sent a non-STANDARD request" tripwire, both of which pass at 0.
    // ⚠️ It is NOT "the player's global race-car index" today, and no code here treats it as
    // one -- the request itself carries the real index (RCEM::SendResetOnTrackRequests writes
    // it), which is what ProcessResetOnTrackRequest actually resolves against.
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;   // X360 +322040
    EGlobalRaceCarIndex mePlayerGlobalRaceCarIndex;   // X360 +322044
};

// Free post-increment over the AI prepare-stage enum (DWARF BrnAIModule.h:417). X360 0x82765A10.
AIModule::EPrepareStage operator++(AIModule::EPrepareStage& leEnumIndex, int);
}
