#pragma once

// BrnTraffic::BrnTrafficIO IO buffers (TrafficEntityModule shared IO). Reconstructed from the
// DecFIGS DWARF (GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h)
// with member OFFSETS pinned by the X360 retail XEX.
//
// This header homes the traffic module's per-stage IO buffers: the InputBuffer_* buffers the
// world/physics/race-car bridges fill for the traffic module to drain, and the OutputBuffer_*
// buffers the traffic module produces. Members whose owning homes live in other TUs are embedded
// by-value using their real committed types where available, or as correctly-sized opaque stand-
// ins (flagged) sized to the X360 spans so the X360-pinned member offsets are exact.

#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"   // CgsSceneManager::EntityId
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"   // RequestInterface<4096>
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                                   // CgsModule::IOBuffer
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h"    // TrafficAIInterface
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h" // TrafficSoundOutputInterface (OutputBuffer_PostPhysics @+3632)

// ---- InputBuffer_PreScene member/parameter type homes ----
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"                 // CgsSystem::TimerStatusInterface
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntity{Active,Global}RaceCarOutputInterface, RCEntityPlayerResetInterface
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"           // TrafficNetworkInputInterface

// ---- InputBuffer_PrePhysics member type homes ----
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                 // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"            // SceneManagerIO::PotentialContact (80B)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h"   // SceneManagerIO::OutOverlapPair (24B)

// ---- InputBuffer_PostScene / InputBuffer_PostPhysics member type homes ----
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h"     // BrnWorld::CrashIO::TrafficOutputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"        // VehicleOutputInterface, VehicleManagerOutputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"         // OutputBuffer_PrePhysics::VehicleInputInterface (retyped 2026-08-10)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEffectsInputInterface.h"  // OutputBuffer_PrePhysics::VehicleEffectsInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"   // OutputBuffer_PrePhysics::VehicleDriverInputInterface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // DeformationOutputInterfaceForEntityModules
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"                       // ContactSpyInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                          // CgsModule::VariableEventQueue<N,16>
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h" // BrnWorld::PropEntityIO::PropToTrafficInterface (InputBuffer_PrePhysics member, retyped 2026-08-19)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"            // CgsSceneManager::SceneManagerIO::InSceneUpdateInterface (canonical home; the two OutputBuffer scene seats, relaid 2026-08-19)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h" // TrafficDirectorOutputInterface (OutputBuffer_PostPhysics @+6208)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h"      // TrafficTypeResponse (OutputBuffer_PostPhysics @+830144)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h"  // TrafficToRaceCarInterface_PreScene (OutputBuffer_PreScene @+818784; typedef'd, was a padding fork)
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface (OutputBuffer_PreScene @+819328)

// ---- OutputBuffer_PreDispatch member type home ----
// BrnTraffic::VehicleRenderInfo is stored BY VALUE inside an Array<...,64>, so a
// forward declaration cannot serve (AGENTS.md permits one only for pointer/
// reference-only use) and the complete type must come in. Its committed home is
// BrnTrafficEntityModule.h (which carries it with the X360 attestation of its
// 12-byte stride and the two offsetof pins BrnWorldModule.cpp holds on it); the
// DWARF's own home for it is BrnTrafficVehicle.h:159, so if that record ever
// MOVES there this include should narrow to BrnTrafficVehicle.h. VERIFIED NOT A
// CYCLE: nothing reachable from BrnTrafficEntityModule.h includes this IO header
// (walked its full 60-header transitive closure).
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"  // BrnTraffic::VehicleRenderInfo

// ---- InputBuffer_Dispatch member type homes (pointer slots) ----
// BrnBlobbyShadowBuffer and BrnSubmissionInterface are NESTED classes, so they cannot be
// forward-declared -- the real headers come in, exactly as the sibling race-car IO header
// (BrnRaceCarEntityModuleIO.h:80/:85) and prop IO header already do.
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"   // BrnBlobbyShadowManager::BrnBlobbyShadowBuffer
#include "GameSource/Graphics/BrnCoronaManager.h"         // BrnCoronaManager::BrnSubmissionInterface
// Pointer-only uses in InputBuffer_Dispatch (AGENTS.md forward-declaration exception (b) --
// the same treatment the sibling BrnWorld::PropEntityIO::InputBuffer_Dispatch already uses):
// including CgsDispatcher.h / BrnShadowMap.h here would pull the whole renderer + shadow-
// cascade tail into every includer of this IO header (and this header is reached from
// BrnWorldModule.h, i.e. from almost everywhere).
namespace CgsGraphics { class DispatchFrame; }
namespace BrnWorld { struct ShadowMap; }

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // ============================================================================
    // InputBuffer_Dispatch  (ADDITIVE GROW: WorldBridgeWorldModuleToEntityModules_Render TU)
    // ============================================================================
    // The traffic-entity module's generate-dispatch-lists INPUT buffer
    // (WorldModule::BridgeWorldModuleToEntityModules_Render fills it via the four setters;
    // BrnTraffic::TrafficEntityModule::GenerateDispatchLists drains it via the four getters).
    // Mirrors the near-identical sibling BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists.
    //
    // LAYOUT (X360 accessor offsets -- PROVENANCE, see the POINTER WIDTH note below):
    //   base    CgsModule::IOBuffer                     (1-byte status FlagSet8; +1.. pad)
    //   +0x8014 u32 muDispatchFrame                     dispatch frame
    //   +0x8018 u32 muBlobbyShadowBuffer                blobby-shadow buffer
    //   +0x801C u32 muCoronaSubmissionInterface         corona-submission interface
    //   +0x8020 u32 muShadowMap                         shadow map
    //
    // ⭐ POINTER WIDTH CORRECTED 2026-08-19 (wave Q6, cluster C2 -- the render-dispatch bridge).
    // All four slots were modelled as `u32` handles because the X360 accessors are a bare
    // `lwzx`/`stwx` of a 32-bit word. They are NOT handles: on the X360 a pointer IS 32 bits, and
    // the one and only producer -- WorldModule::BridgeWorldModuleToEntityModules_Render
    // @0x827ABE28, now bodied in Bridges/WorldBridgeToEntityModules.cpp -- stores real pointers
    // into every one of them:
    //   SetDispatchFrame(r4 = DispatchInputBuffer::GetDispatchFrame()            @0x827A4EB0)
    //   SetBlobbyShadowBuffer(r4 = DispatchInputBuffer::GetBlobbyShadowBuffer()  @0x827A42F0)
    //   SetCoronaSubmissionInterface(r4 = ...GetCoronaSubmissionInterface()      @0x827A4398)
    //   SetShadowMap(r4 = `addis r27,r25,0x5E; addi r27,r27,0x26E0` == &WorldModule::mShadowMap)
    // Those four producer accessors already carry the real pointer types
    // (BrnWorldModuleIO_DispatchInputBuffer.h:80/:111/:115), and BOTH sibling dispatch input
    // buffers were widened already (PropEntityIO::InputBuffer_Dispatch 2026-08-12/2026-08-18,
    // RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists). Keeping them u32 on x64
    // truncates a host pointer -- AGENTS.md gotcha 1, the recurring project bug.
    // The console byte offsets above are PROVENANCE ONLY now; every access goes by name.
    //
    // FLAG (opaque interior): the dispatch-list payload from base up to +0x8014 is the buffer's
    // other (unrecovered) contents and stays modelled as correctly-sized opaque storage, so the
    // four fields still sit at the tail in the console's order. The host pointers are 8-byte
    // aligned, so the tail block starts at 0x8018 rather than 0x8014 -- deliberate, and pinned
    // by _AssertLayout as a HOST fact (nothing serialises this buffer; it is a pure runtime IO
    // object, exactly like its already-widened prop sibling).
    class InputBuffer_Dispatch : public CgsModule::IOBuffer
    {
    public:
        // X360 0x827120D8: read-lock; return the dispatch frame (console word @this+0x8014).
        CgsGraphics::DispatchFrame* GetDispatchFrame() const;

        // ADDITIVE (WorldModule::GenerateDispatchLists @0x827D1CE8 seeds the
        // frustum result through this pipe -- X360 accessor sub_827BB138;
        // VariableEventQueue<32768,16>, the same shape as the sibling buffers).
        CgsModule::VariableEventQueue<32768, 16>* GetSceneResultQueue();
        // X360 0x827A0EC0: write-lock; set the dispatch frame (console word @this+0x8014).
        void SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame);
        // X360 0x82712188 (Hex-Rays "G"): read-lock; return the blobby-shadow buffer (this+0x8018).
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* GetBlobbyShadowBuffer() const;
        // X360 0x827A0F70: write-lock; set the blobby-shadow buffer (this+0x8018).
        void SetBlobbyShadowBuffer(BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBlobbyShadowBuffer);
        // X360 0x82712238 (Hex-Rays "GetCor"): read-lock; return the corona-submission interface (this+0x801C).
        BrnCoronaManager::BrnSubmissionInterface* GetCoronaSubmissionInterface() const;
        // X360 0x827A1020: write-lock; set the corona-submission interface (this+0x801C).
        void SetCoronaSubmissionInterface(BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface);
        // X360 0x827122E8: read-lock; return the shadow map (console word @this+0x8020).
        BrnWorld::ShadowMap* GetShadowMap() const;
        // X360 0x827A10D0: write-lock; set the shadow map (console word @this+0x8020).
        void SetShadowMap(BrnWorld::ShadowMap* lpShadowMap);

        // ⭐ ADDED 2026-08-15 (IO-buffer zero-fill removal audit). X360 0x8275CF40 -- this
        // Construct was never written, so `CreateIOBuffer<InputBuffer_Dispatch>` ran only the
        // inherited CgsModule::IOBuffer::Construct and the four handle words below arrived
        // holding the previous IO-stack tenant's bytes (the old PC CreateIOBuffer<T>
        // value-initialised the buffer, which is the only reason that was invisible).
        // Body in BrnTrafficEntityModuleIO_InputBuffer_Dispatch.cpp.
        void Construct();   // X360 0x8275CF40

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the four pointer slots sit contiguously at the
        // tail (console +0x8014..+0x8020). The preceding dispatch-list payload is folded into
        // correctly-sized opaque storage: 0x8014 - 1 accounts for the base's 1-byte status (see FLAG).
        // On the host the four widened pointers push the block to 0x8018..0x8030 -- deliberate;
        // see the POINTER WIDTH note above.
        u8                                             maPayloadAndPad[0x8014 - 1];       // +0x0001..+0x8013 (status pad + payload)
        CgsGraphics::DispatchFrame*                    mpDispatchFrame;                   // console +0x8014
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* mpBlobbyShadowBuffer;              // console +0x8018
        BrnCoronaManager::BrnSubmissionInterface*      mpCoronaSubmissionInterface;       // console +0x801C
        BrnWorld::ShadowMap*                           mpShadowMap;                       // console +0x8020
    };

    // ============================================================================
    // InputBuffer_PreScene  (ADDITIVE GROW: full DWARF layout, BrnTrafficEntityModuleIO.h:143)
    // ============================================================================
    // The traffic module's pre-scene input buffer. Grown from the earlier minimal 2-setter slice
    // to the full DWARF layout with REAL committed member types, so the batch setter bodies get
    // exact X360 offsets. WorldModule::BridgeInputToEntityModules fills the timer + network members;
    // WorldModule::BridgeRaceCarModuleToTrafficModule_PreScene fills the two race-car interfaces.
    class InputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarOutputInterface;   // DWARF :85
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface GlobalRaceCarOutputInterface;   // DWARF :86

        // X360 0x82710B30 -- read-lock; returns &mTimerStatusInterface (this + 4).
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const;                                       // DWARF :150
        // X360 0x8279FAD8 -- write-lock; field-copies the source timer status into +4 (operator=).
        void SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface);                  // DWARF :151
        // ⭐⭐ ADDITIVE 2026-08-21 (wave T1 round 2, cluster R2A). THE GETTER THAT UNBLOCKS
        // TrafficEntityModule::PreSceneUpdate's WAITING_FOR_PLAYER -> POPULATING transition
        // (round 1's blocker B2). X360 0x82710BD8, IDA-unnamed (`sub_82710BD8`), identified
        // beyond doubt by its own baked assert: read-lock test `extrwi r11,r11,1,27`, then
        // FireAssert("Not locked for reading\n", "...trafficentitymodule\\BrnTrafficEntity
        // ModuleIO.h", 157), then `addi r3, r28, 0x40` -- i.e. it returns THIS BUFFER'S
        // this+0x40, which is exactly where mActiveRaceCarOutputInterface sits (@64, pinned
        // by _AssertLayout below and by the 0x8279FBE8 setter). DWARF declares it at :153,
        // one line above the :154 setter, and its body's assert lands at :157 -- the same
        // decl/body offset the sibling GetTimerStatusInterface (:150 / assert 0x82710B30)
        // shows. Its four X360 callers are PreSceneUpdate @0x8274A968,
        // GenerateNearbyParkedTrafficOutput @0x8271FA18,
        // GeneratePotentialLeapedAndStompedCarsOutput @0x8271F298 and
        // UpdateCollidableVehicles @0x827302C8 -- every one of them a PreScene-buffer reader.
        const ActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const;                                  // DWARF :153
        // X360 0x8279FBE8 -- write-lock; flat 10480B member copy into +64.
        void SetActiveRaceCarOutputInterface(const ActiveRaceCarOutputInterface* lpInterface);                        // DWARF :154
        // X360 0x8279FCA0 -- write-lock; flat 2416B member copy into +10544.
        void SetGlobalRaceCarOutputInterface(const GlobalRaceCarOutputInterface* lpInterface);                        // DWARF :157
        // X360 0x827ACD28 -- write-lock; clear+append the hull queue, copy mbDiverged into +12960.
        void SetTrafficNetworkInputInterface(const TrafficNetworkInputInterface* lpTrafficNetworkInputInterface);     // DWARF :160

        // ADDITIVE (WorldModule::EntityModulePreSceneUpdate @0x827BD1F0): the spine's
        // raw time-of-day store into +13072 under the write lock, expressed as the
        // named member set (source = EnvironmentManager::mfCurrTimeOfDay).
        void SetTimeOfDaySeconds(f32 lfSeconds)
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            mfTimeOfDay_Seconds = lfSeconds;
        }

        static void _AssertLayout();

    private:
        CgsSystem::TimerStatusInterface mTimerStatusInterface;          // DWARF :167  @ +4     (48B)
        ActiveRaceCarOutputInterface    mActiveRaceCarOutputInterface;  // DWARF :168  @ +64    (10480B)
        GlobalRaceCarOutputInterface    mGlobalRaceCarOutputInterface;  // DWARF :169  @ +10544 (2416B)
        TrafficNetworkInputInterface    mTrafficNetworkInputInterface;  // DWARF :170  @ +12960
        f32                             mfTimeOfDay_Seconds;            // DWARF :171
    };

    // ============================================================================
    // InputBuffer_PostScene  (ADDITIVE GROW: DWARF BrnTrafficEntityModuleIO.h:221)
    // ============================================================================
    // The traffic module's post-scene input buffer. WorldModule::BridgeCrashModuleToTrafficModule_
    // PostScene fills the crash-traffic output interface; BridgeRaceCarModuleToTrafficModule_PreScene
    // fills the active-race-car interface.
    //
    // X360-pinned offsets:
    //   mCrashTrafficOutputInterface  (TrafficOutputInterface)               @ 8     (8-aligned after status)
    //   mActiveRaceCarOutputInterface (RCEntityActiveRaceCarOutputInterface) @ 1648  (0x670; 16-aligned)
    //   mRaceCarToTrafficInterface    (RaceCarToTrafficInterface)            follows
    //
    // FLAG (opaque interior): RaceCarToTrafficInterface's concrete home is not yet in-tree, so it is
    // modelled as a 1-byte opaque placeholder (this batch never touches its interior; the two pinned
    // offsets that matter -- @8 and @1648 -- are both asserted in the .cpp bodies). Adopt the named
    // type additively when its home lands.
    class InputBuffer_PostScene : public CgsModule::IOBuffer
    {
    public:
        typedef BrnWorld::CrashIO::TrafficOutputInterface                             CrashTrafficOutputInterface; // DWARF :250
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarOutputInterface; // :240
        // RaceCarToTrafficInterface home not yet reconstructed -- opaque placeholder (see FLAG).
        struct RaceCarToTrafficInterface { u8 muDUMMY; };                                                          // :241

        const CrashTrafficOutputInterface* GetCrashTrafficOutputInterface() const;     // :228 (0x82710F20)
        void SetCrashTrafficOutputInterface(const CrashTrafficOutputInterface*);        // :229 (0x827ACDE8)
        const ActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const;   // :231
        void SetActiveRaceCarOutputInterface(const ActiveRaceCarOutputInterface*);      // :232 (0x8279FEA8)
        const RaceCarToTrafficInterface* GetRaceCarToTrafficInterface() const;         // :234 (0x82711070) read-lock; return &mRaceCarToTrafficInterface (this+12128)

    private:
        CrashTrafficOutputInterface   mCrashTrafficOutputInterface;    // :239 @8
        ActiveRaceCarOutputInterface  mActiveRaceCarOutputInterface;   // :240 @1648 (0x670)
        RaceCarToTrafficInterface     mRaceCarToTrafficInterface;      // :241
    };

    // ============================================================================
    // InputBuffer_PrePhysics  (ADDITIVE GROW: DWARF BrnTrafficEntityModuleIO.h:330)
    // ============================================================================
    // The traffic module's pre-physics input buffer. Member SEQUENCE + NAMES from the DecFIGS DWARF
    // (:330-364); byte OFFSETS pinned by the three X360 setter bodies (0x827A9DE0 / 0x827A9E98 /
    // 0x827A0158):
    //   +0        IOBuffer status flag (1-byte FlagSet base)
    //   +16       mPotentialContactQueue  EventQueue<PotentialContact,2048>  (addi this,0x10)
    //   +163872   mOverlapPairsQueue      EventQueue<OutOverlapPair,128>     (this+0x28020)
    //   +166960   mSceneResultQueue       OutSceneQueryResultsQueue<32768> (not set here; opaque pad)
    //   +199744   mPlayerResetInterface   RCEntityPlayerResetInterface (32B) (this+0x30C40)
    //   +199776   mPropToTrafficInterface PropToTrafficInterface (not set here; opaque placeholder)
    class InputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
    public:
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048> PotentialContactQueue; // :91
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>    OverlapPairsQueue;     // :92
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityPlayerResetInterface                  RCEntityPlayerResetInterface; // :104

        // ⭐⭐ RETYPED 2026-08-19 (wave Q6 cluster C3) -- the one-line unpark of
        // WorldModule::BridgePropModuleToTrafficModule_PrePhysics @0x827AEA70, i.e. of "a
        // smashed traffic light tells the traffic system to change".
        //
        // This used to be `struct PropToTrafficInterface { u8 muDUMMY; };` with the note "its
        // concrete home is not yet in-tree". That note was STALE: the home is
        // BrnWorld::PropEntityIO::PropToTrafficInterface (BrnPropToTrafficInterface.h), the PROP
        // side of this very bridge has held it BY VALUE since 2026-08-12
        // (BrnPropEntityModuleIO.h, OutputBuffer_PrePhysics::PropToTrafficInterfaceStorage), and
        // the console's own InputBuffer_PrePhysics::Construct @0x827615F8 builds THIS member's
        // two queues at +0x30C60 / +0x30CEC (== +199776 / +199916) -- the 140-byte gap between
        // them is exactly sizeof(EventQueue<TrafficLightKnockDownEvent,32>) == 12 + 32*4, which
        // is what identifies the two calls as this interface's two members. So the seat IS this
        // type, X360-attested, not inferred.
        //
        // ⚠ WHY A 1-BYTE SEAT WAS A BUG AND NOT MERELY A GAP: the bridge Clear()s and Append()s
        // both of those queues. Over a 1-byte placeholder that is a ~140-byte write past the end
        // of the member (into mSceneResultQueue's tail on the console layout), with the ring's
        // own miLength read out of foreign memory -- the same class of live corruption the AI
        // and traffic scene seats were parked for. Retyping is the fix; casting was never an option.
        typedef BrnWorld::PropEntityIO::PropToTrafficInterface PropToTrafficInterface;   // :299

        // @ 0x827615F8 (DWARF :274) -- the buffer bring-up CreateIOBuffer<T> runs. LANDED 2026-08-19
        // (wave Q5 round-3 integration): without it the base IOBuffer::Construct ran instead and
        // the first BridgeSceneContactsToTrafficModule_PrePhysics (real since round 3) died on
        // 'mpEvents != NULL' inside SetOverlapPairsQueue -- the never-Constructed-queue IO trap.
        void  Construct();
        const PotentialContactQueue* GetPotentialContactQueue() const;                           // :277
        void  SetPotentialContactQueue(const PotentialContactQueue* lpPotentialContactQueue);    // :278 W (0x827A9DE0)

        const OverlapPairsQueue* GetOverlapPairsQueue() const;                                   // :280
        void  SetOverlapPairsQueue(const OverlapPairsQueue* lpOverlapPairsQueue);                // :281 W (0x827A9E98)

        const RCEntityPlayerResetInterface* GetPlayerResetInterface() const;                     // :283
        void  SetPlayerResetInterface(const RCEntityPlayerResetInterface* lpPlayerResetInterface); // :284 W (0x827A0158)

        const PropToTrafficInterface* GetPropToTrafficInterface() const;                         // :289 (0x827113B8) read-lock; &member (this+199776)
        PropToTrafficInterface*       GetPropToTrafficInterface();                               // :290 (0x827A02D0) write-lock; &member (this+199776)

        static void _AssertLayout();

    private:
        PotentialContactQueue        mPotentialContactQueue;                                     // :294 (offset 16)
        OverlapPairsQueue            mOverlapPairsQueue;                                          // :295 (offset 163872)
        // ⭐ RETYPED 2026-08-19 (wave Q6 cluster C3), SIZE-NEUTRAL. This was
        // `unsigned char maSceneResultQueue[199744 - 166960]` -- a 32,784-byte opaque span that
        // nothing could Construct, so the console's third Construct leg
        // (VariableEventQueue<32768,16>::Construct(this+0x28C30)) had no expression and the
        // queue's mbIsConstructed/miFirstEventOffset stayed at whatever the IO stack's previous
        // tenant left (CreateIOBuffer stopped zero-filling in the 2026-08-15 perf pass).
        // VariableEventQueue<BUFSIZE,ALIGN> is POINTER-FREE (bool + char[BUFSIZE] + three s32),
        // so its host sizeof is BUFSIZE+16 == 32,784 == exactly the span it replaces: the
        // following members do not move. Naming the type is what lets Construct build it.
        CgsModule::VariableEventQueue<32768, 16> mSceneResultQueue;                              // :296 (offset 166960)
        RCEntityPlayerResetInterface mPlayerResetInterface;                                       // :298 (offset 199744, 32B)
        PropToTrafficInterface       mPropToTrafficInterface;                                     // :299 (offset 199776, real type)
    };

    // ============================================================================
    // InputBuffer_PostPhysics  (ADDITIVE GROW: DWARF BrnTrafficEntityModuleIO.h:459)
    // ============================================================================
    // The traffic module's post-physics input buffer. The physics/race-car bridges publish per-
    // frame snapshots via WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics (and
    // BridgeRaceCarModuleToTrafficModule_PreScene for the active-race-car member), calling the five
    // write-lock setters below (all real out-of-line X360 symbols). Each tests the IOBuffer write-
    // lock bit, fires the non-gating "Not locked for writing" assert, then copies the source into
    // the matching embedded member.
    //
    // MEMBER LAYOUT authoritative from the DecFIGS DWARF (:459-499, NAMES all members); the X360
    // accessor/store offsets pin their console positions:
    //   +0x10    (16)     mVehicleOutputInterface                     (:370)
    //   +0xEC30  (60464)  mVehicleManagerOutputInterface              (:372)
    //   +0x128C0 (75968)  mActiveRaceCarOutputInterface               (:374; X360 XMemCpy 0x28F0 == 10480)
    //   +0x151B0 (86448)  mDeformationOutputInterfaceForEntityModules (:375)
    //   +0x19EF0 (106224) mContactSpyInterface                        (:376; single-word copy)
    //
    // Offsets are NOT static_asserted: the SIMD/queue aggregates widen on the 64-bit host so their
    // host sizes do not reproduce the 32-bit X360 offsets (same reason the sibling RaceCar
    // InputBuffer_PostPhysics pins no member offsets). The two members this batch does not access
    // (mSceneResultQueue, mGameActionQueue) are modelled as correctly-sized opaque-by-value stand-
    // ins sized to the X360 spans. Adopt the named queue types additively when their homes land.
    class InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
    public:
        typedef BrnPhysics::Vehicle::VehicleOutputInterface        VehicleOutputInterface;        // :82
        typedef BrnPhysics::Vehicle::VehicleManagerOutputInterface VehicleManagerOutputInterface; // :83
        typedef BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules DeformationOutputInterfaceForEntityModules; // :106
        typedef BrnPhysics::ContactSpy::ContactSpyInterface        ContactSpyInterface;           // :108
        // :374 spells the member type as InputBuffer_PreScene::ActiveRaceCarOutputInterface, itself a
        // typedef for RCEntityActiveRaceCarOutputInterface (DWARF :144). Use the real type directly.
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarOutputInterface;

        // ⛔ RETYPED 2026-08-01 (BridgeGameStateToWorld wave), SIZE UNCHANGED (13328 == the X360
        // span == sizeof(VariableEventQueue<13312,16>) on the host too -- the type is pointer-free).
        // WorldModule::BridgeActionsToTrafficModule (X360 0x827ABFF0) already reinterpret_cast this
        // slot to VariableEventQueue<13312,16> and AddEvents into it for 22 game-action ids; as an
        // opaque blob the queue could never be Constructed, so the first delivered action fired
        // "Not Constructed" (CgsVariableEventQueue.h:454 / :728) -- MEASURED, the moment
        // BridgeGameStateToWorld started feeding the world's queue. Naming the type lets Construct()
        // below build it. The typedef keeps the accessors' signatures identical.
        typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueStorage; // :373 span +0xEC30+sizeof(VMOI) .. +0x128C0

        // See the physics sibling: the X360 CreateIOBuffer<T> template runs T::Construct after the
        // stack alloc; the PC template placement-news only, so WorldModule::Update /
        // UpdateForBootUpVideo call this explicitly. It used to resolve to the base
        // CgsModule::IOBuffer::Construct (status byte only).
        void Construct();

        // X360 0x827A9F50 (:347): write-lock; mVehicleOutputInterface = *src.
        void SetVehicleOutputInterface(const VehicleOutputInterface* lpVehicleOutputInterface);
        // X360 0x827AA000 (:353): write-lock; mVehicleManagerOutputInterface = *src.
        void SetVehicleManagerOutputInterface(const VehicleManagerOutputInterface* lpVehicleManagerOutputInterface);
        // ⭐⭐ ADDITIVE 2026-08-21 (wave T1 round 2, cluster R2A). THE GETTER THAT UNBLOCKS
        // TrafficEntityModule::UpdateRaceCarHulls (round 1's blocker B1) AND PostPhysicsUpdate's
        // local-player position/direction refresh. X360 0x82711850, IDA-unnamed
        // (`sub_82711850`): read-lock test, FireAssert("Not locked for reading\n",
        // "...BrnTrafficEntityModuleIO.h", 362), then `addis r3,r28,1 ; addi r3,r3,0x28C0`
        // == this + 0x128C0 == 75968 == mActiveRaceCarOutputInterface's console offset (the
        // one the 0x827A06C0 setter writes). DWARF declares it at :358, one line above the
        // :359 setter, and the body's assert is :362 -- the same decl/body pairing as every
        // other getter in this buffer (GetVehicleOutputInterface :346 / GetGameActionQueue
        // :355 / ...). Distinct from InputBuffer_PreScene's :153 getter above: different
        // buffer, different member offset, different assert line.
        const ActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const;   // :358 (0x82711850)
        // X360 0x827A06C0 (:359): write-lock; mActiveRaceCarOutputInterface = *src (X360 XMemCpy 0x28F0 == 10480).
        void SetActiveRaceCarOutputInterface(const ActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface);
        // X360 0x827AA0B8 (:362): write-lock; mDeformationOutputInterfaceForEntityModules = *src.
        void SetDeformationOutputInterfaceForEntityModules(const DeformationOutputInterfaceForEntityModules* lpDeformationOutputInterface);
        // X360 0x827A0778 (:366): write-lock; mContactSpyInterface = *src (single-word copy).
        void SetContactSpyInterface(const ContactSpyInterface* lpContactSpyInterface);

        // Read/write getters (bodies in BrnTrafficEntityModuleIO_InputBuffer_Getters.cpp).
        const VehicleOutputInterface*        GetVehicleOutputInterface() const;        // :346 (0x827115B0) &member (this+16)
        const VehicleManagerOutputInterface* GetVehicleManagerOutputInterface() const; // :352 (0x82711700) &member (this+60464)
        const GameActionQueueStorage*        GetGameActionQueue() const;               // :355 (0x827117A8) read-lock; &member (this+62640)
        GameActionQueueStorage*              GetGameActionQueue();                      // :356 (0x827A0618) write-lock; &member (this+62640)

    private:
        // Opaque-by-value stand-in for the scene-result queue member this batch does not touch,
        // sized to the X360 span between the pinned real members (host offsets are not asserted).
        struct SceneResultQueueStorage { unsigned char maReserved[32784]; }; // :371 span +0x10+sizeof(VOI) .. +0xEC30

        VehicleOutputInterface                     mVehicleOutputInterface;                       // :370  X360 +0x10
        SceneResultQueueStorage                    mSceneResultQueue;                             // :371
        VehicleManagerOutputInterface              mVehicleManagerOutputInterface;                // :372  X360 +0xEC30
        GameActionQueueStorage                     mGameActionQueue;                              // :373
        ActiveRaceCarOutputInterface               mActiveRaceCarOutputInterface;                 // :374  X360 +0x128C0 (10480 B)
        DeformationOutputInterfaceForEntityModules mDeformationOutputInterfaceForEntityModules;   // :375  X360 +0x151B0
        ContactSpyInterface                        mContactSpyInterface;                          // :376  X360 +0x19EF0
    };

    // ============================================================================
    // OutputBuffer_PostScene  (DWARF :291; X360 Construct @ 0x82761830)
    // ============================================================================
    // X360 member offsets (from Construct @ 0x82761830 store displacements):
    //   +0      IOBuffer status flag (*a1 = 1)
    //   +4      mSceneCoarseQueryQueue  (VariableEventQueue<16384,16>::Construct(a1+4))
    //   +16416  mTrafficAIInterface     (GetTrafficAIInterface returns a1+16416; count zeroed)
    //   +61488  mTrafficToRaceCarInterface_PostScene (this trailing interface is the post-scene
    //           traffic->race-car interface)
    //
    // mSceneCoarseQueryQueue is the SceneManager coarse-query input queue
    // (InputBuffer_Query::InSmCoarseQueryQueue == InCoarseQueryQueue<16384>, a
    // VariableEventQueue<16384,16> subclass that adds NO data members). The X360 places it at
    // offset 4 (4-aligned, right after the 1-byte IOBuffer status), so it is modelled as a
    // 4-ALIGNED 16400-byte sized blob -- NOT the alignas(16) SceneCoarseQueryQueue slice, which
    // would force it to offset 16. mTrafficAIInterface (alignas 16) then lands at 16416 (16400
    // queue ends at 16404, padded up to the next 16-boundary), matching the X360.
    struct OutputBuffer_PostScene : public CgsModule::IOBuffer
    {
        // 4-aligned sized blob for the coarse-query queue (sizeof(VariableEventQueue<16384,16>)
        // == 1 + 16384 + 12 -> round to 4 == 16400). The full queue layout/methods belong to
        // the SceneCoarseQueryQueue TU; this buffer only takes &mSceneCoarseQueryQueue.
        struct SceneCoarseQueryQueue { unsigned char maReserved[16400]; };

        // DWARF :187 -- the trailing traffic->race-car post-scene interface. The DWARF spells
        // it as a 1-byte placeholder (muDUMMY); the X360 zeroes it in Construct. Modelled as the
        // DWARF 1-byte struct (this TU only takes its address, never its interior).
        struct TrafficToRaceCarInterface_PostScene { u8 muDUMMY; };

        void Construct();                                                                  // :249
        const SceneCoarseQueryQueue* GetSceneCoarseQueryQueue() const;                     // :252
        SceneCoarseQueryQueue*       GetSceneCoarseQueryQueue();                            // :253
        const TrafficAIInterface*    GetTrafficAIInterface() const;                        // :255
        TrafficAIInterface*          GetTrafficAIInterface();                              // :256 W (0x827111C0)
        const TrafficToRaceCarInterface_PostScene* GetTrafficToRaceCarInterface_PostScene() const; // :258
        TrafficToRaceCarInterface_PostScene*       GetTrafficToRaceCarInterface_PostScene();        // :259

    private:
        SceneCoarseQueryQueue               mSceneCoarseQueryQueue;                // :263 (offset 4)
        TrafficAIInterface                  mTrafficAIInterface;                   // :264 (offset 16416)
        TrafficToRaceCarInterface_PostScene mTrafficToRaceCarInterface_PostScene;  // :265
    };

    // ============================================================================
    // OutputBuffer_PostPhysics  (DWARF BrnTrafficEntityModuleIO.h :459+, post-physics output buffer)
    // ============================================================================
    // The post-physics output buffer the traffic module produces and WorldModule::
    // BridgeEntityModulesToOutput_PostPhysics drains.
    //
    // ⭐⭐ RELAID OUT 2026-08-19 (wave Q6 cluster C3) FROM ITS OWN Construct. Every member below
    // is named and placed by BrnTrafficIO::OutputBuffer_PostPhysics::Construct @0x82761908,
    // recovered this cluster by a targeted headless IDA run on a private .i64 copy
    // (scratchpad/waveQ6/ida_bridges/) -- 0x82761908 has NO per-address JSON under
    // .ida-exports/, an export-run gap, not a missing function; its name comes from
    // CreateIOBuffer<OutputBuffer_PostPhysics> @0x827B79D0's own xrefs_from.
    // Construct, store for store:
    //   stb 1,0(this)                                          IOBuffer status
    //   CrashIO::TrafficInputInterface::Construct     this+8        mCrashTrafficInputInterface
    //   TrafficNetworkOutputInterface::Construct      this+0xDA0    mNetworkInterface        (3488)
    //   sth 0,0xE30(this)                             this+0xE30    mTrafficSoundOutputInterface (3632)
    //   stw 0,0x2650(this)                            (inside mTrafficDirectorOutputInterface)
    //   VariableEventQueue<1536,16>::Construct        this+0x2660   mGameEventQueue          (9824)
    //   InSceneUpdateInterface::Construct             this+0x2C70   mSceneInputInterface     (11376) ⭐
    //   VariableEventQueue<4096,16>::Construct+Clear  this+0xCAB90  mResourceRequestInterface(830672)
    //   11x stw 0 stride 4                            this+0xCFCE0  mInterfaceAt834784 (44 B, X360-only)
    //   EventQueue<TrafficTypeResponse,32>::Construct this+0xCAAC0  mTrafficTypeResponseQueue(830144)
    //   VariableEventQueue<32768,16>::Construct       this+0xCFD0C  mGuiEventQueue           (834828)
    // and CreateIOBuffer allocates 0xD3D20 == 867,616 bytes, which closes the last span exactly
    // (867616 - 834828 == 32,788 == sizeof(VariableEventQueue<32768,16>) rounded to 4).
    //
    // X360 ACCESSOR MAP (read = bit 4 IsBufferLockedForReading; write = bit 3):
    //   +8      mCrashTrafficInputInterface      read 0x827A0830 (baked 391) / write 0x82711A48 (392)
    //   +3488   mNetworkInterface                read 0x827A08D8 (baked 394)
    //   +3632   mTrafficSoundOutputInterface     read 0x827A0980 (baked 397) / write 0x82711B98 (398)
    //   +6208   mTrafficDirectorOutputInterface  read 0x827A0A28 (baked 400)
    //   +9824   mGameEventQueue                  read 0x827A0AD0 (baked 403) / write 0x82711CE8 (404)
    //   +834784 mInterfaceAt834784               read 0x827A0B78 (baked 406) / write 0x82711D90 (407)
    //   +11376  mSceneInputInterface             read 0x827A0C20 (baked 409)                   ⭐
    //   +830144 mTrafficTypeResponseQueue        read 0x827A0CC8 (baked 412)
    //   +830672 mResourceRequestInterface        read 0x827A0D70 (baked 415)
    //   +834828 mGuiEventQueue                   read 0x827A0E18 (baked 418) / write 0x82712030 (419)
    //
    // ⛔ THE DEFECT THIS FIXES, AND WHY THE OLD MAP LOOKED RIGHT. The tree modelled
    // `mSceneInputInterface` at +834784 as a 44-byte `SceneInputInterfaceStorage`, on the strength
    // of the DecFIGS declaration-line ladder: the read accessors bake source lines
    // 391/394/397/400/403/406/409/412/415/418 (step 3, i.e. const+non-const pairs) and DecFIGS
    // declares GetSceneInputInterface at :402, which is baked 406 under this header's uniform +4
    // skew. But the X360 header carries ONE accessor pair DecFIGS does not (for the +834784
    // member), which displaces every later slot by one: the real GetSceneInputInterface is baked
    // 409 -> +11376. Two independent witnesses settle it against the line ladder:
    //   (a) Construct calls InSceneUpdateInterface::Construct on this+11376, and that aggregate is
    //       the only InSceneUpdateInterface-typed member the DWARF gives this class;
    //   (b) 11376 + 818768 (the console sizeof(InSceneUpdateInterface)) == 830144 EXACTLY, the
    //       next member -- while +834784 has only 44 bytes before +834828.
    // Appending a 44-byte span reinterpreted as the 25-queue aggregate made
    // InSceneUpdateInterface::Append read 25 mpEvents/miLength pairs out of foreign memory, which
    // is why WorldBridgeEntityModulesToScene.cpp's post-physics traffic leg was parked rather than
    // cast. That leg is LIVE as of this wave.
    //
    // FLAG (foreign types / opaque interior): mTrafficDirectorOutputInterface is the real committed
    // SharedIO type; mInterfaceAt834784 is the X360-only 44-byte member DecFIGS has no counterpart
    // for and is named by offset role, not invented. NOTHING here pins a host byte offset: several
    // members are queue aggregates whose host header is 16 bytes where the console's is 12, so host
    // offsets legitimately exceed the console ones. Parity is by NAMED MEMBER + SEQUENCE, and the
    // members are in the console's ascending-offset order, which is also the DWARF's member order
    // (:416..:424) with the one X360-only insertion.
    class OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
    public:
        // DWARF :416 / :94 -- the crash module's traffic input interface (real committed type).
        typedef BrnWorld::CrashIO::TrafficInputInterface CrashTrafficInputInterface;
        // DWARF :420 -- Construct proves the shape (VariableEventQueue<1536,16> @ +9824).
        typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;
        // DWARF :421 spells the member `OutputBuffer_Prepare::SceneInputInterface`, which is the
        // scene manager's InSceneUpdateInterface -- Construct-attested at +11376.
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface;
        // DWARF :422 / BrnTrafficTypeInterface.h:51.
        typedef CgsModule::EventQueue<TrafficTypeResponse, 32> TrafficTypeResponseQueue;
        // DWARF :423 -- the same 4096-slot request pipe OutputBuffer_Prepare owns.
        typedef BrnResource::GameDataIO::RequestInterface<4096> ResourceRequestInterface;
        // DWARF :424 -- Construct proves the shape (VariableEventQueue<32768,16> @ +834828),
        // the same instantiation every other GuiEventInputQueue in the tree is.
        typedef CgsModule::VariableEventQueue<32768, 16> GuiEventInputQueue;
        // X360-ONLY member at +834828-44: 44 bytes, zeroed by Construct as 11 words, no DecFIGS
        // counterpart and no recovered type. Named by offset role (the in-file precedent), NOT
        // invented -- it used to carry the fabricated name "TrafficTypeResponseFactory".
        struct InterfaceAt834784 { unsigned char maReserved[44]; };

        // @0x82761908 -- run by CreateIOBuffer<OutputBuffer_PostPhysics> @0x827B79D0.
        void Construct();

        // +8   read/write mCrashTrafficInputInterface.
        const CrashTrafficInputInterface* GetCrashTrafficInputInterface() const;  // 0x827A0830 (baked 391)
        CrashTrafficInputInterface*       GetCrashTrafficInputInterface();        // 0x82711A48 (baked 392)
        // +3488 read mNetworkInterface.
        const TrafficNetworkOutputInterface* GetNetworkInterface() const;         // 0x827A08D8 (baked 394)
        // +3632 read/write mTrafficSoundOutputInterface (real SharedIO type).
        const TrafficSoundOutputInterface* GetTrafficSoundOutputInterface() const; // 0x827A0980 (baked 397)
        TrafficSoundOutputInterface*       GetTrafficSoundOutputInterface();       // 0x82711B98 (baked 398)
        // +6208 read mTrafficDirectorOutputInterface.
        const TrafficDirectorOutputInterface* GetTrafficDirectorOutputInterface() const; // 0x827A0A28 (baked 400)
        // +9824 read/write mGameEventQueue.
        const GameEventQueue* GetGameEventQueue() const;                          // 0x827A0AD0 (baked 403)
        GameEventQueue*       GetGameEventQueue();                                // 0x82711CE8 (baked 404)
        // +834784 read/write the X360-only member.
        const InterfaceAt834784* GetReadInterfaceAt834784() const;                // 0x827A0B78 (baked 406)
        InterfaceAt834784*       GetWriteInterfaceAt834784();                     // 0x82711D90 (baked 407)
        // ⭐ +11376 read mSceneInputInterface -- the leg WorldModule::
        // BridgeEntityModulesToScene_PostPhysics @0x827AB608 calls.
        const SceneInputInterface* GetSceneInputInterface() const;                // 0x827A0C20 (baked 409)
        // +830144 read mTrafficTypeResponseQueue.
        const TrafficTypeResponseQueue* GetTrafficTypeResponseQueue() const;      // 0x827A0CC8 (baked 412)
        // +830672 read mResourceRequestInterface.
        const ResourceRequestInterface* GetResourceRequestInterface() const;      // 0x827A0D70 (baked 415)
        // ⭐ ADDED 2026-08-21 (wave T1 round 3, closure item 1) -- the WRITE half, and the
        // last of the four declarations round 2 parked TrafficEntityModule::UpdateStreaming on.
        // X360 sub_82711F88, whose ONLY xref is UpdateStreaming @0x82748848: it asserts
        // IsBufferLockedForWriting ("Not locked for writing", BrnTrafficEntityModuleIO.h
        // baked line 416) and returns `this + 830672` -- which is mResourceRequestInterface,
        // the SAME member the const getter above returns at the same console offset. Modelled
        // as `return &mResourceRequestInterface;` (by NAME): the console offset is provenance
        // for WHICH member, never arithmetic in the body.
        ResourceRequestInterface*       GetResourceRequestInterface();            // 0x82711F88 (baked 416)
        // +834828 read/write mGuiEventQueue.
        const GuiEventInputQueue* GetGuiEventQueue() const;                       // 0x827A0E18 (baked 418)
        GuiEventInputQueue*       GetGuiEventQueue();                             // 0x82712030 (baked 419)

        static void _AssertLayout();

    private:
        // Members in the console's ascending-offset order; the console offsets are PROVENANCE and
        // every access below goes by name (see the FLAG above -- no host offset is pinned).
        u8                             maStatusPad[7];                     // +1..+7 (force +8)
        CrashTrafficInputInterface     mCrashTrafficInputInterface;        // console +8      :416
        TrafficNetworkOutputInterface  mNetworkInterface;                  // console +3488   :417
        TrafficSoundOutputInterface    mTrafficSoundOutputInterface;       // console +3632   :418
        TrafficDirectorOutputInterface mTrafficDirectorOutputInterface;    // console +6208   :419
        GameEventQueue                 mGameEventQueue;                    // console +9824   :420
        SceneInputInterface            mSceneInputInterface;               // console +11376  :421 ⭐
        TrafficTypeResponseQueue       mTrafficTypeResponseQueue;          // console +830144 :422
        ResourceRequestInterface       mResourceRequestInterface;          // console +830672 :423
        InterfaceAt834784              mInterfaceAt834784;                 // console +834784 (X360-only)
        GuiEventInputQueue             mGuiEventQueue;                     // console +834828 :424
    };

    // ============================================================================
    // OutputBuffer_PreScene  (DWARF :185+, pre-scene output buffer)  -- NEW HOME
    // ============================================================================
    // ⭐⭐ RELAID OUT 2026-08-19 (wave Q6 cluster C3) FROM ITS OWN Construct @0x82761790, and the
    // old layout had BOTH its members in the wrong place AND its two accessors on the wrong
    // members. Construct, store for store:
    //   stb 1,0(this)                                     IOBuffer status
    //   InSceneUpdateInterface::Construct   this+0x10      mSceneInputInterface     (+16)     ⭐
    //   7x std 0 stride 8 from this+0xC7E60, then stw 0 at +0x1C0/+0x204/+0x208/+0x20C and four
    //     stfs flt_82001CC0 at +0x210..+0x21C                mTrafficToRaceCarInterface_PreScene
    //                                                        (+818784, and 819328-818784 == 544)
    //   VariableEventQueue<131072,16>::Construct  this+0xC8080   mTriggerManagementInputInterface
    //                                                            add queue        (+819328)
    //   InRemoveTriggerEvent<256>::Construct      this+0xE8090   its remove queue (+950416
    //                                                            == 819328 + 131088)
    //   stwx 0, this, 0xE8720                     this+951072    mPotentialScorees's count (:209;
    //                                                            not modelled -- see below)
    //
    // X360 ACCESSOR MAP (baked source line in brackets; uniform +4 skew off the DecFIGS lines):
    //   +16     mSceneInputInterface                read 0x8279FD58 [186] (DWARF :182)   ⭐
    //   +818784 mTrafficToRaceCarInterface_PreScene  read 0x827BB090 [189] (:185)
    //                                                write 0x82710DD0 [190] (:186)
    //   +819328 mTriggerManagementInputInterface     read 0x8279FE00 [192] (:188)
    //                                                write 0x82710E78 [193] (:189)
    //
    // ⛔ WHAT WAS WRONG, MEASURED. (1) The tree put mTrafficToRaceCarInterface_PreScene at +63424
    // on the strength of accessor 0x827A00B0 -- but that accessor bakes line 262, which is
    // OutputBuffer_PostScene's ladder (256 -> +4, 259 -> +16416, 262 -> +63424), not this
    // buffer's; and +63424 falls INSIDE the scene interface's [16, 818784) span, so it could
    // never have been a sibling member. (2) The +818784 accessor pair 0x827BB090/0x82710DD0 was
    // homed on mTriggerManagementInputInterface -- one member too early. That is not cosmetic:
    // WorldBridgeEntityModulesToEntityModules.cpp's BridgeTrafficToTrigger_PreScene Appends
    // GetTriggerManagementInputInterface() into the trigger module's real
    // TriggerManagementInputInterface, i.e. it would have read a
    // VariableEventQueue<131072,16> header out of the 544-byte traffic->race-car block. That TU
    // is not on the build list, so the bug was latent; it is fixed here, not worked around.
    // (3) The separate `class OutputBuffer` that used to sit below, homing
    // GetReadInterfaceAt818784 (0x827BB090) and GetWriteInterfaceAt819328 (0x82710E78) as
    // offset-role members, was a DUPLICATE MODEL of these same two seats made when nobody knew
    // whose they were. Both accessors are this buffer's; the phantom class is retired and had
    // no user anywhere in the tree (grepped).
    //
    // NOT MODELLED, and named so nobody re-derives it: mPotentialScorees (DWARF :209,
    // GuiTrafficCarInfoEvent::ScoringVehicleArray, console +951072, its count word zeroed by
    // Construct) has no accessor in the tree and no producer; it is this buffer's last member and
    // leaving it out shortens the host object without moving anything. Its `AddPotentialScoree`
    // (:198) / `GetPotentialScorees` (:202) are likewise absent.
    //
    // ⭐ Q6 ROUND-1 FIX (bridges #1) -- THE PADDING FORK IS RETIRED. This block used to carry a
    // FLAG reading "TrafficToRaceCarInterface_PreScene has no home in the tree, so it stays
    // opaque". THAT WAS FALSE: BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene has a
    // fully reconstructed, named, canonical home in this same subsystem --
    // TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h -- and
    // BrnRaceCarEntityModuleIO.h had already deleted its own inline slice to adopt it, precisely
    // to avoid an ODR-confusing duplicate of that fully-qualified name. The nested opaque struct
    // is now a typedef of the committed type (the same idiom BrnRaceCarEntityModuleIO.h:475
    // uses, so it is proven to compile), which is size-neutral: the canonical type is
    // pointer-free and its host sizeof is the console span's 544 bytes, as _AssertLayout()
    // still pins. Consequence to collect: BrnWorldModule.cpp:2656's
    // `reinterpret_cast<const BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene*>`
    // adapter existed ONLY because of this fork and can now be retired (left for the conductor,
    // who owns that file).
    class OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        // DWARF :206 spells the member `OutputBuffer_Prepare::SceneInputInterface` == the scene
        // manager's InSceneUpdateInterface (Construct-attested at +16).
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface;
        // DWARF :207 -- the committed SharedIO type, adopted by typedef (see the ⭐ Q6 note in
        // the class banner above; console span [818784, 819328) == its 544-byte sizeof).
        typedef BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene TrafficToRaceCarInterface_PreScene;
        // DWARF :208 / :89 -- the real committed trigger-management aggregate (its two queues are
        // exactly the pair Construct builds at +819328 and +819328+131088).
        typedef BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface TriggerManagementInputInterface;

        // @0x82761790 -- run by CreateIOBuffer<OutputBuffer_PreScene> @0x827B6330.
        void Construct();

        // ⭐ +16 read -- the leg WorldModule::BridgeEntityModulesToSceneModule_PreScene
        // @0x827AB490 calls (its first merge, 0x827AB570).
        const SceneInputInterface* GetSceneInputInterface() const;                              // 0x8279FD58 [186]
        // ⭐ ADDED 2026-08-21 (wave T1 round 4, item 2) -- the WRITE half, and the last
        // declaration TrafficEntityModule::CreateNewVehicleEntities @0x8272FA30 was blocked on.
        // X360 sub_82710D28 (IDA-unnamed, the same blind spot class as the :153 getter above):
        // it tests the WRITE lock bit (`(*a1 >> 3) & 1`), fires
        // FireAssert("Not locked for writing\n", "...\\BrnTrafficEntityModuleIO.h", 187) and
        // returns `a1 + 16` -- which is mSceneInputInterface, the SAME member the const getter
        // above returns (Construct @0x82761790 calls InSceneUpdateInterface::Construct on
        // this+0x10). Its four callers are all PreScene-buffer WRITERS:
        // GenerateCrashedVehicleEvents @0x82720030, KillDyingVehicleEntity @0x8272EB40,
        // CreateNewVehicleEntities @0x8272FA30 and UpdateCollidableVehicles @0x827302C8.
        // Modelled as `return &mSceneInputInterface;` BY NAME; the console's +16 is provenance
        // for WHICH member, never arithmetic in the body. Exact sibling of the
        // OutputBuffer_PostPhysics::GetResourceRequestInterface() non-const half landed in
        // round 3.
        SceneInputInterface*       GetSceneInputInterface();                                    // 0x82710D28 [187]
        // +818784 read/write.
        const TrafficToRaceCarInterface_PreScene* GetTrafficToRaceCarInterface_PreScene() const; // 0x827BB090 [189]
        TrafficToRaceCarInterface_PreScene*       GetTrafficToRaceCarInterface_PreScene();       // 0x82710DD0 [190]
        // +819328 read/write.
        const TriggerManagementInputInterface* GetTriggerManagementInputInterface() const;       // 0x8279FE00 [192]
        TriggerManagementInputInterface*       GetTriggerManagementInputInterface();             // 0x82710E78 [193]

        static void _AssertLayout();

    private:
        u8                                 maStatusPadTo16[16 - 1];                       // +1..+15 (force +16)
        SceneInputInterface                mSceneInputInterface;                          // console +16     :206
        TrafficToRaceCarInterface_PreScene mTrafficToRaceCarInterface_PreScene;           // console +818784 :207
        TriggerManagementInputInterface    mTriggerManagementInputInterface;              // console +819328 :208
    };

    // ============================================================================
    // The PRE-DISPATCH pair -- the traffic render seam.
    // ============================================================================
    //
    // ⭐⭐ INTERIORS HOMED 2026-08-21 (traffic wave T1, cluster C5). Both classes
    // used to be `u8 maDeferredPayload[16]` with a "sizes NOT X360-attested" FLAG.
    // That FLAG was the *reason* the traffic render seam was dead: with no array
    // inside OutputBuffer_PreDispatch there was nothing for
    // TrafficEntityModule::PreDispatchUpdate to fill and nothing for
    // WorldModule::CalculateVehicleLODs to classify, so BrnWorldModule.cpp had to
    // route the LOD pass through the stand-in GetTrafficRenderInfosBringUp(),
    // which hands back a permanently length-0 array.
    //
    // THE SIZES ARE NOW FULLY X360-ATTESTED -- from the allocators, not inferred:
    //
    //   CreateIOBuffer<InputBuffer_PreDispatch>  @0x827B7250
    //       IOBufferStack::Alloc( stack, 0xA50 /* 2640 */, tag )
    //   InputBuffer_PreDispatch::Construct       @0x8275CEE8
    //       stb  r9(=1), 0(r3)             -- IOBuffer::Construct (status)
    //       stw  r10(=0), 0xA48(r3)        -- +2632
    //       stvx128 v0, r3, r8(=16)        -- a 16-byte {0,0,0,0} store at +16
    //         (v0 is built on the stack from three stfs of flt_82001CC0 plus a
    //          zero word; flt_82001CC0 is 0.0f -- proven independently by
    //          DepthOfField::SetParams @0x821F1AC8, which loads that same address
    //          into f31 and uses it as the right-hand side of the assert
    //          "lfBlurriness >= 0.0f")
    //     => a 16-byte Vector3 at +16, then an Array whose ELEMENTS start at +32
    //        and whose live-count word lands at +2632. 2632 - 32 == 2600 == 650 * 4
    //        == sizeof(EntityId) * 650. Total 2636, rounded to the 2640 allocated.
    //
    //   CreateIOBuffer<OutputBuffer_PreDispatch> @0x827B7320
    //       IOBufferStack::Alloc( stack, 0x308 /* 776 */, tag )
    //   OutputBuffer_PreDispatch::Construct      @0x8275CF28
    //       stb r11(=1), 0(r3) ; stw r10(=0), 0x304(r3)      -- +772
    //   OutputBuffer_PreDispatch::Clear          @0x82755BB8
    //       stw r11(=0), 0x304(r3)                            -- +772
    //     => one Array whose elements start at +4 and whose count word is at +772.
    //        772 - 4 == 768 == 64 * 12 == sizeof(VehicleRenderInfo) * 64, and
    //        4 + 772 == 776 exactly. INDEPENDENTLY CORROBORATED by the consumer:
    //        WorldModule::GenerateDispatchLists @0x827D1CE8 computes the array it
    //        passes to CalculateVehicleLODs as `addi r22, r19, 4` (r19 == the
    //        buffer) -- the same "+4" (already written down in BrnWorldModule.cpp's
    //        GetTrafficRenderInfosBringUp banner).
    //
    // THE MEMBER NAMES ARE DecFIGS DWARF-EXACT (dwarfdump
    // GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h):
    //   InputBuffer_PreDispatch  : Vector3 mCameraPosition                       (:444)
    //                              Array<CgsSceneManager::EntityId,650u> maTrafficEntityIds (:445)
    //                              Construct (:439), Clear (:442)
    //   OutputBuffer_PreDispatch : Array<BrnTraffic::VehicleRenderInfo,64u> maTrafficRenderInfos (:457)
    //                              Construct (:452), Clear (:455)
    // and the DWARF shows BOTH members PUBLIC (unlike the sibling
    // InputBuffer_Dispatch, which the same dump prints under an explicit
    // `private:`). That is why the console can do `addi r22, r19, 4` inline with
    // no accessor call, and why there is no Get* pair to reconstruct: the DWARF
    // lists Construct and Clear as this pair's ONLY methods.
    //
    // ⭐ CONTRACT FOR CLUSTER C6 (the render wire): the replacement for
    // `GetTrafficRenderInfosBringUp( lpTrafficRenderInfos )` in BrnWorldModule.cpp
    // is simply `lpTrafficRenderInfos->maTrafficRenderInfos` -- the exact object
    // the console's `buffer + 4` names. Deleting the stand-in is C6's edit; this
    // header is the half of the contract C5 owed.
    //
    // ⛔ NO Get*() ACCESSORS EXIST ON EITHER OF THESE TWO BUFFERS, AND NONE MAY BE
    //    ADDED (checked 2026-08-21, wave T1 round 2, cluster R2A -- the round-2 spec
    //    asked for `GetVisibleEntities()` / `GetCameraPosition()` and an
    //    `Array<VehicleRenderInfo,64>` accessor; that premise is REFUTED, so they are
    //    parked rather than fabricated). Two independent checks agree:
    //      * DecFIGS DWARF (dwarfdump .../BrnTrafficEntityModuleIO.h, structs at :436
    //        and :449) lists exactly FOUR members between the pair -- Construct (:439),
    //        Clear (:442), Construct (:452), Clear (:455) -- and NO accessor of any
    //        kind, with both data members emitted OUTSIDE any access label (i.e. public
    //        in a `struct`), unlike the sibling InputBuffer_Dispatch which the same dump
    //        prints under an explicit `private:`;
    //      * the X360 ledger (progress/status.json -> func) carries only
    //        InputBuffer_PreDispatch::Construct, OutputBuffer_PreDispatch::Construct and
    //        OutputBuffer_PreDispatch::Clear for these two classes. Adding an accessor
    //        would therefore be surface the shipped binary does not have.
    //    THE MEMBERS ARE ALREADY PUBLIC, so a consumer reads them directly -- which is
    //    precisely what the console does inline (`addi r22, r19, 4`, no call):
    //        lpInput->mCameraPosition            // the camera position  (:444)
    //        lpInput->maTrafficEntityIds         // the visible-entity list (:445)
    //        lpOutput->maTrafficRenderInfos      // the render-info array  (:457)
    //    The two Set* writers below are a de-inlining of WorldModule::
    //    GenerateDispatchLists' two inline stores and are marked as such; they are the
    //    only non-DWARF surface on this pair and no read-side twin is warranted.
    //
    // ⭐ HOST LAYOUT: every member of both buffers is POINTER-FREE (a Vector3, a
    // 4-byte EntityId array, and a 12-byte VehicleRenderInfo array), so unlike
    // most IO buffers in this file the console spans really do survive to x64 --
    // and _AssertLayout() pins them as pointer-invariant facts rather than as
    // console byte offsets. The one host-vs-console difference is the leading
    // status byte: CgsModule::IOBuffer is a 1-byte FlagSet8, so the explicit pads
    // below reproduce the console's start-of-payload alignment.
    //
    // NEVER-CONSTRUCTED-QUEUE CHECK (AGENTS.md gotcha (a)): neither buffer holds
    // an EventQueue, a VariableEventQueue, a RequestInterface or any other
    // aggregate with its own Construct -- the complete member set is the four
    // fields listed above. What they DO hold are two Array<T,N>, and an Array is
    // unusable until its count word leaves the KI_UNCONSTRUCTED(-1) sentinel; the
    // console's Construct writes that word for both (the +2632 and +772 stores
    // above), and so does each Construct() below. That is the same class of bug
    // in this file's own vocabulary, and it is closed here rather than assumed.
    // ------------------------------------------------------------------------
    class InputBuffer_PreDispatch : public CgsModule::IOBuffer
    {
    public:
        // @0x8275CEE8 -- run by CreateIOBuffer<InputBuffer_PreDispatch> @0x827B7250.
        void Construct();

        // DWARF :442. Not separately exported (the console inlines it); the empty
        // state is "no visible entities", i.e. the Array's count word back to 0 --
        // the same store Construct makes at +2632.
        void Clear();

        // The two de-inlined writers WorldModule::GenerateDispatchLists fills the
        // buffer through (@0x827D1CE8 writes both members inline, between
        // Construct() and its LockForRead(); there is NO write lock around them,
        // so neither asserts one -- see BrnWorldModule.cpp's
        // "traffic pre-dispatch + the vehicle LOD policy" block).
        void SetVisibleEntities( const Array<CgsSceneManager::EntityId, 650u>& lrEntities );
        void SetCameraPosition( Vector3 lvCameraPosition );

        static void _AssertLayout();

        // PUBLIC, per the DWARF -- the traffic module reads them directly.
        u8                                     maStatusPadTo16[16 - 1];   // +1..+15 (force the Vector3 to +16)
        Vector3                                mCameraPosition;           // console +16    :444
        Array<CgsSceneManager::EntityId, 650u> maTrafficEntityIds;        // console +32    :445
    };

    class OutputBuffer_PreDispatch : public CgsModule::IOBuffer
    {
    public:
        // @0x8275CF28 -- run by CreateIOBuffer<OutputBuffer_PreDispatch> @0x827B7320.
        void Construct();

        // @0x82755BB8 -- `*(this + 772) = 0`, i.e. maTrafficRenderInfos.Clear().
        void Clear();

        static void _AssertLayout();

        // PUBLIC, per the DWARF. Producer: TrafficEntityModule::PreDispatchUpdate
        // @0x8274D900 (cluster C6). Consumers: WorldModule::CalculateVehicleLODs
        // (writes VehicleRenderInfo::mLOD) and TrafficEntityModule::
        // GenerateDispatchLists @0x8273B280.
        Array<BrnTraffic::VehicleRenderInfo, 64u> maTrafficRenderInfos;   // console +4     :457
    };

    // ------------------------------------------------------------------------
    // [T1-rinfo] BRING-UP PROBE -- NOT IN THE X360 BINARY. DELETE WHEN STABLE.
    // Latched report of how many VehicleRenderInfos the traffic module produced;
    // prints only when the count CHANGES, and only under BRN_TRAFFIC_DIAG. It is
    // a free function rather than a member so it adds nothing at all to the
    // buffer's shape. Call it right after TrafficEntityModule::PreDispatchUpdate
    // returns -- that is the frame position where the number first means
    // something. OutputBuffer_PreDispatch::Clear() also calls it, so the probe
    // still reports even before C6 wires the call site.
    // ------------------------------------------------------------------------
    void T1Diag_ReportTrafficRenderInfoCount( const OutputBuffer_PreDispatch& lrBuffer );

    // OutputBuffer_Prepare  (DWARF :2/:115, prepare/boot output buffer)  -- NEW HOME
    // ============================================================================
    // Owns the scene-input interface (@16, read 0x8279F988 [baked 126] / write 0x827109E0) and the
    // 4096-slot resource-request interface (@818784, read 0x8279FA30 [baked 129] / write 0x82710A88).
    //
    // ⭐⭐ SceneInputInterface PROMOTED 2026-08-19 (wave Q6 cluster C3) -- AND IT DISARMS A LIVE
    // LANDMINE, not merely a gap. It was `struct { unsigned char maBytes[1]; }` in front of a raw
    // `maPad16To818784` span: an UNTYPED, NEVER-CONSTRUCTED byte block that
    // WorldModule::BridgeTrafficModuleToSceneModule_Prepare @0x827AB300 used to
    // reinterpret_cast to the 25-queue aggregate and Append -- reading 25 mpEvents/miLength pairs
    // out of whatever the IO stack's previous tenant left (CreateIOBuffer stopped zero-filling in
    // the 2026-08-15 perf pass). Wave Q5/F3 disarmed that leg and filed the recipe; this is the
    // fix it asked for. The type is NOT inferred: OutputBuffer_Prepare::Construct @0x82761740
    // calls CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct(this+0x10), and
    // 16 + 818768 == 818784 == the next member exactly.
    class OutputBuffer_Prepare : public CgsModule::IOBuffer
    {
    public:
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface;  // :12
        // RETYPED 2026-07-24 (WorldModule::Prepare/BridgeTrafficResourceRequestsToOutput
        // consumers need the real pipe; the :12 comment already named it).
        typedef BrnResource::GameDataIO::RequestInterface<4096> ResourceRequestInterface;

        // @0x82761740 -- run by CreateIOBuffer<OutputBuffer_Prepare> @0x827B5C70. ADDED with the
        // promotion: without it the base IOBuffer::Construct ran alone and the aggregate's 25
        // queues kept a foreign mpEvents (AGENTS.md gotcha 14).
        void Construct();

        // +16 read/write.
        const SceneInputInterface* GetSceneInputInterface() const;      // 0x8279F988 [baked 126]
        SceneInputInterface*       GetSceneInputInterface();            // 0x827109E0
        // +818784 read/write.
        const ResourceRequestInterface* GetResourceRequestInterface() const; // 0x8279FA30 [baked 129]
        ResourceRequestInterface*       GetResourceRequestInterface();       // 0x82710A88 (:130)

        static void _AssertLayout();

    private:
        u8                       maStatusPadTo16[16 - 1];                     // +1..+15
        SceneInputInterface      mSceneInputInterface;                        // console +16
        // ⚠ NO PAD HERE ANY MORE: the seat holds the REAL aggregate, whose host sizeof already
        // EXCEEDS the 818,768-byte console span (its queues carry an 8-byte mpEvents where the
        // console has 4). Same disposition PhysicsModuleIO::OutputBuffer's scene seat took on
        // 2026-08-19; keeping the pad as well would add 818 KB of dead space.
        ResourceRequestInterface mResourceRequestInterface;                   // console +818784
    };

    // ============================================================================
    // OutputBuffer_PrePhysics  (DWARF :403, pre-physics output buffer)  -- NEW HOME
    // ============================================================================
    // ⭐⭐ RETYPED 2026-08-10 (pre-physics bridge wave) -- AND IT WAS A LATENT MEMORY BUG.
    //
    // This class used to model ONE member, `mVehicleDriverInterface`, as an opaque
    // `unsigned char[1]` behind a 143,983-byte pad, with a console-pinned
    // `static_assert(offsetof(...) == 143984)`. Nothing on the PC build had ever read it, so
    // the slice was inert -- but WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics
    // @0x827AAEC0 reads all THREE interfaces this buffer owns and merges them into the physics
    // input, and `VehicleDriverInputInterface::Append` alone moves 5,284 bytes. Landing that
    // bridge over the 1-byte slice would have been a ~5.3 KB heap overrun on the first frame.
    //
    // THE LAYOUT IS NOT GUESSED. Three independent witnesses agree:
    //  (1) BrnTrafficIO::OutputBuffer_PrePhysics::Construct @0x827618A0, read instruction for
    //      instruction: status byte at +0; VehicleInputInterface::Construct(this+16);
    //      CreateAirRamEvent<20>::Construct(this+142192); CreateSpinEvent<10>::Construct(
    //      this+143488); the two effect counters at +142200/+143496 zeroed;
    //      VehicleDriverInputInterface::Construct(this+143984); `stbx 0` at +149280.
    //  (2) the DecFIGS DWARF for this very class (BrnTrafficEntityModuleIO.h:324..327):
    //      mVehicleInputInterface, mVehicleEffectsInterface, mVehicleDriverInterface,
    //      `bool mbPlayingShowtime` -- the +149280 byte, with GetPlayingShowtime/
    //      SetPlayingShowtime declared at :319/:320.
    //  (3) arithmetic against the race-car pre-physics buffer, which holds the same three
    //      interface types: 142192-16 == 142176 == sizeof(VehicleInputInterface);
    //      143984-142192 == 1792 == sizeof(VehicleEffectsInputInterface); and 143488-142192
    //      == 1296 == the air-ram -> spin gap the bridge's own `Append(x+0x510)` pair uses.
    //
    // ⛔ THE `offsetof == 143984` STATIC_ASSERT IS DELETED, and that is the CORRECT direction,
    // not a weakening: it pinned a CONSOLE byte offset on an x64 host layout in which
    // VehicleInputInterface's embedded queues carry widened pointers, so the moment the seat
    // became real the gate would have failed on a layout that is right. Parity here is BY
    // NAMED MEMBER + SEQUENCE, per the standing x64 rule. (It had also never once compiled --
    // this TU has never been on the build list. It is mounted in the same commit.)
    class OutputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
    public:
        // DWARF :79/:80/:81 -- the three typedefs, all naming the REAL physics-side types (the
        // same three the race-car pre-physics buffer owns).
        typedef BrnPhysics::Vehicle::VehicleInputInterface        VehicleInputInterface;        // :79
        typedef BrnPhysics::Vehicle::VehicleEffectsInputInterface VehicleEffectsInputInterface; // :80
        typedef BrnPhysics::Vehicle::VehicleDriverInputInterface  VehicleDriverInputInterface;  // :81

        // @0x827618A0 (DWARF :307). Bodied in BrnTrafficEntityModuleIO.cpp.
        void Construct();

        const VehicleInputInterface*        GetVehicleInputInterface() const;   // :310 R (0x827A0378, +16)
        VehicleInputInterface*              GetVehicleInputInterface();         // :311 W
        const VehicleEffectsInputInterface* GetVehicleEffectsInterface() const; // :313 R (0x827A0420, +142192)
        VehicleEffectsInputInterface*       GetVehicleEffectsInterface();       // :314 W
        const VehicleDriverInputInterface*  GetVehicleDriverInterface() const;  // :316 R (0x827A04C8, +143984)
        VehicleDriverInputInterface*        GetVehicleDriverInterface();        // :317 W (0x82711508)

        // DWARF :319/:320. The +149280 byte Construct zeroes. Header inlines -- the console has
        // no out-of-line symbol for either.
        bool GetPlayingShowtime() const   { return mbPlayingShowtime; }
        void SetPlayingShowtime(bool lb)  { mbPlayingShowtime = lb; }

    private:
        u8                           maStatusPadTo16[16 - 1];        // +1..+15 (status byte is the base)
        VehicleInputInterface        mVehicleInputInterface;         // :324  console +16
        VehicleEffectsInputInterface mVehicleEffectsInterface;       // :325  console +142192
        VehicleDriverInputInterface  mVehicleDriverInterface;        // :326  console +143984
        bool                         mbPlayingShowtime;              // :327  console +149280
    };
}
}
