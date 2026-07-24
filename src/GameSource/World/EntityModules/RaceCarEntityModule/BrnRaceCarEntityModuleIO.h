// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h
//
// "BrnWorld::RaceCarEntityModuleIO" is a NAMESPACE. The ledger functions are inline
// IOBuffer accessors spread across 10 per-frame IO-buffer structs (each derives
// CgsModule::IOBuffer), plus 2 per-index element accessors on the output interfaces
// (homed in BrnRaceCarEntityModuleOutputInterface.h) and 3 EventQueue template
// instantiations for the traffic-system events (homed in BrnRaceCarToTrafficInterface.h).
// Layout/signatures are X360-gated against the FULL DecFIGS DWARF for
// BrnRaceCarEntityModuleIO.h (buffers laid out at DWARF lines 118..652) and the two
// SharedIO headers.
//
// ACCESSOR SHAPE (X360 binary, authoritative): every Get*Interface()/Get*Queue() body
// asserts the buffer's lock bit then returns &member. Lock bit -> const-ness:
//   ">>3 &1" (eStatusLockedForWrite 0x08) => NON-const mutable getter;
//   ">>4 &1" (eStatusLockedForRead  0x10) => CONST getter.
// Offsets are NOT hardcoded -- bodies return &member, which lands the right byte offset
// once each sub-interface payload carries its real size in its own TU.
//
// SUB-INTERFACE PAYLOADS (this assemble pass): the ~27 sub-interface/queue payloads
// embedded BY VALUE are now COMPLETE types living in their CANONICAL homes/namespaces
// (the stub phase landed minimal-complete sized slices per the committed
// CheckpointData/OpponentData pattern). This header therefore #includes those homes and
// references each payload by its canonical fully-qualified name (per-buffer typedefs are
// retargeted to the canonical FQN, member types unchanged in spelling). The two
// BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_* payloads had no stub group, so a
// minimal-complete local slice is defined inline below (see RISK note in the assemble
// report); they are accessed only by-name, so byte-exact size is not required.
//
// CORRECTIONS baked in (verified punch-list):
//  (1) X360 0x8279E310 is the NON-const GetSceneInputInterface() of InputBuffer_PostPhysics
//      (write-lock; returns &mSceneInputInterface), NOT a non-const GetContactSpyInterface.
//  (2) There is NO non-const GetContactSpyInterface: InputBuffer_PostPhysics declares only
//      the const getter (:528) + SetContactSpyInterface (:529).
//  (3) EActiveRaceCarEngineState uses the full DWARF enumerators (in the OutputInterface header).
#pragma once

#include "types.hpp"                                                       // s8/s32/u8/u16/u32/f32
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                      // CgsModule::IOBuffer base
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                    // CgsModule::EventQueue<T,N>
#include "BrnCommonTypes.h"                                                 // CgsID, Vector3, Vector4
#include "GameSource/BurnoutConstants.h"                                    // EActiveRaceCarIndex, EGlobalRaceCarIndex
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntity* interfaces, AudioCarDataLoadedEvent
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarToTrafficInterface.h"          // RaceCarToTrafficInterface + Create/Remove Rival events

// ---- Canonical homes of the sub-interface/queue payloads embedded by value ----------
// (Each was reconstructed as a minimal-complete sized slice in its DWARF home + namespace.
//  These replace the .ref's local namespace-scope forward declarations.)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"          // BrnPhysics::Vehicle::VehicleInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"    // BrnPhysics::Vehicle::VehicleDriverInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"         // BrnPhysics::Vehicle::VehicleOutputInterface + VehicleManagerOutputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEffectsInputInterface.h"   // BrnPhysics::Vehicle::VehicleEffectsInputInterface
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h"      // BrnWorld::CrashIO::RaceCarOutputInterface (CrashInterface)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h" // BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene (canonical 544B)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h" // BrnWorld::PlayerVehicleControls
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"                          // BrnAI::AIModuleIO::RaceCarAIInterface + AIRaceCarInterface
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"                     // BrnAI::AIModuleIO::AIModuleRequestInterface
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"                      // BrnAI::AIModuleIO::AIModuleResultInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"            // CgsSceneManager::SceneManagerIO::InSceneUpdateInterface (SceneInputInterface)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQuery.h"            // CgsSceneManager::SceneManagerIO::SceneCoarseQueryQueue
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"                  // CgsSceneManager::SceneManagerIO::SceneFineLineTestQueue
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // BrnPhysics::Deformation::DeformationOutputInterface (+ ...ForEntityModules)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                    // BrnGameState::GameStateModuleIO::ScoringOutputInterface + OnlineScoringOutputInterface
#include "GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.h"                // BrnDirector::BrnDirectorVehicleInputInterface
#include "GameSource/Replays/BrnReplayStatusInterface.h"                                  // BrnReplays::ReplayIO::StatusInterface (ReplayStatusInterface)
#include "GameSource/Director/Camera/Camera.h"                                            // BrnDirector::Camera::Camera
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"                       // BrnPhysics::ContactSpy::ContactSpyInterface
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleIOQueues.h" // local queue slices (GameActionQueue/GameEventQueue/PotentialContactQueue/SceneResultQueue/TakedownEventQueue/ResourceRequestInterface)

namespace BrnNetwork { enum EPaybackType : s32; }
namespace CgsGraphics { struct DispatchFrame; }
// RECONCILED 2026-07-24 (ODR fix, see BrnRendererModuleIO.h): BrnBlobbyShadowBuffer
// is a NESTED class of the real BrnBlobbyShadowManager CLASS -- include the home.
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"
// RECONCILED 2026-07-24 (ODR fix, see BrnRendererModuleIO.h): BrnSubmissionInterface is a
// NESTED class of the real BrnCoronaManager CLASS. The partial-class spelling that used to
// stand here re-declared the enclosing class and collided (C2011) with the real definition
// in every TU that saw both -- include the real home instead.
#include "GameSource/Graphics/BrnCoronaManager.h"   // BrnCoronaManager::BrnSubmissionInterface

namespace BrnWorld
{
struct ShadowMap;                                                          // :652 mpShadowMap target

namespace RaceCarEntityModuleIO
{
    // ---- Remaining locally-homed payloads -----------------------------------------
    // TimerStatusInterface (:254 member type): the DecFIGS DWARF for InputBuffer_PreScene
    // (BrnRaceCarEntityModuleIO.h:53) declares `TimerStatusInterface mTimerStatusInterface;`
    // -- a BY-VALUE member, NOT a pointer. The X360 getter (0x822B4A38) does
    // `addi r3, this, 0x5C` (returns &member), and the by-value member pins mCameraInput at
    // +0x90 (0x5C + 48, Camera 16-byte aligned); a pointer member would instead put
    // mCameraInput at +0x60, contradicting the 0x822B4AE0 getter's `addi r3, this, 0x90`.
    // It is not in the canonical type-map (its DWARF home was not dumped), so a
    // minimal-complete 48-byte slice is defined here in this namespace, mirroring the X360
    // CgsSystem::TimerStatusInterface payload (two 24-byte TimerStatus blocks, game then sim).
    // Accessed only by-name across homes, so the exact field spelling is not load-bearing;
    // the 48-byte size IS (it fixes mCameraInput's offset).
    struct alignas(4) TimerStatusInterface                // :254 (by-value payload, 48B)
    {
        unsigned char maReserved[48];   // 2 x 24B TimerStatus (game/sim); size X360-attested
    };

    // AudioCarLoadedDataQueue (homed in BrnRaceCarEntityModuleOutputInterface.h via the
    // AudioCarDataLoadedEvent element, which is in this namespace): EventQueue<...,16>.
    typedef CgsModule::EventQueue<AudioCarDataLoadedEvent, 16> AudioCarLoadedDataQueue; // OutputInterface.h

    // ---- BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_* --------------------------
    // TrafficToRaceCarInterface_PreScene now has a canonical byte-exact (544B) definition in
    // GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h
    // (#included above), so the earlier inline 256-byte slice is deleted here to avoid an ODR
    // duplicate of that FQN and to make SetTrafficToRaceCarInterface_PreScene's memcpy copy the
    // full 0x220 (544) bytes at member offset +0xC0. TrafficToRaceCarInterface_PostScene has NO
    // canonical home yet (the traffic ledger only landed the PreScene type), so its DWARF-faithful
    // 1-byte muDUMMY slice remains defined inline below; it is embedded BY VALUE
    // (mTrafficToRaceCarInterface_PostScene :447) and accessed only by-name.
}
}

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout reconstructed
    // by its own TU (DWARF home BrnTrafficToRaceCarInterface.h:175). Size 1 (DWARF-faithful:
    // the real type is a single uint8_t muDUMMY placeholder, BrnTrafficToRaceCarInterface.h:187).
    // Natural alignment: no Vector*/Matrix*/SIMD/EventQueue member.
    struct TrafficToRaceCarInterface_PostScene
    {
        u8 muDUMMY;   // BrnTrafficToRaceCarInterface.h:187
    };
}
}

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    // ============================================================================
    // OutputBuffer_Prepare  (DWARF :118)
    // ============================================================================
    struct OutputBuffer_Prepare : public CgsModule::IOBuffer
    {
        typedef RaceCarEntityModuleIO::ResourceRequestInterface ResourceRequestInterface;  // :70
        void Construct();                                                                  // :123
        const ResourceRequestInterface* GetResourceRequestInterface() const;              // :126 R  (0x8279CDF0)
        ResourceRequestInterface*       GetResourceRequestInterface();                     // :127 W  (0x822B4990)
    private:
        ResourceRequestInterface mResourceRequestInterface;                                // :131
    };

    // ============================================================================
    // InputBuffer_PreScene  (DWARF :146)  (0x8279D060 W GetGameActionQueue, assert :164)
    // ============================================================================
    struct InputBuffer_PreScene : public CgsModule::IOBuffer
    {
        typedef RaceCarEntityModuleIO::GameActionQueue          GameActionQueue;           // :98
        typedef BrnReplays::ReplayIO::StatusInterface           ReplayStatusInterface;     // :106
        typedef RaceCarEntityModuleIO::AudioCarLoadedDataQueue  AudioCarLoadedDataQueue;   // OutputInterface.h
        void Construct();                                                                  // :151
        const TimerStatusInterface*    GetTimerStatusInterface() const;                    // :154
        void                           SetTimerStatusInterface(const TimerStatusInterface*); // :155
        const BrnDirector::Camera::Camera* GetCameraInput() const;                         // :157
        void                           SetCameraInput(const BrnDirector::Camera::Camera*); // :158
        const BrnWorld::PlayerVehicleControls* GetPlayerVehicleControls() const;           // :160
        void                           SetPlayerVehicleControls(const BrnWorld::PlayerVehicleControls*); // :161
        const GameActionQueue*         GetGameActionQueue() const;                         // :163
        GameActionQueue*               GetGameActionQueue();                               // :164 W  (0x8279D060)
        BrnNetwork::EPaybackType       GetActivePaybackType() const;                       // :166
        void                           SetActivePaybackType(BrnNetwork::EPaybackType);     // :167
        EActiveRaceCarIndex            GetActivePaybackAggressor() const;                  // :169
        void                           SetActivePaybackAggressor(EActiveRaceCarIndex);     // :170
        const ReplayStatusInterface*   GetReplayStatusInterface() const;                   // :173
        void                           SetReplayStatusInterface(const ReplayStatusInterface*); // :174
        AudioCarLoadedDataQueue*       GetAudioCarLoadedDataQueue();                        // :176
        const AudioCarLoadedDataQueue* GetAudioCarLoadedDataQueue() const;                 // :177
        bool                           GetReceivedNetworkDriverControls(EActiveRaceCarIndex) const; // :181
        void                           SetReceivedNetworkDriverControls(EActiveRaceCarIndex); // :185
        void                           SetRaceCarColourIndex(EActiveRaceCarIndex, u16);    // :190
        u16                            GetRaceCarColourIndex(EActiveRaceCarIndex) const;   // :194
        bool                           IsRaceCarColourIndexValid(EActiveRaceCarIndex) const; // :198
        void                           SetRaceCarPaintFinishIndex(EActiveRaceCarIndex, u16); // :203
        u16                            GetRaceCarPaintFinishIndex(EActiveRaceCarIndex) const; // :207
        bool                           IsRaceCarPaintFinishIndexValid(EActiveRaceCarIndex) const; // :211
        void                           SetLostContact(EActiveRaceCarIndex);                // :215
        bool                           GetLostContact(EActiveRaceCarIndex) const;          // :219
        void                           SetRegainedContact(EActiveRaceCarIndex);            // :223
        bool                           GetRegainedContact(EActiveRaceCarIndex) const;      // :227
        void                           SetCarSelectStatus(EActiveRaceCarIndex, bool);      // :232
        bool                           GetCarSelectStatus(EActiveRaceCarIndex) const;      // :236
        bool                           IsCarSelectStatusValid(EActiveRaceCarIndex) const;  // :240
    private:
        u16                     mau16RaceCarColourIndex[8];                                // :244
        u16                     mau16RaceCarPaintFinishIndex[8];                           // :245
        bool                    mabReceivedNetworkDriverControls[8];                       // :246
        bool                    mabRaceCarColourIndexValid[8];                             // :247
        bool                    mabRaceCarPaintFinishIndexValid[8];                        // :248
        bool                    mabLostContactThisFrame[8];                                // :249
        bool                    mabRegainedContactThisFrame[8];                            // :250
        bool                    mabCarSelectStatus[8];                                     // :251
        bool                    mabCarSelectStatusValid[8];                                // :252
        TimerStatusInterface    mTimerStatusInterface;                                     // :254 (BY VALUE; DWARF :53, &member @+0x5C)
        BrnDirector::Camera::Camera mCameraInput;                                          // :255 (@+0x90)
        BrnWorld::PlayerVehicleControls mPlayerVehicleControls;                            // :256
        GameActionQueue         mGameActionQueue;                                          // :257  (0x8279D060 -> this)
        BrnNetwork::EPaybackType meActivePaybackType;                                      // :258
        EActiveRaceCarIndex     meActivePaybackAggressor;                                  // :259
        ReplayStatusInterface   mReplayStatusInterface;                                    // :260 (BY VALUE; DWARF :79, operator=-assigned @+0x3644)
        AudioCarLoadedDataQueue mAudioCarLoadedDataQueue;                                  // :261
    };

    // ============================================================================
    // OutputBuffer_PreScene  (DWARF :274)
    // ============================================================================
    struct OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
        typedef BrnPhysics::Vehicle::VehicleInputInterface          VehicleInputInterface; // :72
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface; // :71
        typedef BrnAI::AIModuleIO::RaceCarAIInterface               RaceCarAIInterface;     // :81
        void Construct();                                                                   // :279
        const VehicleInputInterface* GetVehicleInputInterface() const;                      // :282
        VehicleInputInterface*       GetVehicleInputInterface();                            // :283 W  (0x822B4ED0)
        const SceneInputInterface*   GetSceneInputInterface() const;                        // :285
        SceneInputInterface*         GetSceneInputInterface();                              // :286 W  (0x822B4F78)
        const RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const;        // :288
        RCEntityActiveRaceCarOutputInterface*       GetActiveRaceCarOutputInterface();              // :289
        const RCEntityGlobalRaceCarOutputInterface* GetGlobalRaceCarOutputInterface() const;        // :291
        RCEntityGlobalRaceCarOutputInterface*       GetGlobalRaceCarOutputInterface();              // :292
        const RCEntityActiveRaceCarOutputInterface* GetReplayActiveRaceCarOutputInterface() const;  // :294
        RCEntityActiveRaceCarOutputInterface*       GetReplayActiveRaceCarOutputInterface();        // :295
        const RCEntityGlobalRaceCarOutputInterface* GetReplayGlobalRaceCarOutputInterface() const;  // :297
        RCEntityGlobalRaceCarOutputInterface*       GetReplayGlobalRaceCarOutputInterface();        // :298
        const RaceCarAIInterface*    GetRaceCarAIInterface() const;                          // :300 R  (0x8279D6F8)
        RaceCarAIInterface*          GetRaceCarAIInterface();                                // :301
        bool                         IsRequestingRivalUpdate() const;                       // :303
        void                         SetRequestingRivalUpdate(bool);                        // :304
        AudioCarLoadedDataQueue*       GetAudioCarLoadedDataQueue();                         // :306
        const AudioCarLoadedDataQueue* GetAudioCarLoadedDataQueue() const;                  // :307
    private:
        VehicleInputInterface                mVehicleInputInterface;                        // :311
        SceneInputInterface                  mSceneInputInterface;                          // :312
        RCEntityActiveRaceCarOutputInterface mActiveRaceCarOutputInterface;                 // :313
        RCEntityGlobalRaceCarOutputInterface mGlobalRaceCarOutputInterface;                 // :314
        RCEntityActiveRaceCarOutputInterface mReplayActiveRaceCarOutputInterface;           // :315
        RCEntityGlobalRaceCarOutputInterface mReplayGlobalRaceCarOutputInterface;           // :316
        RaceCarAIInterface                   mRaceCarAIInterface;                           // :317
        bool                                 mbRequestingRivalUpdate;                       // :318
        AudioCarLoadedDataQueue              mAudioCarLoadedDataQueue;                      // :320
    };

    // ============================================================================
    // InputBuffer_PostScene  (DWARF :335)  (0x822B5410 R GetTrafficToRaceCarInterface_PreScene)
    // ============================================================================
    struct InputBuffer_PostScene : public CgsModule::IOBuffer
    {
        typedef BrnWorld::CrashIO::RaceCarOutputInterface                CrashInterface;                   // :92
        typedef BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene TrafficToRaceCarInterface_PreScene; // :94
        void Construct();                                                                  // :340
        const CrashInterface* GetCrashInterface() const;                                   // :343
        void                  SetCrashInterface(const CrashInterface*);                    // :344
        const TrafficToRaceCarInterface_PreScene* GetTrafficToRaceCarInterface_PreScene() const; // :346 R (0x822B5410)
        void                  SetTrafficToRaceCarInterface_PreScene(const TrafficToRaceCarInterface_PreScene*); // :347
    private:
        CrashInterface                     mCrashInterface;                               // :351
        TrafficToRaceCarInterface_PreScene mTrafficToRaceCarInterface_PreScene;           // :352
    };

    // ============================================================================
    // OutputBuffer_PostScene  (DWARF :365)
    // ============================================================================
    struct OutputBuffer_PostScene : public CgsModule::IOBuffer
    {
        typedef CgsSceneManager::SceneManagerIO::SceneCoarseQueryQueue  SceneCoarseQueryQueue;  // :77
        typedef CgsSceneManager::SceneManagerIO::SceneFineLineTestQueue SceneFineLineTestQueue; // :78
        typedef BrnAI::AIModuleIO::AIModuleRequestInterface             AIModuleRequestInterface; // :83
        void Construct();                                                                  // :370
        const SceneCoarseQueryQueue*  GetSceneCoarseQueryQueue() const;                    // :373
        SceneCoarseQueryQueue*        GetSceneCoarseQueryQueue();                          // :374
        const SceneFineLineTestQueue* GetSceneFineLineTestQueue() const;                   // :376
        SceneFineLineTestQueue*       GetSceneFineLineTestQueue();                         // :377
        const AIModuleRequestInterface* GetAIModuleRequestInterface() const;               // :379
        AIModuleRequestInterface*       GetAIModuleRequestInterface();                     // :380 W (0x822B5608)
        const RaceCarToTrafficInterface* GetRaceCarToTrafficInterface() const;             // :382
        RaceCarToTrafficInterface*       GetRaceCarToTrafficInterface();                   // :383 W (0x822B56B0)
    private:
        SceneCoarseQueryQueue     mSceneCoarseQueryQueue;                                  // :387
        SceneFineLineTestQueue    mSceneFineLineTestQueue;                                 // :388
        AIModuleRequestInterface  mAIModuleRequestInterface;                              // :389
        RaceCarToTrafficInterface mRaceCarToTrafficInterface;                             // :390
    };

    // ============================================================================
    // InputBuffer_PrePhysics  (DWARF :404)
    // ============================================================================
    struct InputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
        typedef RaceCarEntityModuleIO::PotentialContactQueue          PotentialContactQueue;       // :89
        typedef RaceCarEntityModuleIO::SceneResultQueue               SceneResultQueue;            // :79
        typedef BrnAI::AIModuleIO::AIModuleResultInterface            AIModuleResultInterface;     // :84
        typedef RaceCarEntityModuleIO::TakedownEventQueue             TakedownEventQueue;          // :97
        typedef BrnGameState::GameStateModuleIO::ScoringOutputInterface       ScoringInterface;       // :100
        typedef BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface OnlineScoringInterface; // :101
        typedef BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PostScene TrafficToRaceCarInterface_PostScene; // :95
        void Construct();                                                                  // :409
        const PotentialContactQueue* GetPotentialContactQueue() const;                     // :412
        void                         SetPotentialContactQueue(const PotentialContactQueue*); // :413
        const SceneResultQueue* GetSceneResultQueue() const;                              // :415
        SceneResultQueue*       GetSceneResultQueue();                                    // :416
        const AIModuleResultInterface* GetAIModuleResultInterface() const;                // :418
        void                           SetAIModuleResultInterface(const AIModuleResultInterface*); // :419
        const TakedownEventQueue* GetTakedownEventQueue() const;                          // :421
        void                      SetTakedownEventQueue(const TakedownEventQueue*);        // :422
        const ScoringInterface* GetScoringInterface() const;                              // :424
        void                    SetScoringInterface(const ScoringInterface*);             // :425
        const OnlineScoringInterface* GetOnlineScoringInterface() const;                  // :427 R (0x822B5800)
        void                          SetOnlineScoringInterface(const OnlineScoringInterface*); // :428
        const TrafficToRaceCarInterface_PostScene* GetTrafficToRaceCarInterface_PostScene() const; // :430 R (0x822B5950)
        void                                       SetTrafficToRaceCarInterface_PostScene(const TrafficToRaceCarInterface_PostScene*); // :431
        bool GetControllerActive() const;                                                 // :433
        void SetControllerActive(bool);                                                   // :434
        bool GetInHardStopCamera() const;                                                 // :436
        void SetInHardStopCamera(bool);                                                   // :437
    private:
        PotentialContactQueue               mPotentialContactQueue;                       // :441
        SceneResultQueue                    mSceneResultQueue;                            // :442
        AIModuleResultInterface             mAIModuleResultInterface;                      // :443
        TakedownEventQueue                  mTakedownEventQueue;                          // :444
        ScoringInterface                    mScoringInterface;                            // :445
        OnlineScoringInterface              mOnlineScoringInterface;                       // :446  (0x822B5800)
        TrafficToRaceCarInterface_PostScene mTrafficToRaceCarInterface_PostScene;         // :447  (0x822B5950)
        bool                                mbControllerActive;                           // :448
        bool                                mbInHardStopCamera;                           // :449
    };

    // ============================================================================
    // OutputBuffer_PrePhysics  (DWARF :462)
    // ============================================================================
    struct OutputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
        typedef BrnPhysics::Vehicle::VehicleDriverInputInterface  VehicleDriverInputInterface;  // :73
        typedef BrnPhysics::Vehicle::VehicleEffectsInputInterface VehicleEffectsInputInterface; // :76
        typedef RaceCarEntityModuleIO::GameEventQueue             GameEventQueue;               // :99
        void Construct();                                                                  // :467
        const OutputBuffer_PreScene::VehicleInputInterface* GetVehicleInputInterface() const; // :470
        OutputBuffer_PreScene::VehicleInputInterface*       GetVehicleInputInterface();       // :471 W (0x822B5C00)
        const VehicleDriverInputInterface*  GetVehicleDriverInterface() const;             // :473
        VehicleDriverInputInterface*        GetVehicleDriverInterface();                   // :474
        const VehicleEffectsInputInterface* GetVehicleEffectsInterface() const;            // :476
        VehicleEffectsInputInterface*       GetVehicleEffectsInterface();                  // :477
        const RCEntityPlayerResetInterface* GetPlayerResetInterface() const;               // :479
        RCEntityPlayerResetInterface*       GetPlayerResetInterface();                     // :480
        const GameEventQueue* GetGameEventQueue() const;                                   // :482 R (0x8279E070)
        GameEventQueue*       GetGameEventQueue();                                         // :483 W (0x822B5CA8)
    private:
        OutputBuffer_PreScene::VehicleInputInterface mVehicleInputInterface;               // :487
        VehicleDriverInputInterface  mVehicleDriverInterface;                              // :488
        VehicleEffectsInputInterface mVehicleEffectsInterface;                             // :489
        RCEntityPlayerResetInterface mPlayerResetInterface;                                // :490
        GameEventQueue               mGameEventQueue;                                      // :491
    };

    // ============================================================================
    // InputBuffer_PostPhysics  (DWARF :505)
    //   CORRECTION (1): 0x8279E310 (W,+29856) = the non-const GetSceneInputInterface()
    //                   (returns &mSceneInputInterface), NOT a non-const GetContactSpyInterface.
    //   CORRECTION (2): the DWARF declares ONLY the const GetContactSpyInterface() (:528) +
    //                   SetContactSpyInterface(:529) -- NO non-const getter overload exists.
    // ============================================================================
    struct InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        typedef BrnPhysics::Vehicle::VehicleOutputInterface        VehicleOutputInterface;        // :74
        typedef BrnPhysics::Vehicle::VehicleManagerOutputInterface VehicleManagerOutputInterface; // :75
        typedef BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules DeformationOutputInterfaceForEntityModules; // :87
        typedef BrnPhysics::Deformation::DeformationOutputInterface DeformationOutputInterface;   // :88
        typedef BrnPhysics::ContactSpy::ContactSpyInterface        ContactSpyInterface;           // :90
        typedef BrnAI::AIModuleIO::AIRaceCarInterface              AIRaceCarInterface;            // :82
        void Construct();                                                                  // :510
        const VehicleOutputInterface* GetVehicleOutputInterface() const;                   // :513
        void                          SetVehicleOutputInterface(const VehicleOutputInterface*); // :514
        const VehicleManagerOutputInterface* GetVehicleManagerOutputInterface() const;     // :516
        void                                 SetVehicleManagerOutputInterface(const VehicleManagerOutputInterface*); // :517
        const OutputBuffer_PreScene::SceneInputInterface* GetSceneInputInterface() const;  // :519
        OutputBuffer_PreScene::SceneInputInterface*       GetSceneInputInterface();        // :520 W (0x8279E310) [CORRECTION (1)]
        const DeformationOutputInterfaceForEntityModules* GetDeformationOutputInterfaceForEntityModules() const; // :522 R (0x822B5F48)
        void SetDeformationOutputInterfaceForEntityModules(const DeformationOutputInterfaceForEntityModules*); // :523
        const DeformationOutputInterface* GetDeformationOutputInterface() const;           // :525 R (0x822B5FF0)
        void                              SetDeformationOutputInterface(const DeformationOutputInterface*); // :526
        const ContactSpyInterface* GetContactSpyInterface() const;                         // :528 R (0x822B6140)
        void                       SetContactSpyInterface(const ContactSpyInterface*);     // :529 [CORRECTION (2): no non-const getter]
        const AIRaceCarInterface*  GetAIRaceCarInterface() const;                          // :531 R (0x822B6290)
        void                       SetAIRaceCarInterface(const AIRaceCarInterface*);       // :532
    private:
        VehicleOutputInterface                     mVehicleOutputInterface;               // :536
        VehicleManagerOutputInterface              mVehicleManagerOutputInterface;        // :537
        OutputBuffer_PreScene::SceneInputInterface mSceneInputInterface;                  // :538  (0x8279E310 -> +29856)
        DeformationOutputInterfaceForEntityModules mDeformationOutputInterfaceForEntityModules; // :539
        DeformationOutputInterface                 mDeformationOutputInterface;           // :540
        ContactSpyInterface                        mContactSpyInterface;                  // :541
        AIRaceCarInterface                         mAIRaceCarInterface;                   // :542
    };

    // ============================================================================
    // OutputBuffer_PostPhysics  (DWARF :555)
    // ============================================================================
    struct OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        typedef BrnDirector::BrnDirectorVehicleInputInterface DirectorVehicleInputInterface; // :103
        typedef RaceCarEntityModuleIO::ResourceRequestInterface ReplayRequestInterface;       // :107
        void Construct();                                                                  // :560
        const ResourceRequestInterface* GetResourceRequestInterface() const;              // :563
        ResourceRequestInterface*       GetResourceRequestInterface();                     // :564
        const OutputBuffer_PreScene::SceneInputInterface* GetSceneInputInterface() const;  // :566
        OutputBuffer_PreScene::SceneInputInterface*       GetSceneInputInterface();        // :567 W (0x822B5D50)
        const DirectorVehicleInputInterface* GetDirectorVehicleInputInterface() const;     // :569
        DirectorVehicleInputInterface*       GetDirectorVehicleInputInterface();           // :570
        const RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const;        // :572
        RCEntityActiveRaceCarOutputInterface*       GetActiveRaceCarOutputInterface();              // :573
        const RCEntityGlobalRaceCarOutputInterface* GetGlobalRaceCarOutputInterface() const;        // :575
        RCEntityGlobalRaceCarOutputInterface*       GetGlobalRaceCarOutputInterface();              // :576
        const RCEntityActiveRaceCarOutputInterface* GetReplayActiveRaceCarOutputInterface() const;  // :578 R (0x8279E678)
        RCEntityActiveRaceCarOutputInterface*       GetReplayActiveRaceCarOutputInterface();        // :579 W (0x822B6488)
        const RCEntityGlobalRaceCarOutputInterface* GetReplayGlobalRaceCarOutputInterface() const;  // :581
        RCEntityGlobalRaceCarOutputInterface*       GetReplayGlobalRaceCarOutputInterface();        // :582 W (0x822B6920)
        const GameEventQueue* GetGameEventQueue() const;                                   // :584
        GameEventQueue*       GetGameEventQueue();                                         // :585
        const ReplayRequestInterface* GetReplayRequestInterface() const;                   // :587
        ReplayRequestInterface*       GetReplayRequestInterface();                         // :588
        const OutputBuffer_PreScene::VehicleInputInterface* GetVehicleInputInterface() const; // :590 R (0x8279E9C0)
        OutputBuffer_PreScene::VehicleInputInterface*       GetVehicleInputInterface();       // :591 W (0x822B6878)
    private:
        ResourceRequestInterface             mResourceRequestInterface;                    // :595
        OutputBuffer_PreScene::SceneInputInterface mSceneInputInterface;                   // :596  (0x822B5D50 -> +147488)
        DirectorVehicleInputInterface        mDirectorVehicleInputInterface;               // :597
        RCEntityActiveRaceCarOutputInterface mActiveRaceCarOutputInterface;               // :598
        RCEntityGlobalRaceCarOutputInterface mGlobalRaceCarOutputInterface;               // :599
        RCEntityActiveRaceCarOutputInterface mReplayActiveRaceCarOutputInterface;         // :600  (0x822B6488/0x8279E678 -> +826992)
        RCEntityGlobalRaceCarOutputInterface mReplayGlobalRaceCarOutputInterface;         // :601  (0x822B6920 -> +855200)
        GameEventQueue                       mGameEventQueue;                              // :602
        ReplayRequestInterface               mReplayRequestInterface;                      // :603
        OutputBuffer_PreScene::VehicleInputInterface mVehicleInputInterface;               // :604  (0x822B6878/0x8279E9C0 -> +855152)
    };

    // ============================================================================
    // InputBuffer_GenerateDispatchLists  (DWARF :619)
    //   The two byte-reads at +8289/+8290 are trailing bool members past the captured
    //   DWARF member list (:652); names NOT in DWARF -> placeholder mb*/GetDispatchFlag*.
    // ============================================================================
    struct InputBuffer_GenerateDispatchLists : public CgsModule::IOBuffer
    {
        void Construct();                                                                  // :624
        const BrnDirector::Camera::Camera* GetCameraInput() const;                         // :627 R (0x822B69C8)
        void          SetCameraInput(const BrnDirector::Camera::Camera*);                  // :628
        const SceneResultQueue* GetSceneResultQueue() const;                              // :630
        SceneResultQueue*       GetSceneResultQueue();                                    // :631
        CgsGraphics::DispatchFrame* GetDispatchFrame() const;                             // :633
        void                        SetDispatchFrame(CgsGraphics::DispatchFrame*);        // :634
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* GetBlobbyShadowBuffer() const;     // :636
        void SetBlobbyShadowBuffer(BrnBlobbyShadowManager::BrnBlobbyShadowBuffer*);       // :637
        BrnCoronaManager::BrnSubmissionInterface* GetCoronaSubmissionInterface() const;   // :639
        void SetCoronaSubmissionInterface(BrnCoronaManager::BrnSubmissionInterface*);     // :640
        BrnWorld::ShadowMap* GetShadowMap() const;                                        // :642
        void                 SetShadowMap(BrnWorld::ShadowMap*);                          // :643
        // Trailing bool flags (names NOT in DWARF; placeholders): read by 0x822B6B20/0x822B6BD0.
        bool GetDispatchFlagA() const; // 0x822B6B20 -> *(this+8289)
        bool GetDispatchFlagB() const; // 0x822B6BD0 -> *(this+8290)
    private:
        BrnDirector::Camera::Camera  mCameraInput;                                        // :647
        SceneResultQueue             mSceneResultQueue;                                   // :648
        CgsGraphics::DispatchFrame*  mpDispatchFrame;                                     // :649
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* mpBlobbyShadowBuffer;              // :650
        BrnCoronaManager::BrnSubmissionInterface*      mpCoronaSubmissionInterface;       // :651
        BrnWorld::ShadowMap*         mpShadowMap;                                         // :652
        bool                         mbDispatchFlagA;                                     // +8289 (placeholder name)
        bool                         mbDispatchFlagB;                                     // +8290 (placeholder name)
    };

    // ============================================================================
    // Element accessors homed in BrnRaceCarEntityModuleOutputInterface.h (NOT here):
    //   0x8227D690 RCEntityActiveRaceCarOutputInterface::GetRaceCarStateMutable(EActiveRaceCarIndex)
    //              (asserts + IsRaceCarActive gate; &maRaceCarStates[idx]).
    //   0x823101C0 RCEntityGlobalRaceCarOutputInterface::GetActiveCarDataElementAddress(EActiveRaceCarIndex) const
    //              (const void* into the parallel-array window; 36*idx + base + 528).
    //
    // Traffic-system event-queue template instantiations (own TUs):
    //   0x827A7AB8 CgsModule::BaseEventQueue<CreateRivalInTrafficSystemEvent>::Append(const&)  (stride 48)
    //   0x827A7BA8 CgsModule::BaseEventQueue<RemoveRivalFromTrafficSystemEvent>::Append(const&) (stride 1)
    //   0x822E3520 CgsModule::EventQueue<RemoveRivalFromTrafficSystemEvent,34>::Construct()     (capacity 34)
    // ============================================================================
}
}
