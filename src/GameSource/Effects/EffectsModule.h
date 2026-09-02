#ifndef GAMESOURCE_EFFECTS_EFFECTSMODULE_H
#define GAMESOURCE_EFFECTS_EFFECTSMODULE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // Vector3 / Matrix44Affine / CgsID
#include "SharedClasses/BrnSharedConstants.h"                            // BrnUpdateSet
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"       // CgsModule::ModuleSingleBuffered (base)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"     // CgsModule::EventReceiverQueue<2048,16>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"    // CgsResource::ResourceHandle
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                    // CgsNumeric::Random (BY VALUE)
#include "GameSource/Effects/Particles/ParticleModule.h"                 // BrnParticle::ParticleModule (BY VALUE)
#include "GameSource/Effects/ActiveRaceCarData.h"                        // BrnEffects::ActiveRaceCarData (x8, BY VALUE)
#include "GameSource/Effects/BrnEffectsDebugComponent.h"                 // BrnEffects::EffectsDebugComponent (BY VALUE)
#include "GameSource/Effects/BrnCrashTriangleCache.h"                    // BrnEffects::BrnCrashTriangleCache (BY VALUE)
#include "GameSource/Effects/BrnEffectsGlassManager.h"                   // BrnEffects::BrnEffectsGlassManager (BY VALUE)
#include "GameSource/AttribSys/Generated/classes/sparkeffect.h"          // Attrib::Gen::sparkeffect (x4)
#include "GameSource/AttribSys/Generated/classes/debrisparams.h"         // Attrib::Gen::debrisparams (x3)
#include "GameSource/AttribSys/Generated/classes/junkyardlocators.h"     // Attrib::Gen::junkyardlocators
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"          // Attrib::Gen::surfacelist
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // BrnGameState::GameStateModuleIO::EGameModeType
#include "GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h"   // BrnReplays::EffectsSerialiser (BY VALUE, X360-only member)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface / BoostOutputInfo / EActiveRaceCarIndex
#include "rw/core/base/ostypes.h"                                        // RwRGBAReal

// ============================================================================
// GameSource/Effects/EffectsModule.h
//
// BrnEffects::EffectsModule -- the game's effects (VFX) module: a
// CgsModule::ModuleSingleBuffered that owns the particle module, the per-car
// effect state (wheel skid smoke + tyre-mark emitters, boost, jump, crash
// bursts), the crash triangle cache, the glass-smash manager and the effects
// replay serialiser. BrnGame::BrnGameModule embeds it BY VALUE (mEffectsModule)
// and drives it through the module lifecycle:
//
//   Construct @0x8228FE98   Prepare @0x8229E690 (-> PrepareResources @0x8229D8A8,
//   ParticleModule::Prepare / PostPreparePrepare)   PostWorldPreparePrepare
//   @0x822902F0 (the loading spine's LoadWorldCollision tail)   Release @0x8227FCA8
//   Destruct @0x8227FD78   Update @0x8229EC28 (once per simulation sub-step, from
//   BrnGameModule::DoUpdate_Effects @0x823DD0A8)   GenerateDispatchLists @0x82296668
//   (once per frame, from DoDispatch @0x823DC458)   RestartEffects @0x822793E0.
//
// 2026-09-02 (tyre-mark wave): RETYPED BY NAME. This used to be an opaque
// `u8 mOpaqueBody[0x2F550]` whose sub-objects the constructor placement-new'd at
// console byte offsets (and, once ParticleModule grew to its real host size, ON TOP
// OF EACH OTHER). Members are now the DWARF's (DecFIGS EffectsModule.h:552-634), in
// the DWARF's order, each typed with its committed type; the console byte offset
// each one was pinned at by the ARTIST asm is given per member. Four members are
// X360-only (absent from the FIGS DWARF, present in every ARTIST body that touches
// the object): muSlipStreamEffectHandle, mEffectsSerialiser and the two bools the
// Update ladder raises at +0x2F5A8/9. Nothing on the host addresses this object by
// byte offset -- the console layout pins the member SET, not host positions.
//
// LAYOUT (X360, 4-byte pointers; the base ModuleSingleBuffered is 0x228 bytes):
//   +0x00228 mEffectInstanceHandle      +0x0022C mQAEffectHandle
//   +0x00230 liEffectInstanceIndex      +0x00234 mResetAttribs
//   +0x00238 mePrepareStage             +0x0023C meReleaseStage
//   +0x00240 meResourceStage            +0x00244 mReceiverQueue (EventReceiverQueue<2048,16>)
//   +0x00A64 mSchemaResourceHandle      +0x00A6C mVaultResourceHandle
//   +0x00A80 mParticleModule            +0x2C280 mCarStateCache
//   +0x2C330 mDebugComponent            +0x2C3C0 mRandom
//   +0x2C3F0 maActiveRaceCarData[8] (stride 384)
//   +0x2CFF0 mafAccumulatedParticleCountCrash[9]   +0x2D014 mafAccumulatedParticleCountTyres[9]
//   +0x2D038 mafTimeUntilNextDebrisBurst[8]        +0x2D058 mafTimeUntilNextSparksBurst[8]
//   +0x2D078 mafCrashingTrailAccumulators[8][6]    +0x2D140 maRaceCarPreviousTransforms[8]
//   +0x2D340 meCurrentGameMode          +0x2D344 mbEventIntroActive
//   +0x2D348 mSparkParams[4]            +0x2D388/398/3A8 the three debrisparams
//   +0x2D3B8 mJunkYardLocatorsData      +0x2D3C8 mSurfaceList
//   +0x2D400 mCrashTriangleCache        +0x2F280 mGlassSmashManager
//   +0x2F510 maShowtimeBounceEffectHandles[3]  +0x2F51C muNextShowtimeBounceEffect
//   +0x2F520 mfLastShowtimeBounceEffectTime    +0x2F524 maJunkyardEffectHandles[10]
//   +0x2F54C muSlipStreamEffectHandle   +0x2F550 mEffectsSerialiser (0x58)
//   +0x2F5A8 / +0x2F5A9 the two Update-raised bools
// ============================================================================

namespace CgsMemory   { class HeapMalloc; }
namespace CgsModule   { class IOBufferStack; }
namespace CgsResource { namespace Events { struct AcquireResourceResponse; } }
namespace BrnResource { namespace GameDataIO { class AllocatorList; } }
namespace BrnGame     { struct DispatchThreadInputBuffer; }
namespace BrnDirector { namespace Camera { class Camera; } }
namespace BrnPhysics  { namespace Vehicle { struct RaceCarState; } }
namespace BrnPhysics  { namespace Deformation { struct DeformationOutputInterface; } }
namespace BrnPhysics  { namespace ContactSpy { struct ContactSpyInterface; } }
namespace BrnSound    { namespace Module { namespace Io { struct AudioEffectsMessageQueue; } } }

namespace BrnEffects
{
    namespace EffectsIO
    {
        struct InputBuffer;
        struct OutputBuffer;
        struct DispatchInputBuffer;
    }
    class  RaceCarParticleEffectHelper;

    // ------------------------------------------------------------------------
    // BrnEffects::EffectsModuleParams (DWARF EffectsModule.h:59) -- the per-update
    // parameter block EffectsModule::Update builds on its stack and hands every
    // per-car / contact handler.
    //
    // ⚠ THE ARTIST BLOCK IS 32 BYTES: mDt / mTime / (pad) / mCameraPosition. The
    // FIGS DWARF adds `const Camera* mpCamera` at :64; the ARTIST Update @0x8229EC28
    // never stores a camera into the block (it passes the camera as a separate
    // argument to ProcessCarContactQueues / HandlePlayerTriangleCache) and
    // UpdateActiveRaceCars @0x8229DB30 copies exactly four doublewords of it into
    // CarState (+0x10..+0x2F). The merge-window delta is recorded, not imported.
    // ------------------------------------------------------------------------
    struct EffectsModuleParams
    {
        f32     mDt;              // +0x00  the simulation time step
        f32     mTime;            // +0x04  the absolute simulation time
        u8      mPad08[8];        // +0x08  (alignment to the 16-byte vector)
        Vector3 mCameraPosition;  // +0x10  the effects camera's world position
    };

    // ------------------------------------------------------------------------
    // BrnEffects::CarState (DWARF EffectsModule.h:68) -- the per-car, per-update
    // snapshot the effects state machines (boost / jump / wheel) consult.
    //
    // The ARTIST record is WIDER than the FIGS one -- it leads with the race-car
    // index and carries the replay serialiser pointer, the boost bool / amount /
    // type and the race-car pointer inline. Every offset below is the one
    // UpdateActiveRaceCars @0x8229DB30 STORES it at (v104 @sp+0x90 is the record):
    //   +0x00 muRaceCarIndex (v104[0])           +0x10 mEffectsModuleParams (4 dwords, v105..v108)
    //   +0x30 mpEffectsSerialiser (v109)          +0x34 mbIsBoosting (v110 <- BoostOutputInfo+0)
    //   +0x38 meBoostType (v111 <- info+32)       +0x3C mfBoostAmount (v112[0] <- info+16)
    //   +0x40 mpCarState (v112[1])                +0x44 mfSpeedMPH (v112[2] <- RaceCarState+972)
    //   +0x48 mfExhaustPopIntensity (v112[3])     +0x4C mbExhaustPopThisFrame (v113)
    //   +0x4D mbCrashing (v114 <- RaceCarState+1098)  +0x4E mbJumping (v115)  +0x4F mbEngineRunning (v116)
    // ⚠ CORRECTION (2026-09-02): the previous header named +0x4C mbCrashing and +0x4D
    // mbJumping. The DWARF order (mbExhaustPopThisFrame, mbCrashing, mbJumping,
    // mbEngineRunning at :75-78) and the stores above put the crash flag at +0x4D.
    // ------------------------------------------------------------------------
    struct CarState
    {
        u32                               muRaceCarIndex;         // +0x00 (ARTIST-only lead)
        u8                                mPad04[0x0C];           // +0x04
        EffectsModuleParams               mEffectsModuleParams;   // +0x10 (:70) -- mDt @+0x10, mTime @+0x14, camera pos @+0x20
        BrnReplays::EffectsSerialiser*    mpEffectsSerialiser;    // +0x30 (ARTIST-only)
        bool                              mbIsBoosting;           // +0x34 (ARTIST-only; BoostOutputInfo::mbIsBoosting)
        u8                                mPad35[0x03];           // +0x35
        s32                               meBoostType;            // +0x38 (ARTIST-only; BrnWorld::EBoostType, -1 none)
        f32                               mfBoostAmount;          // +0x3C (ARTIST-only; BoostOutputInfo::mfBoostAmount)
        const BrnPhysics::Vehicle::RaceCarState* mpCarState;      // +0x40 (:72)
        f32                               mfSpeedMPH;             // +0x44 (:73)  RaceCarState::mfSpeedMPH
        f32                               mfExhaustPopIntensity;  // +0x48 (:74)  the audio exhaust-pop event's value
        bool                              mbExhaustPopThisFrame;  // +0x4C (:75)
        bool                              mbCrashing;             // +0x4D (:76)
        bool                              mbJumping;              // +0x4E (:77)
        bool                              mbEngineRunning;        // +0x4F (:78)

        // :81 (DWARF inline).
        bool IsBoosting() const { return mbIsBoosting; }
        // The two params every machine reads (the +0x10 / +0x14 words).
        f32  GetDt() const   { return mEffectsModuleParams.mDt; }
        f32  GetTime() const { return mEffectsModuleParams.mTime; }
    };

    class EffectsModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // DWARF EffectsModule.h:99 / :108 / :115.
        enum EPrepareStage
        {
            E_PREPARESTAGE_START                = 0,
            E_PREPARESTAGE_MANAGER              = 1,
            E_PREPARESTAGE_RESOURCES            = 2,
            E_PREPARESTAGE_POST_PREPARE_PREPARE = 3,
            E_PREPARESTAGE_DONE                 = 4,
        };
        enum EReleaseStage
        {
            E_RELEASESTAGE_START   = 0,
            E_RELEASESTAGE_MANAGER = 1,
            E_RELEASESTAGE_DONE    = 2,
        };
        enum EResourceAcquireStage
        {
            E_RESOURCESTAGE_START             = 0,
            E_RESOURCESTAGE_LOADING_VAULT     = 1,
            E_RESOURCESTAGE_ACQUIRING_VAULT   = 2,
            E_RESOURCESTAGE_REGISTERING_VAULT = 3,
            E_RESOURCESTAGE_DONE              = 4,
        };

        static const u32 KU_MAX_JUNKYARD_VFX          = 10;   // DWARF :633
        static const u32 KU_MAX_SHOWTIME_BOUNCE_EFFECTS = 3;  // DWARF :627
        static const u32 KU_NUM_ACTIVE_RACE_CARS      = 8;    // E_ACTIVE_RACE_CAR_INDEX_COUNT
        static const u32 KU_NUM_SPARK_PARAMS          = 4;    // DWARF :607 sparkeffect[4]

        // ------------------------------------------------------------------------
        // DWARF EffectsModule.h:131 -- the player's race-car snapshot the render
        // requests read (GenerateRenderRequests copies it into the layer-0 effects
        // frame). Update @0x8229EC28 fills the velocities / speed / steering / camera
        // flag (+0x2C300 / +0x2C310 / +0x2C320 / +0x2C324 / +0x2C328).
        // ------------------------------------------------------------------------
        struct TempRaceCarStateCache
        {
            Matrix44Affine mCarTransform;      // :133  +0x2C280
            Matrix44Affine mCameraTransform;   // :134  +0x2C2C0
            Vector3        mvLinearVelocity;   // :135  +0x2C300
            Vector3        mvAngularVelocity;  // :136  +0x2C310
            f32            mfSpeedMPH;         // :137  +0x2C320
            f32            mfSteering;         // :138  +0x2C324
            bool           mbIsGameCamera;     // :139  +0x2C328

            // :142-:220 (DWARF inlines).
            void SetLinearVelocity(Vector3 lv)        { mvLinearVelocity = lv; }
            void SetAngularVelocity(Vector3 lv)       { mvAngularVelocity = lv; }
            void SetSpeedMPH(f32 lf)                  { mfSpeedMPH = lf; }
            void SetSteering(f32 lf)                  { mfSteering = lf; }
            void SetCarTransform(const Matrix44Affine& lm)    { mCarTransform = lm; }
            void SetIsRacingGameplayCamera(bool lb)   { mbIsGameCamera = lb; }
            void SetCameraTransform(const Matrix44Affine& lm) { mCameraTransform = lm; }
            Vector3 GetLinearVelocity() const         { return mvLinearVelocity; }
            Vector3 GetAngularVelocity() const        { return mvAngularVelocity; }
            f32     GetSpeedMPH() const               { return mfSpeedMPH; }
            f32     GetSteering() const               { return mfSteering; }
            const Matrix44Affine& GetCarTransform() const    { return mCarTransform; }
            bool    GetIsRacingGameplayCamera() const { return mbIsGameCamera; }
            const Matrix44Affine& GetCameraTransform() const { return mCameraTransform; }
        };

        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface RCEntityActiveRaceCarOutputInterface;
        typedef BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo                     BoostOutputInfo;
        // EActiveRaceCarIndex lives in the GLOBAL namespace (GameSource/BurnoutConstants.h), not in
        // BrnWorld::RaceCarEntityModuleIO -- that header only USES it.
        typedef ::EActiveRaceCarIndex                                                EActiveRaceCarIndex;
        typedef BrnPhysics::Deformation::DeformationOutputInterface                  DeformationOutputInterface;
        typedef BrnSound::Module::Io::AudioEffectsMessageQueue                       AudioEffectsMessageQueue;

        // X360 ARTIST @0x827E35E0 -- the C++ constructor: every member's own construction
        // (the console placement-news / vector-constructs each sub-object at the offsets
        // above); the only scalar stores it makes of its own are the debug component's
        // defaults and the random generator's priming, which those members' constructors
        // now own. Body in EffectsModule.cpp.
        EffectsModule();

        // ---- the module lifecycle (EffectsModule.cpp) ---------------------------------
        // @0x8228FE98 (DWARF EffectsModule.cpp:335). vtable slot 0 on the console.
        void Construct() override;
        // @0x8229E690 (DWARF :415). `bool Prepare(const AllocatorList*, IOBufferStack*,
        // OutputBuffer*)`: the 5-stage ladder. Returns false while still preparing.
        bool Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                     CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                     EffectsIO::OutputBuffer* lpOutputBuffer);
        // @0x822902F0 (DWARF :556). The loading spine's LoadWorldCollision tail: re-point
        // the surface list at the world's collection and push every surface's skid-mark
        // colour pair into the trail system (TrailSystem::UpdateTrailType).
        void PostWorldPreparePrepare();
        // @0x8227FCA8 (DWARF :767) / @0x8227FD78 (DWARF :825).
        bool Release() override;
        void Destruct() override;
        // @0x8229EC28 (DWARF :852). One simulation sub-step of the whole effects system.
        void Update(CgsModule::IOBufferStack* lpInputBufferStack,
                    CgsModule::IOBufferStack* lpOutputBufferStack,
                    const EffectsIO::InputBuffer* lpInputBuffer,
                    EffectsIO::OutputBuffer* lpOutputBuffer,
                    BrnUpdateSet leUpdateSet);
        // @0x82296668 (DWARF :1345). Once per frame from DoDispatch: the render requests,
        // then the particle module's dispatch input + its render-data publish.
        void GenerateDispatchLists(CgsModule::IOBufferStack* lpInputBufferStack,
                                   const EffectsIO::DispatchInputBuffer* lpDispatchInputBuffer,
                                   BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer);
        // @0x822793E0 (DWARF :2110). Static: raises the QA "restart effects" latch.
        static void RestartEffects();

        // The module's base Update() (CgsModule::Module slot) is not the entry the game
        // module drives; the 5-arg Update above is (console vtable +68).
        void Update() override {}

        // ---- accessors (DWARF :303 / :308 / :313) --------------------------------------
        CgsNumeric::Random&          RandomNumberGenerator() { return mRandom; }
        BrnParticle::ParticleModule& ParticleModule()        { return mParticleModule; }
        Attrib::Gen::surfacelist&    SurfaceList()           { return mSurfaceList; }

        // One of the KU_MAX_JUNKYARD_VFX junkyard effect handles (maJunkyardEffectHandles[]).
        u32 GetJunkyardEffectHandle(u32 luIndex) const;
        // True while a junkyard VFX edit session is live (the particle module's
        // mbIsInJunkyard, which Update raises on the junkyard camera flag).
        bool IsJunkyardVfxActive() const { return mParticleModule.mbIsInJunkyard; }

        // Prepare's ladder position (the loading spine logs it).
        EPrepareStage GetPrepareStage() const { return mePrepareStage; }
        // [effects-load] the trace in LoadingScriptedState::LoadEffectsModule needs the
        // particle module's FX-bundle ladder position; it is the half that can stall.
        BrnParticle::ParticleModule& ParticleModuleRef() { return mParticleModule; }

    private:
        // ---- the private lifecycle helpers (DWARF :632 / :653 / :1197 / :588 / :2946) ----
        // @0x8229D8A8. The post-fx vault + colour-cube dictionary acquire ladder.
        bool PrepareResources(EffectsIO::OutputBuffer* lpOutputBuffer);
        // @0x8227F098. Iterate the module's resource-acquire reply queue.
        const CgsResource::Events::AcquireResourceResponse*
        GetNextAcquireResourceResponse(const CgsResource::Events::AcquireResourceResponse* lpPrevious);
        // @0x8227FF10. The post-fx effects frames (DOF / blur / colour cubes).
        void GenerateRenderRequests(const EffectsIO::DispatchInputBuffer* lpDispatchInputBuffer);
        // @0x82290510. Push the 12 native simple-particle parameter sets into the arrays.
        void LoadNativeParticleParams();
        // @0x822803C0.
        const BrnPhysics::Vehicle::RaceCarState*
        GetPlayerRaceCarState(const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars);

        // ---- the per-car pipeline (DWARF :2976 / :3069 / :1743 / :2921 / :2083) ----------
        void ProcessActiveRaceCars(const EffectsModuleParams& lrParams,
                                   const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                   const BoostOutputInfo* lpBoostInfos,
                                   const DeformationOutputInterface* lpDeformation,
                                   const AudioEffectsMessageQueue* lpAudioEffects,
                                   bool lbIsGameCamera);
        void UpdateActiveRaceCars(EActiveRaceCarIndex lePlayerIndex,
                                  const EffectsModuleParams& lrParams,
                                  const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                  const BoostOutputInfo* lpBoostInfos,
                                  const DeformationOutputInterface* lpDeformation,
                                  const AudioEffectsMessageQueue* lpAudioEffects,
                                  bool lbIsGameCamera);
        void HandleWheels(CarState& lrCarState, RaceCarParticleEffectHelper& lrHelper);
        void HandleJumpAndLandingEffects(CarState& lrCarState, RaceCarParticleEffectHelper& lrHelper,
                                         f32 lfDt, f32 lfTime, f32 lfGroundPositionY);
        void HandlePlayerTriangleCache(const EffectsIO::InputBuffer* lpInputBuffer,
                                       const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                                       ActiveRaceCarData& lrActiveRaceCar);
        // @0x82296FD8 (DWARF :2206). The game-action consumer.
        void HandleGameActions(const CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue,
                               const EffectsIO::InputBuffer* lpInputBuffer);
        // @0x822926C8. Drive the convoy slip-stream LION effect.
        void HandleConvoySlipStream(f32 lfBlend, u32 luUnused,
                                    const rw::math::vpu::Matrix44Affine& lrTransform);

        // ---- NOT RECONSTRUCTED on this build (each logs ONCE when reached, then returns) ----
        // See the banner in EffectsModule.cpp: these are the spark / debris / glass /
        // crash-trail / junkyard-editor / QA arms off the tyre-mark path. They are neither
        // trap-stubs (a CGS_ASSERT there would kill every crash / junkyard run on the shared
        // box) nor silent: every one announces itself in BrnGame.log the first time.
        void HandleCrashingTrail(ActiveRaceCarData& lrActiveRaceCar, f32 lfDt, f32 lfTime,
                                 const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                                 EActiveRaceCarIndex leIndex);
        void ProcessCarContactQueues(const EffectsModuleParams& lrParams,
                                     const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                     const BrnPhysics::ContactSpy::ContactSpyInterface* lpContactSpy,
                                     const BrnDirector::Camera::Camera* lpCamera);
        void HandleGlassSmashEventsForAllCars(const EffectsIO::InputBuffer* lpInputBuffer,
                                              const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                              f32 lfDt, f32 lfTime);
        void HandleQADebugTests(f32 lfDt, f32 lfTime, const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState);
        void HandleShowtimeTrafficBounce(const void* lpJustBouncedAction, const EffectsIO::InputBuffer* lpInputBuffer);
        void JunkyardVfxStart(Vector3 lvCameraPosition);
        void JunkyardVfxStop();

        // ---- members (DWARF EffectsModule.h:552-634, in order; X360 offsets above) ------
        u32                                      mEffectInstanceHandle;              // :552  +0x228   Construct: -1
        u32                                      mQAEffectHandle;                    // :553  +0x22C   Construct: -1
        s32                                      liEffectInstanceIndex;              // :556  +0x230   (DWARF's own name)
        bool                                     mResetAttribs;                      // :558  +0x234   Construct: 0
        EPrepareStage                            mePrepareStage;                     // :560  +0x238   Construct: 0
        EReleaseStage                            meReleaseStage;                     // :561  +0x23C   Construct: 2 (DONE)
        EResourceAcquireStage                    meResourceStage;                    // :564  +0x240   Construct: 0
        CgsModule::EventReceiverQueue<2048, 16>  mReceiverQueue;                     // :567  +0x244
        CgsResource::ResourceHandle              mSchemaResourceHandle;              // :570  +0xA64
        CgsResource::ResourceHandle              mVaultResourceHandle;               // :571  +0xA6C
        BrnParticle::ParticleModule              mParticleModule;                    // :574  +0xA80
        TempRaceCarStateCache                    mCarStateCache;                     // :577  +0x2C280
        EffectsDebugComponent                    mDebugComponent;                    // :580  +0x2C330
        CgsMemory::HeapMalloc*                   mpHeapMalloc;                       // :583
        CgsNumeric::Random                       mRandom;                            // :585  +0x2C3C0
        ActiveRaceCarData                        maActiveRaceCarData[KU_NUM_ACTIVE_RACE_CARS]; // :588 +0x2C3F0 (stride 384)
        f32                                      mafAccumulatedParticleCountCrash[9];// :591  +0x2CFF0
        f32                                      mafAccumulatedParticleCountTyres[9];// :592  +0x2D014
        f32                                      mafTimeUntilNextDebrisBurst[KU_NUM_ACTIVE_RACE_CARS]; // :595 +0x2D038
        f32                                      mafTimeUntilNextSparksBurst[KU_NUM_ACTIVE_RACE_CARS]; // :596 +0x2D058
        f32                                      mafCrashingTrailAccumulators[KU_NUM_ACTIVE_RACE_CARS][6]; // :599 +0x2D078
        Matrix44Affine                           maRaceCarPreviousTransforms[KU_NUM_ACTIVE_RACE_CARS];   // :600 +0x2D140
        BrnGameState::GameStateModuleIO::EGameModeType meCurrentGameMode;            // :603  +0x2D340  Construct: -1
        bool                                     mbEventIntroActive;                 // :604  +0x2D344  Construct: 0
        Attrib::Gen::sparkeffect                 mSparkParams[KU_NUM_SPARK_PARAMS];  // :607  +0x2D348 (16 each)
        Attrib::Gen::debrisparams                mCrashingDebrisParams;              // :609  +0x2D388
        Attrib::Gen::debrisparams                mRoadRageDebrisParams;              // :610  +0x2D398
        Attrib::Gen::debrisparams                mAIRaceCarCrashingTrailDebris;      // :611  +0x2D3A8
        Attrib::Gen::junkyardlocators            mJunkYardLocatorsData;              // :614  +0x2D3B8
        Attrib::Gen::surfacelist                 mSurfaceList;                       // :617  +0x2D3C8
        BrnCrashTriangleCache                    mCrashTriangleCache;                // :621  +0x2D400
        BrnEffectsGlassManager                   mGlassSmashManager;                 // :624  +0x2F280
        u32                                      maShowtimeBounceEffectHandles[KU_MAX_SHOWTIME_BOUNCE_EFFECTS]; // :628 +0x2F510 Construct: -1
        u32                                      muNextShowtimeBounceEffect;         // :629  +0x2F51C  Construct: 0
        f32                                      mfLastShowtimeBounceEffectTime;     // :630  +0x2F520  Construct: 0.0
        u32                                      maJunkyardEffectHandles[KU_MAX_JUNKYARD_VFX];  // :634 +0x2F524 Construct: -1
        // ---- ARTIST-only tail (not in the FIGS DWARF) --------------------------------------
        u32                                      muSlipStreamEffectHandle;           // +0x2F54C  Construct: -1 (HandleConvoySlipStream / HandleGameActions)
        BrnReplays::EffectsSerialiser            mEffectsSerialiser;                 // +0x2F550  Construct: EffectsSerialiser::Construct
        bool                                     mbUpdateRan2F5A8;                   // +0x2F5A8  Update: = 1 every non-suspended step (FLAG: name inferred, no reader found)
        bool                                     mbUpdateRan2F5A9;                   // +0x2F5A9  Update: = 1 (same)
    };
}

#endif // GAMESOURCE_EFFECTS_EFFECTSMODULE_H
