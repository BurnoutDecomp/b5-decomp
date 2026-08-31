#ifndef BRN_SOUND_VEHICLES_ENGINES_CLUTCH_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_CLUTCH_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "GameSource/AttribSys/Generated/classes/vehicleengine.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels { struct WheelControl; }
namespace Engines
{

struct PhysicsControl;
struct EngineControl;
struct HybridExhaustControl;

struct ClutchControl : public BrnSound::Logic::BrnEffectControl,
                       public ShiftControl::IShiftingActivator
{
    enum EClutchState
    {
        E_CLUTCH_STATE_NONE = 0,
        E_CLUTCH_STATE_ATTACK_BEGIN = 1,
        E_CLUTCH_STATE_ATTACK_UPDATE = 2,
        E_CLUTCH_STATE_IDLE_BEGIN = 3,
        E_CLUTCH_STATE_IDLE_REVING = 4,
        E_CLUTCH_STATE_IDLE_DISENGAGE = 5,
        E_CLUTCH_STATE_INTERRUPT = 6,
        E_CLUTCH_STATE_INIFINITE_GEAR = 7,
        E_CLUTCH_STATE_BOOST = 8,
    };

    enum EDrivingState
    {
        E_DRIVING_STATE_REGULAR = 0,
        E_DRIVING_STATE_BOOST = 1,
        E_MAX_DRIVING_STATES = 2,
    };

    ClutchControl();
    virtual ~ClutchControl();

    virtual s32 GetController(s32 aiSlot);
    virtual void AttachController(CgsSound::Logic::EffectBase* apController);
    virtual bool Attach();
    virtual void UpdateParams(f32 afTimeStep);
    virtual void ProcessUpdate() {}
    virtual void Notify(const CgsSound::Io::MessageHeader* apkMessage);

    static CgsSound::Logic::EffectControl* CreateObject(u32 luType);

    bool IsActive() const { return meClutchState != E_CLUTCH_STATE_NONE; }
    bool IsInfiniteGears() const { return meClutchState == E_CLUTCH_STATE_INIFINITE_GEAR; }
    bool IsBoostingGears() const { return meClutchState == E_CLUTCH_STATE_BOOST; }
    f32 GetClutchRPM() const { return mInterpRPM.GetValueFloat(); }
    f32 GetClutchThrottle() const { return mInterpThrottle.GetValueFloat(); }
    f32 GetClutchVolume() const { return mInterpVol.GetValueFloat(); }
    f32 GetDamageThrottle() const { return mfDamageThrottleAmount; }
    EClutchState GetClutchState() const { return meClutchState; }

    virtual f32 GetStartRPM();
    virtual f32 GetTargetRPM();
    virtual f32 GetRiseFromRPM();

private:
    bool ShouldBeginClutchAttack() const;
    bool ShouldBeginIdleClutch() const;
    bool ShouldBeginBoostAttack() const;
    bool ShouldBeginInfiniteGearRise() const;
    void UpdateClutchState(f32 afTimeStep);
    void UpdateDamagedEngine(f32 afTimeStep);
    void GenerateDamagedWindow();

    PhysicsControl* mpPhysicsControl;
    EngineControl* mpEngineControl;
    ShiftControl* mpShiftControl;
    HybridExhaustControl* mpHybridExhaustControl;
    BrnSound::Vehicles::Wheels::WheelControl* mpWheelControl;
    Attrib::Gen::vehicleengine mVehicleEngineAttributes;
    EClutchState meClutchState;
    f32 mfLastClutchAttack;
    f32 mfLastIdleClutch;
    CgsSound::Utils::InterpolateLine mInterpThrottle;
    CgsSound::Utils::InterpolateLine mInterpRPM;
    CgsSound::Utils::InterpolateLine mInterpVol;
    f32 mfElapsedTimeOfInfiniteGears;
    EDrivingState meDrivingState;
    CgsNumeric::Random mRandom;
    f32 mfMaxIncrement;
    f32 mfRandomTarget;
    f32 mfRPMBeforeShift;
    f32 mfDamageEngineAmount;
    f32 mfDamageThrottleAmount;
    CgsSound::Utils::SqaureWave mDamagedThrottle;
    CgsSound::Utils::SqaureWave mDamagedWindow;
};

struct AIClutchControl : public ClutchControl
{
    AIClutchControl();
    virtual ~AIClutchControl();
    static CgsSound::Logic::EffectControl* CreateObject(u32 luType);
    virtual void UpdateParams(f32 /*afTimeStep*/) {}
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif
