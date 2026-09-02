#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Effects/SharedIO/BrnEffectsModuleIO_InputBuffer.h
//
// BrnEffects::EffectsIO::InputBuffer -- the per-frame input aggregate the game
// bridges (BrnGame::BrnGameModule::DoUpdate_Effects @0x823DD0A8 / BridgeEntityToEffects
// @0x823CDF00) fill before the effects module updates (EffectsModule::Update
// @0x8229EC28). Every member is embedded BY VALUE; each guarded accessor takes the
// buffer lock, then returns &member (getters) or copies into the member (setters);
// the two fixed-size queue setters clear the destination queue first, then Append.
//
// 2026-09-02 (tyre-mark wave): RETYPED BY NAME. This used to be an opaque
// `u8 maImage[0xE4A0]` addressed at console byte offsets, with `const void*`
// accessors and a queue-view `Append` that only ADDED THE COUNTS (it copied no
// event -- the silent-drop shape). Every member type is now a real committed type
// and every copy is the type's own copy. The DWARF member set / order / names
// (DecFIGS EffectsModuleIO.h:19-72) are the authority; the X360 offsets below are
// the pins from InputBuffer::Construct @0x82293618 and the accessor bodies, and
// they tile the console's 58,528-byte allocation exactly:
//
//   +0x00000  IOBuffer status byte
//   +0x00004  BoostOutputInfo[8]                       maBoostInfos               (:21)  36 x 8 = 288
//   +0x00124  VariableEventQueue<4096,16>              mInEventQueue              (:28)  Construct @+292
//   +0x01140  RCEntityActiveRaceCarOutputInterface     mActiveRaceCarInterface    (:31)  Clear @+4416, Set XMemCpy 0x28F0
//   +0x03A30  EventQueue<PhysicalTrafficState,20>      mVehiclePhysicalStateQueue (:34)  Construct @+14896
//   +0x07A00  BrnDirector::Camera::Camera              mCameraInput               (:37)  Clear @+31232
//   +0x07B60  CgsSystem::TimerStatusInterface          mTimerStatusInterface      (:40)  Clear @+31584 (48 B)
//   +0x07B90  ContactSpyInterface                      mContactSpyInterface       (:43)  Construct @+31632
//   +0x07BA0  DeformationOutputInterface               mDeformationInterface      (:46)  Construct @+31648
//   +0x0A690  VariableEventQueue<13312,16>             mGameActionQueue           (:49)  Construct @+42640
//   +0x0DAA0  EffectsEnvironmentInterface              mEffectsEnvironmentInterface (:52) 16 B
//   +0x0DAB0  ReplayIO::StatusInterface                mReplayStatusInterface     (:59)  the inlined Clear @+55984..+57540
//   +0x0E0D0  EventQueue<PropVFXLocatorEvent,10>       mPropVFXLocatorQueue       (:62)  Construct @+57552
//   +0x0E400  TriangleCacheInterface                   mTriangleCacheInterface    (:69)  one word (the manager pointer)
//   +0x0E404  AudioEffectsMessageQueue                 mAudioEffectsMessageQueue  (:72)  144 B (VEQ<128,16>)
//   +0x0E494  bool                                     mbSuspendEffects           X360-only tail (see below)
//
// The console layout is what pins the MEMBER SET; nothing on the host addresses this
// object by byte offset. Host widths differ (pointers, the 16-aligned camera) and are
// irrelevant to a by-name aggregate.
//
// mbSuspendEffects (FLAG: name inferred). Not in the FIGS DWARF; the X360 buffer is 8 bytes
// longer and DoUpdate_Effects stores its `a32` bool at +0xE494 (`*(v43 + 58516) = a32`),
// which EffectsModule::Update reads to drive its suspend / resume ladder (set -> suspend
// the playing effects after 5 frames; cleared -> resume). The value comes from the game
// module's DoUpdate (the console arg is the "return to front end / loading" gate).
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                                     // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                           // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                   // CgsModule::EventQueue
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"                   // CgsSystem::TimerStatusInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"           // CgsSceneManager::SceneManagerIO::TriangleCacheInterface
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface / BoostOutputInfo
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"          // VehicleOutputInterface::PhysicalTrafficStateQueue
#include "GameSource/Director/Camera/Camera.h"                                             // BrnDirector::Camera::Camera
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"                        // BrnPhysics::ContactSpy::ContactSpyInterface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"  // BrnPhysics::Deformation::DeformationOutputInterface
#include "GameSource/Effects/SharedIO/BrnEffectsEnvironmentInterface.h"                    // BrnEffects::EffectsEnvironmentInterface
#include "GameSource/Replays/BrnReplayStatusInterface.h"                                   // BrnReplays::ReplayIO::StatusInterface
#include "GameSource/World/BrnWorldModuleIO.h"                                             // BrnWorldIO::...::PropVFXLocatorQueue (EventQueue<PropVFXLocatorEvent,10>)
#include "GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h"                         // BrnSound::Module::Io::AudioEffectsMessageQueue

namespace BrnEffects
{
namespace EffectsIO
{
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // ---- the DWARF typedefs (EffectsModuleIO.h:24 / :49 / :56 / :66) --------------
        typedef CgsModule::VariableEventQueue<4096, 16>                          InAttribSysUserEventQueue;
        typedef CgsModule::VariableEventQueue<13312, 16>                         GameActionQueue;
        typedef BrnReplays::ReplayIO::StatusInterface                            ReplayStatusInterface;
        typedef CgsSceneManager::SceneManagerIO::TriangleCacheInterface          InTriangleCacheInterface;
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface RCEntityActiveRaceCarOutputInterface;
        typedef BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo                 BoostOutputInfo;
        typedef BrnPhysics::Vehicle::VehicleOutputInterface::PhysicalTrafficStateQueue PhysicalTrafficStateQueue;
        typedef BrnWorldIO::UpdateOutputBuffer::PropVFXLocatorQueue              PropVFXLocatorQueue;
        typedef BrnSound::Module::Io::AudioEffectsMessageQueue                   AudioEffectsMessageQueue;

        static const u32 KU_NUM_BOOST_INFOS = 8;

        // X360 0x82293618 (:76). The buffer-stack Construct: status = 1, then every embedded
        // queue / interface constructed or cleared in the console's order.
        void Construct();
        // (:79) -- ICF-folded into IOBuffer::Destruct on the console (no member teardown).
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

        // ---- getters (read-lock, "Not locked for reading\n") -------------------------------
        const GameActionQueue*                  GetGameActionQueue() const;            // 0x8227DBE0 :145
        const InTriangleCacheInterface*         GetTriangleCacheInterface() const;     // 0x8227DD30 :175
        const BrnDirector::Camera::Camera*      GetCameraInput() const;                // 0x8227D940 :106
        const CgsSystem::TimerStatusInterface*  GetTimerStatusInterface() const;       // 0x8227D9E8 :115
        const BrnPhysics::ContactSpy::ContactSpyInterface* GetContactSpyInterface() const; // 0x8227DA90 :124
        const BrnPhysics::Deformation::DeformationOutputInterface* GetDeformationInterface() const; // 0x8227DB38 :133
        const AudioEffectsMessageQueue*         GetAudioEffectsMessageQueue() const;   // 0x8227DDD8 :184
        const RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarInterface() const; // 0x8227D7F0 :88
        const PhysicalTrafficStateQueue*        GetVehiclePhysicalStateQueue() const;  // 0x8227D898 :97
        const PropVFXLocatorQueue*              GetPropVFXLocatorQueue() const;        // 0x8227DC88 :169
        const ReplayStatusInterface*            GetReplayStatusInterface() const;      // (:148)
        const EffectsEnvironmentInterface*      GetEffectsEnvironmentInterface() const;// (:160)
        // The per-car boost snapshot array (DWARF :21). EffectsModule::Update hands `this+4`
        // to ProcessActiveRaceCars as the BoostOutputInfo* it strides by 36.
        const BoostOutputInfo*                  GetBoostInfos() const { return maBoostInfos; }
        bool                                    GetSuspendEffects() const { return mbSuspendEffects; }

