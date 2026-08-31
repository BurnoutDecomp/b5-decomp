#ifndef BRN_SOUND_VEHICLES_ENGINES_SHIFT_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_SHIFT_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameSource/AttribSys/Generated/classes/shiftpattern.h"

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct PhysicsControl;
struct EngineControl;
struct HybridExhaustControl;

struct ShiftControl : public BrnSound::Logic::BrnEffectControl
{
    enum EShiftStage
    {
        E_SHFT_NONE = 0,
        E_SHFT_UP_DISENGAGE = 1,
        E_SHFT_UP_ENGAGING = 2,
        E_SHFT_UP_LFO = 3,
        E_SHFT_DOWN_DISENGAGE = 4,
        E_SHFT_DOWN_ENGAGING_RISE = 5,
        E_SHFT_DOWN_ENGAGING_FALL = 6,
        E_SHFT_DOWN_ENGAGING_REATTACH = 7,
    };

    enum EPostShiftLFO
    {
        E_SHIFT_LFO_NONE = 0,
        E_SHIFT_LFO_ON = 1,
    };

    struct IShiftingActivator
    {
        virtual ~IShiftingActivator() {}
        virtual f32 GetStartRPM() = 0;
        virtual f32 GetTargetRPM() = 0;
        virtual f32 GetRiseFromRPM() = 0;
    };

    ShiftControl();
    virtual ~ShiftControl();

    virtual s32 GetController(s32 aiSlot);
    virtual void AttachController(CgsSound::Logic::EffectBase* apController);
    virtual bool Attach();
    virtual void SetupLoadData();
    virtual void UpdateParams(f32 afTimeStep);
    virtual void ProcessUpdate() {}

    static CgsSound::Logic::EffectControl* CreateObject(u32 luType);

    bool IsActive() const
    {
        return meShiftState != E_SHFT_NONE;
    }
    bool IsUpshifting() const
    {
        return meShiftState == E_SHFT_UP_DISENGAGE || meShiftState == E_SHFT_UP_ENGAGING;
    }
    bool IsDownShifting() const
    {
        return meShiftState >= E_SHFT_DOWN_DISENGAGE &&
               meShiftState <= E_SHFT_DOWN_ENGAGING_REATTACH;
    }
    f32 GetShiftingRPM() const { return mInterpShiftRPM.GetValueFloat(); }
    f32 GetShiftingThrottle() const { return mInterpShiftThrottle.GetValueFloat(); }
    f32 GetShiftingVolume() const { return mInterpShiftVol.GetValueFloat(); }
    EShiftStage GetShiftingState() const { return meShiftState; }
    EShiftStage GetShiftingStateChange() const { return meShiftStageChanged; }
    f32 GetVolLFO_Amplitude() const { return mfVOL_LFO_AMP; }
    f32 GetVolLFO_Frequency() const { return mfVOL_LFO_FRQ; }
    f32 GetRPM_LFO_Amplitude() const { return mfRPM_LFO_AMP; }
    f32 GetRPM_LFO_Frequncy() const { return mfRPM_LFO_FRQ; }
    f32 GetLastUpShiftTime() const { return mfLastUpShift; }

    void BeginUpShift(IShiftingActivator* lpShiftingActivator);
    void BeginDownShift(IShiftingActivator* lpShiftingActivator);

private:
    void UpdateGearShiftState(f32 afTimeStep);
    void EndShifting();
    void PostShiftFX_Init();
    void PostShiftFX_Update(f32 afTimeStep);
    void PostShiftFX_End();
    void UpdateThrottle(f32 afTimeStep);
    void UpdateRPM(f32 afTimeStep);

    PhysicsControl*       mpPhysicsControl;
    EngineControl*        mpEngineControl;
    HybridExhaustControl* mpHybridControl;
    bool                  mbNeed_ShiftGearSnd;
    bool                  mbNeed_DisengageSnd;
    bool                  mbNeed_EngageSnd;
    Attrib::Gen::shiftpattern mShiftingPatternData;
    EShiftStage           meShiftState;
    EShiftStage           meShiftStageChanged;
    s32                   miRaceCarIndex;
    EPostShiftLFO         meShift_LFO;
    f32                   mfVOL_LFO_AMP;
    f32                   mfVOL_LFO_FRQ;
    f32                   mfRPM_LFO_AMP;
    f32                   mfRPM_LFO_FRQ;
    CgsSound::Utils::InterpolateLine mInterpRPM_LFODecay;
    CgsSound::Utils::InterpolateLine mInterpVol_LFODecay;
    f32                   mfRPMAtShift;
    f32                   mfLastUpShift;
    IShiftingActivator*   mpShiftingActivator;
    CgsSound::Utils::InterpolateLine mInterpShiftThrottle;
    CgsSound::Utils::InterpolateLine mInterpShiftRPM;
    CgsSound::Utils::InterpolateLine mInterpShiftVol;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif
