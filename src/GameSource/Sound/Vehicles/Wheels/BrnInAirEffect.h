#ifndef BRN_SOUND_VEHICLES_WHEELS_IN_AIR_EFFECT_H
#define BRN_SOUND_VEHICLES_WHEELS_IN_AIR_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoicePool.h"
#include "GameSource/AttribSys/Generated/classes/crashbin.h"
#include "GameSource/Sound/Global/BrnMusicEffect.h"

namespace BrnPhysics { namespace Vehicle { struct RaceCarState; } }
namespace BrnTraffic { namespace BrnTrafficIO { struct TrafficSoundEntity; } }

namespace BrnSound
{
namespace Logic { namespace Collision { class CollisionStateManager; enum ECollisionSpliceTags ; } }
namespace Vehicles
{
namespace Engines { struct PhysicsControl; }
namespace Environment { struct EnclosureControl; }
namespace Wheels
{
extern bool KB_DEBUG_SPEW_RAMP_LAUNCH;
extern f32 gfFORCE_RAMP_LUNCH;
extern CgsSound::Utils::SlopeParams K_RAMP_LAUNCH_PARAMS;

struct WheelControl;

// Player/traffic/junkyard airborne, suspension-compression and landing audio.
// The declaration order is the DecFIGS BrnInAirEffect.h shape; ARTIST fixes the
// control ids, state transitions, sample-tag routing and voice parameters.
struct InAirEffect : public BrnSound::Logic::BrnEffectObject
{
    struct SuspensionStatus
    {
        enum eSuspensionLatchedState
        {
            E_NONE = 0,
            E_COMPRESSED = 1,
            E_UNCOMPRESSED = 2,
        };

        SuspensionStatus() { Clear(); }
        void Clear();

        f32 mfSuspensionHeight;
        eSuspensionLatchedState meSuspensionLatchedState;
        CgsSound::Utils::Average<3u, f32> mSuspensionDelta;
    };

    struct InAirPhysicsData
    {
        InAirPhysicsData();

        CgsSound::Utils::DataPoint<bool> mIsOnGround;
        CgsSound::Utils::DataPoint<bool> maWheelOnGround[4];
        f32 mafSuspensionHeights[4];
        CgsSound::Utils::DataPoint<f32> mfTimeInAir;
        f32 mfTimeSinceReset;
        bool mbIsCrashing;
    };

    InAirEffect();
    virtual ~InAirEffect();

    const char* GetTypeName() const ;
    s32 GetController(s32 aiSlot) ;
    void AttachController(CgsSound::Logic::EffectBase* apController) ;
    void SetupLoadData() ;
    bool Attach() ;
    void UpdateParams(f32 afTimeStep) ;
    void ProcessUpdate() ;
    bool Detach() ;

protected:
    virtual void UpdatePhysicsData(f32 afTimeStep);
    void UpdateWheelLandings(f32 afTimeStep);
    void UpdateSuspensionSqueeks(f32 afTimeStep);
    void PlayLanding(BrnSound::Logic::Collision::ECollisionSpliceTags aeTag,
                     f32 afVolumeModifier);
    void PlayJumpCamLanding();
    BrnSound::Logic::BrnEffectObject::SampleTag GetSampleLandingId(
        BrnSound::Logic::Collision::ECollisionSpliceTags aeTag);

    WheelControl* mpWheelControl;
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl;
    BrnSound::Vehicles::Environment::EnclosureControl* mpEnclosureControl;
    CgsSound::Logic::VoiceWrapper mInAirVoice;
    CgsSound::Logic::VoicePool<4> mLandingVoices;
    CgsSound::Logic::VoiceWrapper mHardLandingVoice;
    CgsSound::Logic::VoiceWrapper mJunkyardLandingSweetnerVoice;
    CgsSound::Logic::VoiceWrapper mJumpCamLandingVoice;
    CgsSound::Logic::VoiceWrapper::FunctorPointer<InAirEffect> mLaunchFunctionPointer;
    f32 mfHardLandingVoiceSecondGain;
    f32 mfJumpCamLandingVoiceSecondGain;
    f32 mfJunkyardLandingSweetnerVoiceSecondGain;
    f32 mfSuspensionSensitivity;
    f32 mfSuspensionThreshold;
    Attrib::Gen::crashbin mBin;
    u32 muRoundRobin;
    SuspensionStatus mSuspensionStatus[4];
    BrnSound::Logic::Collision::CollisionStateManager* mpCollisionMgr;
    f32 mfTimeSinceJumpCamera;
    InAirPhysicsData mPhysicsData;
};

struct TrafficInAir : public InAirEffect
{
    TrafficInAir();
    virtual ~TrafficInAir();

    static CgsSound::Logic::EffectObject* CreateObject(u32 luType);
    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const ;
    const char* GetTypeName() const ;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();

    bool Attach() ;
    void ProcessUpdate() ;

protected:
    void UpdatePhysicsData(f32 afTimeStep) ;
    void Clear();

    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* mpTrafficEntity;
};

struct JunkyardInAirEffect : public InAirEffect
{
    JunkyardInAirEffect();
    virtual ~JunkyardInAirEffect();

    static CgsSound::Logic::EffectObject* CreateObject(u32 luType);
    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const ;
    const char* GetTypeName() const ;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();

    bool Attach() ;
    void Notify(const CgsSound::Io::MessageHeader* apkMessage) ;

protected:
    void UpdatePhysicsData(f32 afTimeStep) ;
    void FlushData(const BrnPhysics::Vehicle::RaceCarState* apkVehicleData);

    CgsSound::Utils::DataPoint<bool> mbIsVehicleValid;
    BrnSound::Logic::MusicEffect::EJunkyardAmbience meJunkyardAmbience;
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif
