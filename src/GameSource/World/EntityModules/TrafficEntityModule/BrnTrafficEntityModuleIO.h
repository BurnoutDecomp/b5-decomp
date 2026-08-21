#pragma once

// BrnTraffic::BrnTrafficIO IO buffers (TrafficEntityModule shared IO), from the DecFIGS DWARF
// (GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h) with member
// offsets pinned by the X360 retail XEX.
//
// The InputBuffer_* buffers are what the world/physics/race-car bridges fill for the traffic
// module to drain; the OutputBuffer_* buffers are what the traffic module produces. Members
// homed in other TUs are embedded by value using their real committed types where those exist,
// or as FLAGged opaque stand-ins sized to the X360 spans so the pinned offsets stay exact.

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
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"         // OutputBuffer_PrePhysics::VehicleInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEffectsInputInterface.h"  // OutputBuffer_PrePhysics::VehicleEffectsInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"   // OutputBuffer_PrePhysics::VehicleDriverInputInterface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // DeformationOutputInterfaceForEntityModules
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"                       // ContactSpyInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                          // CgsModule::VariableEventQueue<N,16>
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h" // BrnWorld::PropEntityIO::PropToTrafficInterface (InputBuffer_PrePhysics member)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"            // CgsSceneManager::SceneManagerIO::InSceneUpdateInterface (the two OutputBuffer scene seats)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h" // TrafficDirectorOutputInterface (OutputBuffer_PostPhysics @+6208)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h"      // TrafficTypeResponse (OutputBuffer_PostPhysics @+830144)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h"  // TrafficToRaceCarInterface_PreScene (OutputBuffer_PreScene @+818784)
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface (OutputBuffer_PreScene @+819328)

// ---- OutputBuffer_PreDispatch member type home ----
// BrnTraffic::VehicleRenderInfo is stored by value inside an Array<...,64>, so the complete type
// must come in. Its committed home is BrnTrafficEntityModule.h; the DWARF's own home is
// BrnTrafficVehicle.h:159, so narrow this include if the record ever moves there. Not a cycle:
// nothing in BrnTrafficEntityModule.h's transitive closure includes this IO header.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"  // BrnTraffic::VehicleRenderInfo

// ---- InputBuffer_Dispatch member type homes (pointer slots) ----
// BrnBlobbyShadowBuffer and BrnSubmissionInterface are nested classes, so they cannot be
// forward-declared; the real headers come in.
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"   // BrnBlobbyShadowManager::BrnBlobbyShadowBuffer
#include "GameSource/Graphics/BrnCoronaManager.h"         // BrnCoronaManager::BrnSubmissionInterface
// Pointer-only uses in InputBuffer_Dispatch (forward-declaration exception (b)): including
// CgsDispatcher.h / BrnShadowMap.h here would pull the renderer and shadow-cascade tail into
// every includer, and BrnWorldModule.h reaches this header.
namespace CgsGraphics { class DispatchFrame; }
namespace BrnWorld { struct ShadowMap; }

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // ============================================================================
    // InputBuffer_Dispatch
    // ============================================================================
    // The traffic module's generate-dispatch-lists input buffer.
    // WorldModule::BridgeWorldModuleToEntityModules_Render fills it through the four setters;
    // TrafficEntityModule::GenerateDispatchLists drains it through the four getters.
    //
    // Console layout, provenance only (every access below goes by name):
    //   base    CgsModule::IOBuffer   1-byte status FlagSet8, then pad
    //   +0x8014 dispatch frame
    //   +0x8018 blobby-shadow buffer
    //   +0x801C corona-submission interface
    //   +0x8020 shadow map
    // All four are POINTERS, not u32 handles: the X360 accessors are a bare lwzx/stwx because a
    // console pointer is 32 bits, and the only producer
    // (WorldModule::BridgeWorldModuleToEntityModules_Render @0x827ABE28) stores real pointers
    // into every one. Modelling them as u32 truncates a host pointer.
    //
    // FLAG (opaque interior): the dispatch-list payload from base to +0x8014 is unrecovered and
    // stays correctly-sized opaque storage, so the four fields keep the console's tail order. On
    // the host the widened pointers put that block at 0x8018, which _AssertLayout pins as a HOST
    // fact. Nothing serialises this buffer; it is a pure runtime IO object.
    class InputBuffer_Dispatch : public CgsModule::IOBuffer
    {
    public:
        // X360 0x827120D8: read-lock; return the dispatch frame (console word @this+0x8014).
        CgsGraphics::DispatchFrame* GetDispatchFrame() const;

        // WorldModule::GenerateDispatchLists @0x827D1CE8 seeds the frustum result through this
        // pipe (X360 accessor sub_827BB138; VariableEventQueue<32768,16>, as the siblings are).
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

        // Without this, CreateIOBuffer<InputBuffer_Dispatch> runs only the inherited
        // CgsModule::IOBuffer::Construct and the four pointer slots below arrive holding the
        // previous IO-stack tenant's bytes. Body in
        // BrnTrafficEntityModuleIO_InputBuffer_Dispatch.cpp.
        void Construct();   // X360 0x8275CF40

        static void _AssertLayout();

    private:
        // The IOBuffer base is one status byte; the four pointer slots sit contiguously at the
        // tail (console +0x8014..+0x8020). The preceding dispatch-list payload is folded into
        // opaque storage, and 0x8014 - 1 accounts for the base's status byte. On the host the
        // widened pointers push the block to 0x8018..0x8030.
        u8                                             maPayloadAndPad[0x8014 - 1];       // +0x0001..+0x8013 (status pad + payload)
        CgsGraphics::DispatchFrame*                    mpDispatchFrame;                   // console +0x8014
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* mpBlobbyShadowBuffer;              // console +0x8018
        BrnCoronaManager::BrnSubmissionInterface*      mpCoronaSubmissionInterface;       // console +0x801C
        BrnWorld::ShadowMap*                           mpShadowMap;                       // console +0x8020
    };

    // ============================================================================
    // InputBuffer_PreScene  (DWARF BrnTrafficEntityModuleIO.h:143)
    // ============================================================================
    // The traffic module's pre-scene input buffer. WorldModule::BridgeInputToEntityModules fills
    // the timer and network members; WorldModule::BridgeRaceCarModuleToTrafficModule_PreScene
    // fills the two race-car interfaces.
    class InputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarOutputInterface;   // DWARF :85
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface GlobalRaceCarOutputInterface;   // DWARF :86

        // X360 0x82710B30 -- read-lock; returns &mTimerStatusInterface (this + 4).
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const;                                       // DWARF :150
        // X360 0x8279FAD8 -- write-lock; field-copies the source timer status into +4 (operator=).
        void SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface);                  // DWARF :151
        // X360 0x82710BD8, IDA-unnamed. Identified by its own baked assert
        // (FireAssert "Not locked for reading", BrnTrafficEntityModuleIO.h line 157) followed by
        // `addi r3, r28, 0x40`, i.e. this+0x40 == mActiveRaceCarOutputInterface, the same member
        // the 0x8279FBE8 setter writes. Callers: PreSceneUpdate @0x8274A968,
        // GenerateNearbyParkedTrafficOutput @0x8271FA18,
        // GeneratePotentialLeapedAndStompedCarsOutput @0x8271F298, UpdateCollidableVehicles
        // @0x827302C8.
        const ActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const;                                  // DWARF :153
        // X360 0x8279FBE8 -- write-lock; flat 10480B member copy into +64.
        void SetActiveRaceCarOutputInterface(const ActiveRaceCarOutputInterface* lpInterface);                        // DWARF :154
        // X360 0x8279FCA0 -- write-lock; flat 2416B member copy into +10544.
        void SetGlobalRaceCarOutputInterface(const GlobalRaceCarOutputInterface* lpInterface);                        // DWARF :157
        // X360 0x827ACD28 -- write-lock; clear+append the hull queue, copy mbDiverged into +12960.
        void SetTrafficNetworkInputInterface(const TrafficNetworkInputInterface* lpTrafficNetworkInputInterface);     // DWARF :160

        // WorldModule::EntityModulePreSceneUpdate @0x827BD1F0 stores time of day into +13072
        // under the write lock (source: EnvironmentManager::mfCurrTimeOfDay).
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
    // InputBuffer_PostScene  (DWARF BrnTrafficEntityModuleIO.h:221)
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
    // FLAG (opaque interior): RaceCarToTrafficInterface has no home in the tree, so it is a
    // 1-byte placeholder. Nothing here touches its interior, and the two offsets that matter
    // (@8 and @1648) are asserted in the .cpp bodies. Adopt the named type when its home lands.
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
    // InputBuffer_PrePhysics  (DWARF BrnTrafficEntityModuleIO.h:330)
    // ============================================================================
    // The traffic module's pre-physics input buffer. Member sequence and names from the DWARF
    // (:330-364); byte offsets pinned by the three X360 setter bodies (0x827A9DE0 / 0x827A9E98 /
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

        // The seat is X360-attested, not inferred: InputBuffer_PrePhysics::Construct @0x827615F8
        // builds this member's two queues at +0x30C60 / +0x30CEC (199776 / 199916), and the
        // 140-byte gap is exactly sizeof(EventQueue<TrafficLightKnockDownEvent,32>) == 12 + 32*4.
        // It must stay the real type: BridgePropModuleToTrafficModule_PrePhysics @0x827AEA70
        // Clear()s and Append()s both queues, so a 1-byte placeholder here is a ~140-byte write
        // past the member with the ring's miLength read out of foreign memory.
        typedef BrnWorld::PropEntityIO::PropToTrafficInterface PropToTrafficInterface;   // :299

        // @0x827615F8 (DWARF :274) -- the buffer bring-up CreateIOBuffer<T> runs. Without it the
        // base IOBuffer::Construct runs instead and
        // BridgeSceneContactsToTrafficModule_PrePhysics dies on 'mpEvents != NULL' inside
        // SetOverlapPairsQueue.
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
        // The named type is what lets the console's third Construct leg
        // (VariableEventQueue<32768,16>::Construct(this+0x28C30)) have an expression; as an
        // opaque span its mbIsConstructed/miFirstEventOffset kept whatever the previous IO-stack
        // tenant left. Size-neutral: VariableEventQueue<BUFSIZE,ALIGN> is pointer-free
        // (bool + char[BUFSIZE] + three s32), so its host sizeof is 32,784, the span it replaces.
        CgsModule::VariableEventQueue<32768, 16> mSceneResultQueue;                              // :296 (offset 166960)
        RCEntityPlayerResetInterface mPlayerResetInterface;                                       // :298 (offset 199744, 32B)
        PropToTrafficInterface       mPropToTrafficInterface;                                     // :299 (offset 199776, real type)
    };

    // ============================================================================
    // InputBuffer_PostPhysics  (DWARF BrnTrafficEntityModuleIO.h:459)
    // ============================================================================
    // The traffic module's post-physics input buffer. The physics and race-car bridges publish
    // per-frame snapshots through WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics
    // (and BridgeRaceCarModuleToTrafficModule_PreScene for the active-race-car member), calling
    // the five write-lock setters below. Each tests the write-lock bit, fires the non-gating
    // "Not locked for writing" assert, then copies the source into the matching member.
    //
    // The DWARF (:459-499) names every member; the X360 accessor/store offsets pin them:
    //   +0x10    (16)     mVehicleOutputInterface                     (:370)
    //   +0xEC30  (60464)  mVehicleManagerOutputInterface              (:372)
    //   +0x128C0 (75968)  mActiveRaceCarOutputInterface               (:374; X360 XMemCpy 0x28F0 == 10480)
    //   +0x151B0 (86448)  mDeformationOutputInterfaceForEntityModules (:375)
    //   +0x19EF0 (106224) mContactSpyInterface                        (:376; single-word copy)
    //
    // Offsets are not static_asserted: the SIMD and queue aggregates widen on the 64-bit host, so
    // host sizes cannot reproduce the 32-bit X360 offsets. mSceneResultQueue is an opaque
    // by-value stand-in sized to its X360 span; adopt the named queue type when its home lands.
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

        // Size-neutral: 13328 is both the X360 span and the host sizeof
        // (VariableEventQueue<13312,16> is pointer-free). WorldModule::BridgeActionsToTrafficModule
        // @0x827ABFF0 AddEvents into this slot for 22 game-action ids; as an opaque blob the queue
        // could never be Constructed and the first delivered action fired "Not Constructed"
        // (CgsVariableEventQueue.h:454 / :728). Naming the type lets Construct() below build it.
        typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueStorage; // :373 span +0xEC30+sizeof(VMOI) .. +0x128C0

        // The X360 CreateIOBuffer<T> runs T::Construct after the stack alloc; the PC template only
        // placement-news, so WorldModule::Update / UpdateForBootUpVideo call this explicitly.
        void Construct();

        // X360 0x827A9F50 (:347): write-lock; mVehicleOutputInterface = *src.
        void SetVehicleOutputInterface(const VehicleOutputInterface* lpVehicleOutputInterface);
        // X360 0x827AA000 (:353): write-lock; mVehicleManagerOutputInterface = *src.
        void SetVehicleManagerOutputInterface(const VehicleManagerOutputInterface* lpVehicleManagerOutputInterface);
        // X360 0x82711850, IDA-unnamed. Read-lock test, FireAssert "Not locked for reading"
        // (BrnTrafficEntityModuleIO.h line 362), then `addis r3,r28,1 ; addi r3,r3,0x28C0` ==
        // this+0x128C0 == 75968 == mActiveRaceCarOutputInterface, the member the 0x827A06C0
        // setter writes. Distinct from InputBuffer_PreScene's :153 getter: different buffer,
        // member offset and assert line. Consumers are UpdateRaceCarHulls and PostPhysicsUpdate's
        // local-player position/direction refresh.
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
    // Every member below is named and placed by OutputBuffer_PostPhysics::Construct @0x82761908.
    // That address has no per-address JSON under .ida-exports/ (an export-run gap, not a missing
    // function); its name comes from CreateIOBuffer<OutputBuffer_PostPhysics> @0x827B79D0's
    // xrefs_from. Construct, store for store:
    //   stb 1,0(this)                                          IOBuffer status
    //   CrashIO::TrafficInputInterface::Construct     this+8        mCrashTrafficInputInterface
    //   TrafficNetworkOutputInterface::Construct      this+0xDA0    mNetworkInterface        (3488)
    //   sth 0,0xE30(this)                             this+0xE30    mTrafficSoundOutputInterface (3632)
    //   stw 0,0x2650(this)                            (inside mTrafficDirectorOutputInterface)
    //   VariableEventQueue<1536,16>::Construct        this+0x2660   mGameEventQueue          (9824)
    //   InSceneUpdateInterface::Construct             this+0x2C70   mSceneInputInterface     (11376)
    //   VariableEventQueue<4096,16>::Construct+Clear  this+0xCAB90  mResourceRequestInterface(830672)
    //   11x stw 0 stride 4                            this+0xCFCE0  mInterfaceAt834784 (44 B, X360-only)
    //   EventQueue<TrafficTypeResponse,32>::Construct this+0xCAAC0  mTrafficTypeResponseQueue(830144)
    //   VariableEventQueue<32768,16>::Construct       this+0xCFD0C  mGuiEventQueue           (834828)
    // CreateIOBuffer allocates 0xD3D20 == 867,616 bytes, closing the last span exactly
    // (867616 - 834828 == 32,788 == sizeof(VariableEventQueue<32768,16>) rounded to 4).
    //
    // X360 ACCESSOR MAP (read = bit 4 IsBufferLockedForReading; write = bit 3):
    //   +8      mCrashTrafficInputInterface      read 0x827A0830 (baked 391) / write 0x82711A48 (392)
    //   +3488   mNetworkInterface                read 0x827A08D8 (baked 394)
    //   +3632   mTrafficSoundOutputInterface     read 0x827A0980 (baked 397) / write 0x82711B98 (398)
    //   +6208   mTrafficDirectorOutputInterface  read 0x827A0A28 (baked 400)
    //   +9824   mGameEventQueue                  read 0x827A0AD0 (baked 403) / write 0x82711CE8 (404)
    //   +834784 mInterfaceAt834784               read 0x827A0B78 (baked 406) / write 0x82711D90 (407)
    //   +11376  mSceneInputInterface             read 0x827A0C20 (baked 409)
    //   +830144 mTrafficTypeResponseQueue        read 0x827A0CC8 (baked 412)
    //   +830672 mResourceRequestInterface        read 0x827A0D70 (baked 415)
    //   +834828 mGuiEventQueue                   read 0x827A0E18 (baked 418) / write 0x82712030 (419)
    //
    // mSceneInputInterface is at +11376, NOT at +834784. The DecFIGS line ladder suggests +834784
    // (its read accessors bake 391/394/.../418 in threes and DecFIGS declares
    // GetSceneInputInterface at :402), but the X360 header carries one accessor pair DecFIGS does
    // not, for the +834784 member, which displaces every later slot by one. Two witnesses settle
    // it: Construct calls InSceneUpdateInterface::Construct on this+11376, the only
    // InSceneUpdateInterface-typed member the DWARF gives this class; and 11376 + 818768 (the
    // console sizeof of that aggregate) == 830144, the next member exactly, while +834784 has
    // only 44 bytes before +834828. Getting this wrong makes InSceneUpdateInterface::Append read
    // 25 mpEvents/miLength pairs out of foreign memory.
    //
    // FLAG (foreign types / opaque interior): mTrafficDirectorOutputInterface is the real
    // committed SharedIO type; mInterfaceAt834784 is the X360-only 44-byte member DecFIGS has no
    // counterpart for, named by offset role rather than invented. Nothing here pins a host byte
    // offset: several members are queue aggregates whose host header is 16 bytes where the
    // console's is 12, so host offsets legitimately exceed the console ones. Parity is by named
    // member and sequence, in the console's ascending-offset order, which is also the DWARF's
    // member order (:416..:424) plus the one X360-only insertion.
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
        // X360-only member at +834828-44: 44 bytes, zeroed by Construct as 11 words, no DecFIGS
        // counterpart and no recovered type. PARKED: named by offset role rather than invented.
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
        // +11376 read mSceneInputInterface -- the leg
        // WorldModule::BridgeEntityModulesToScene_PostPhysics @0x827AB608 calls.
        const SceneInputInterface* GetSceneInputInterface() const;                // 0x827A0C20 (baked 409)
        // +830144 read mTrafficTypeResponseQueue.
        const TrafficTypeResponseQueue* GetTrafficTypeResponseQueue() const;      // 0x827A0CC8 (baked 412)
        // +830672 read mResourceRequestInterface.
        const ResourceRequestInterface* GetResourceRequestInterface() const;      // 0x827A0D70 (baked 415)
        // The write half. X360 sub_82711F88, whose only xref is UpdateStreaming @0x82748848: it
        // asserts IsBufferLockedForWriting (baked line 416) and returns `this + 830672`, the same
        // mResourceRequestInterface the const getter above returns. The console offset is
        // provenance for which member, never arithmetic in the body.
        ResourceRequestInterface*       GetResourceRequestInterface();            // 0x82711F88 (baked 416)
        // +834828 read/write mGuiEventQueue.
        const GuiEventInputQueue* GetGuiEventQueue() const;                       // 0x827A0E18 (baked 418)
        GuiEventInputQueue*       GetGuiEventQueue();                             // 0x82712030 (baked 419)

        static void _AssertLayout();

    private:
        // Members in the console's ascending-offset order. The offsets are provenance; every
        // access goes by name and no host offset is pinned.
        u8                             maStatusPad[7];                     // +1..+7 (force +8)
        CrashTrafficInputInterface     mCrashTrafficInputInterface;        // console +8      :416
        TrafficNetworkOutputInterface  mNetworkInterface;                  // console +3488   :417
        TrafficSoundOutputInterface    mTrafficSoundOutputInterface;       // console +3632   :418
        TrafficDirectorOutputInterface mTrafficDirectorOutputInterface;    // console +6208   :419
        GameEventQueue                 mGameEventQueue;                    // console +9824   :420
        SceneInputInterface            mSceneInputInterface;               // console +11376  :421
        TrafficTypeResponseQueue       mTrafficTypeResponseQueue;          // console +830144 :422
        ResourceRequestInterface       mResourceRequestInterface;          // console +830672 :423
        InterfaceAt834784              mInterfaceAt834784;                 // console +834784 (X360-only)
        GuiEventInputQueue             mGuiEventQueue;                     // console +834828 :424
    };

    // ============================================================================
    // OutputBuffer_PreScene  (DWARF :185+, pre-scene output buffer)
    // ============================================================================
    // Laid out from its own Construct @0x82761790, store for store:
    //   stb 1,0(this)                                     IOBuffer status
    //   InSceneUpdateInterface::Construct   this+0x10      mSceneInputInterface     (+16)
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
    //   +16     mSceneInputInterface                read 0x8279FD58 [186] (DWARF :182)
    //   +818784 mTrafficToRaceCarInterface_PreScene  read 0x827BB090 [189] (:185)
    //                                                write 0x82710DD0 [190] (:186)
    //   +819328 mTriggerManagementInputInterface     read 0x8279FE00 [192] (:188)
    //                                                write 0x82710E78 [193] (:189)
    // Accessor 0x827A00B0 belongs to OutputBuffer_PostScene, not here: it bakes line 262, which
    // is that buffer's ladder (256 -> +4, 259 -> +16416, 262 -> +63424), and +63424 falls inside
    // this buffer's scene-interface span [16, 818784).
    //
    // NOT MODELLED, named so nobody re-derives it: mPotentialScorees (DWARF :209,
    // GuiTrafficCarInfoEvent::ScoringVehicleArray, console +951072, count word zeroed by
    // Construct) has no accessor and no producer in the tree. It is this buffer's last member, so
    // leaving it out shortens the host object without moving anything. Its AddPotentialScoree
    // (:198) and GetPotentialScorees (:202) are absent too.
    class OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        // DWARF :206 spells the member `OutputBuffer_Prepare::SceneInputInterface` == the scene
        // manager's InSceneUpdateInterface (Construct-attested at +16).
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface;
        // DWARF :207 -- the committed SharedIO type, adopted by typedef. It is pointer-free, so
        // its host sizeof is the console span [818784, 819328), 544 bytes, as _AssertLayout pins.
        typedef BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene TrafficToRaceCarInterface_PreScene;
        // DWARF :208 / :89 -- the real committed trigger-management aggregate (its two queues are
        // exactly the pair Construct builds at +819328 and +819328+131088).
        typedef BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface TriggerManagementInputInterface;

        // @0x82761790 -- run by CreateIOBuffer<OutputBuffer_PreScene> @0x827B6330.
        void Construct();

        // +16 read -- the leg WorldModule::BridgeEntityModulesToSceneModule_PreScene @0x827AB490
        // calls (its first merge, 0x827AB570).
        const SceneInputInterface* GetSceneInputInterface() const;                              // 0x8279FD58 [186]
        // The write half. X360 sub_82710D28, IDA-unnamed: it tests the write-lock bit
        // (`(*a1 >> 3) & 1`), fires FireAssert "Not locked for writing" (baked line 187) and
        // returns `a1 + 16` == mSceneInputInterface, the same member the const getter returns
        // (Construct @0x82761790 calls InSceneUpdateInterface::Construct on this+0x10). Callers:
        // GenerateCrashedVehicleEvents @0x82720030, KillDyingVehicleEntity @0x8272EB40,
        // CreateNewVehicleEntities @0x8272FA30, UpdateCollidableVehicles @0x827302C8.
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
    // Both interiors are X360-attested from the allocators, not inferred:
    //
    //   CreateIOBuffer<InputBuffer_PreDispatch>  @0x827B7250
    //       IOBufferStack::Alloc( stack, 0xA50 /* 2640 */, tag )
    //   InputBuffer_PreDispatch::Construct       @0x8275CEE8
    //       stb  r9(=1), 0(r3)             -- IOBuffer::Construct (status)
    //       stw  r10(=0), 0xA48(r3)        -- +2632
    //       stvx128 v0, r3, r8(=16)        -- a 16-byte {0,0,0,0} store at +16
    //         (v0 is three stfs of flt_82001CC0 plus a zero word; flt_82001CC0 is
    //          0.0f, proven by DepthOfField::SetParams @0x821F1AC8 using that same
    //          address as the right-hand side of "lfBlurriness >= 0.0f")
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
    //        4 + 772 == 776 exactly. Corroborated by the consumer:
    //        WorldModule::GenerateDispatchLists @0x827D1CE8 computes the array it
    //        passes to CalculateVehicleLODs as `addi r22, r19, 4` (r19 == buffer).
    //
    // Member names are DWARF-exact (dwarfdump .../BrnTrafficEntityModuleIO.h):
    //   InputBuffer_PreDispatch  : Vector3 mCameraPosition                       (:444)
    //                              Array<CgsSceneManager::EntityId,650u> maTrafficEntityIds (:445)
    //                              Construct (:439), Clear (:442)
    //   OutputBuffer_PreDispatch : Array<BrnTraffic::VehicleRenderInfo,64u> maTrafficRenderInfos (:457)
    //                              Construct (:452), Clear (:455)
    //
    // NO Get*() ACCESSORS EXIST ON EITHER BUFFER AND NONE MAY BE ADDED. The DWARF
    // (structs at :436 and :449) lists exactly four methods between the pair, the two
    // Constructs and the two Clears, and emits both data members outside any access
    // label, i.e. public. The X360 ledger carries only those same three symbols
    // (OutputBuffer_PreDispatch::Clear plus the two Constructs). Consumers therefore
    // read the members directly, which is what the console does inline with no call:
    //     lpInput->mCameraPosition            // camera position     (:444)
    //     lpInput->maTrafficEntityIds         // visible-entity list (:445)
    //     lpOutput->maTrafficRenderInfos      // render-info array   (:457)
    // The two Set* writers below de-inline WorldModule::GenerateDispatchLists' own
    // stores; they are the only non-DWARF surface here and need no read-side twin.
    //
    // HOST LAYOUT: every member of both buffers is pointer-free (a Vector3, a 4-byte
    // EntityId array, a 12-byte VehicleRenderInfo array), so the console spans do
    // survive to x64 and _AssertLayout pins them as pointer-invariant facts. The one
    // difference is the leading status byte: CgsModule::IOBuffer is a 1-byte FlagSet8,
    // so the explicit pads below reproduce the console's start-of-payload alignment.
    //
    // Neither buffer holds an EventQueue, VariableEventQueue or RequestInterface, but
    // both hold an Array<T,N>, which is unusable until its count word leaves the
    // KI_UNCONSTRUCTED(-1) sentinel. The console's Construct writes that word for both
    // (the +2632 and +772 stores above), and so does each Construct() below.
    // ------------------------------------------------------------------------
    class InputBuffer_PreDispatch : public CgsModule::IOBuffer
    {
    public:
        // @0x8275CEE8 -- run by CreateIOBuffer<InputBuffer_PreDispatch> @0x827B7250.
        void Construct();

        // DWARF :442. Not separately exported (the console inlines it). Empty means "no visible
        // entities", i.e. the Array's count word back to 0, the same store Construct makes.
        void Clear();

        // The two de-inlined writers WorldModule::GenerateDispatchLists fills the buffer through
        // (@0x827D1CE8 writes both members inline between Construct() and its LockForRead()).
        // There is no write lock around them, so neither asserts one.
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

        // PUBLIC, per the DWARF. Producer: TrafficEntityModule::PreDispatchUpdate @0x8274D900.
        // Consumers: WorldModule::CalculateVehicleLODs (writes VehicleRenderInfo::mLOD) and
        // TrafficEntityModule::GenerateDispatchLists @0x8273B280.
        Array<BrnTraffic::VehicleRenderInfo, 64u> maTrafficRenderInfos;   // console +4     :457
    };

    // ------------------------------------------------------------------------
    // [T1-rinfo] BRING-UP PROBE -- NOT IN THE X360 BINARY. DELETE WHEN STABLE.
    // Latched report of how many VehicleRenderInfos the traffic module produced. Prints only
    // when the count changes, and only under BRN_TRAFFIC_DIAG. A free function, so it adds
    // nothing to the buffer's shape. Call it right after
    // TrafficEntityModule::PreDispatchUpdate returns; OutputBuffer_PreDispatch::Clear() also
    // calls it, so it reports even before the call site is wired.
    // ------------------------------------------------------------------------
    void T1Diag_ReportTrafficRenderInfoCount( const OutputBuffer_PreDispatch& lrBuffer );

    // OutputBuffer_Prepare  (DWARF :2/:115, prepare/boot output buffer)
    // ============================================================================
    // Owns the scene-input interface (@16, read 0x8279F988 [baked 126] / write 0x827109E0) and the
    // 4096-slot resource-request interface (@818784, read 0x8279FA30 [baked 129] / write 0x82710A88).
    //
    // The scene-input seat must stay the real type. As an untyped, never-Constructed byte block
    // it is what WorldModule::BridgeTrafficModuleToSceneModule_Prepare @0x827AB300
    // reinterpret_casts to the 25-queue aggregate and Appends into, reading 25 mpEvents/miLength
    // pairs out of whatever the IO stack's previous tenant left. The type is attested, not
    // inferred: OutputBuffer_Prepare::Construct @0x82761740 calls
    // InSceneUpdateInterface::Construct(this+0x10), and 16 + 818768 == 818784, the next member.
    class OutputBuffer_Prepare : public CgsModule::IOBuffer
    {
    public:
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface;  // :12
        // The real pipe, needed by the WorldModule::Prepare /
        // BridgeTrafficResourceRequestsToOutput consumers.
        typedef BrnResource::GameDataIO::RequestInterface<4096> ResourceRequestInterface;

        // @0x82761740 -- run by CreateIOBuffer<OutputBuffer_Prepare> @0x827B5C70. Without it the
        // base IOBuffer::Construct runs alone and the aggregate's 25 queues keep a foreign
        // mpEvents.
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
        // No pad here: the seat holds the real aggregate, whose host sizeof already exceeds the
        // 818,768-byte console span (its queues carry an 8-byte mpEvents where the console has
        // 4). Adding the pad as well would waste 818 KB.
        ResourceRequestInterface mResourceRequestInterface;                   // console +818784
    };

    // ============================================================================
    // OutputBuffer_PrePhysics  (DWARF :403, pre-physics output buffer)
    // ============================================================================
    // All three interfaces must stay real types.
    // WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics @0x827AAEC0 reads all three and
    // merges them into the physics input, and VehicleDriverInputInterface::Append alone moves
    // 5,284 bytes, so a 1-byte stand-in is a ~5.3 KB heap overrun on the first frame.
    //
    // Three witnesses agree on the layout:
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
    // No offsetof static_assert here: VehicleInputInterface's embedded queues carry widened
    // pointers on x64, so a console byte offset would fail on a layout that is right. Parity is
    // by named member and sequence.
    class OutputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
    public:
        // DWARF :79/:80/:81 -- the real physics-side types, the same three the race-car
        // pre-physics buffer owns.
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

        // DWARF :319/:320. The +149280 byte Construct zeroes. Inline here because the console has
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