        // ---- write-lock getters ("Not locked for writing\n") -------------------------------
        GameActionQueue*                        GetGameActionQueue();                  // 0x823BA708 :142

        // ---- setters (write-lock) ------------------------------------------------------------
        void SetActiveRaceCarInterface(const RCEntityActiveRaceCarOutputInterface* lpInterface); // 0x823BA490 :82 (XMemCpy)
        void SetVehiclePhysicalStateQueue(const PhysicalTrafficStateQueue* lpQueue);   // 0x823C96B8 :91  (clear + Append)
        void SetCameraInput(const BrnDirector::Camera::Camera* lpCameraInput);         // 0x823C9770 :100 (operator=)
        void SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpTimer);   // 0x823BA548 :109 (48-byte copy)
        void SetContactSpyInterface(const BrnPhysics::ContactSpy::ContactSpyInterface* lpInterface); // 0x823BA658 :118
        void SetDeformationInterface(const BrnPhysics::Deformation::DeformationOutputInterface* lpInterface); // 0x823C9820 :127 (operator=)
        void SetBoostInfoN(s32 liIndex, const BoostOutputInfo* lpBoostInfo);          // (:136)
        void SetReplayStatusInterface(const ReplayStatusInterface* lpStatus);          // 0x823BA7B0 :151 (operator=)
        void SetEffectsEnvironmentInterface(const EffectsEnvironmentInterface* lpEnv); // 0x823BA868 :154 (16-byte copy)
        void SetPropVFXLocatorQueue(const PropVFXLocatorQueue* lpQueue);               // 0x823C98D0 :163 (clear + Append)
        void SetTriangleCacheInterface(const InTriangleCacheInterface* lpInterface);   // 0x823BA928 :172 (one word)
        void SetAudioEffectsMessageQueue(const AudioEffectsMessageQueue* lpQueue);     // 0x823BA9E0 :178 (144-byte copy)
        void SetSuspendEffects(bool lbSuspend) { mbSuspendEffects = lbSuspend; }       // the +0xE494 store in DoUpdate_Effects

        // The console's BridgeEntityToEffects @0x823CDF00 copies the 8 BoostOutputInfo records
        // straight into +0x4 (nine words each); reached by name here.
        BoostOutputInfo* GetBoostInfosForWrite() { return maBoostInfos; }

    private:
        BoostOutputInfo                                        maBoostInfos[KU_NUM_BOOST_INFOS];   // :21   +0x00004
        InAttribSysUserEventQueue                              mInEventQueue;                      // :28   +0x00124
        RCEntityActiveRaceCarOutputInterface                   mActiveRaceCarInterface;            // :31   +0x01140
        PhysicalTrafficStateQueue                              mVehiclePhysicalStateQueue;         // :34   +0x03A30
        BrnDirector::Camera::Camera                            mCameraInput;                       // :37   +0x07A00
        CgsSystem::TimerStatusInterface                        mTimerStatusInterface;              // :40   +0x07B60
        BrnPhysics::ContactSpy::ContactSpyInterface            mContactSpyInterface;               // :43   +0x07B90
        BrnPhysics::Deformation::DeformationOutputInterface    mDeformationInterface;              // :46   +0x07BA0
        GameActionQueue                                        mGameActionQueue;                   // :49   +0x0A690
        EffectsEnvironmentInterface                            mEffectsEnvironmentInterface;       // :52   +0x0DAA0
        ReplayStatusInterface                                  mReplayStatusInterface;             // :59   +0x0DAB0
        PropVFXLocatorQueue                                    mPropVFXLocatorQueue;               // :62   +0x0E0D0
        InTriangleCacheInterface                               mTriangleCacheInterface;            // :69   +0x0E400
        AudioEffectsMessageQueue                               mAudioEffectsMessageQueue;          // :72   +0x0E404
        bool                                                   mbSuspendEffects;                   // X360  +0x0E494 (FLAG: name inferred)
    };
}
}
