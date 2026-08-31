#ifndef BRN_SOUND_VEHICLES_ENGINES_SWEETENERS_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_SWEETENERS_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace Attrib { namespace Gen { class vehicleengine; } }

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct PhysicsControl;
struct ShiftControl;

// DecFIGS declaration with behavior recovered from ARTIST 0x826F2DD8..0x826FDA84.
// This is the discrete powertrain layer: shift clunks, throttle-off sputters,
// exhaust pops, starter, and damage bangs.
struct SweetenersEffect : public BrnSound::Logic::BrnEffectObject
{
    enum eSweetenerBankElement
    {
        E_SWEETENER_BANK_ELEMENT_GEAR_UP = 0,
        E_SWEETENER_BANK_ELEMENT_GEAR_DOWN = 1,
        E_SWEETENER_BANK_ELEMENT_THROTTLE_ON = 2,
        E_SWEETENER_BANK_ELEMENT_THROTTLE_OFF = 3,
        E_SWEETENER_BANK_ELEMENT_EXHAUST_POP = 4,
        E_SWEETENER_BANK_ELEMENT_STARTING = 5,
        E_SWEETENER_BANK_ELEMENT_WHINE = 6,
        E_SWEETENER_BANK_ELEMENT_TURBO = 7,
        E_SWEETENER_BANK_ELEMENT_COUNT = 8
    };

    enum eSweetenersState
    {
        E_SWEETENER_STATE_NONE = 0,
        E_SWEETENER_STATE_WAIT_CONTENT = 1,
        E_SWEETENER_STATE_UPDATING = 2
    };

    enum ECarStartingState
    {
        E_CARSTARTINGSTATE_NONE = 0,
        E_CARSTARTINGSTATE_BEGIN_STARTING = 1,
        E_CARSTARTINGSTATE_STARTING = 2,
        E_CARSTARTINGSTATE_QUITTING = 3,
        E_CARSTARTINGSTATE_RESTARTING = 4
    };

    struct SweetenerInfo
    {
        SweetenerInfo() : mi16FirstIndex(-1), mi16LastIndex(-1), mfVolume(0.0f) {}
        s16 mi16FirstIndex;
        s16 mi16LastIndex;
        f32 mfVolume;
    };

    SweetenersEffect();
    virtual ~SweetenersEffect();

    virtual s32 GetController(s32 aiSlot);
    virtual void AttachController(CgsSound::Logic::EffectBase* apController);
    virtual bool Attach();
    virtual void UpdateParams(f32 afTimeStep);
    virtual void ProcessUpdate();
    virtual bool Detach();
    virtual void Notify(const CgsSound::Io::MessageHeader* apMessage);

    static BrnSound::Logic::IResourceRequester* CreateObjec(u32 auFlavour);

private:
    f32 UpdatePopingDuration(f32 afTimeStep);
    bool SelectSampleIndex(eSweetenerBankElement aeElement, s32& ariSampleIndex);
    void CreateVoices();
    void UpdateSweetenerInfo(const Attrib::Gen::vehicleengine& arEngineAttributes);
    void UpdateDamageBangs(f32 afTimeStep);
    void UpdateCarStart(f32 afTimeStep);
    void EmitPop(f32 afIntensity);
    CgsSound::Logic::VoiceWrapper::CreateParams MakeSplicerParams(
        const CgsSound::Logic::Content* apContent) const;

    eSweetenersState GetUpdateState() const
    {
        return static_cast<eSweetenersState>(mi8UpdateState);
    }
    eSweetenerBankElement GetCurrentBankElement() const
    {
        return static_cast<eSweetenerBankElement>(mi8CurrentBankElement);
    }
    void SetUpdateState(eSweetenersState aeState)
    {
        mi8UpdateState = static_cast<s8>(aeState);
    }
    void SetCurrentBankElement(eSweetenerBankElement aeElement)
    {
        mi8CurrentBankElement = static_cast<s8>(aeElement);
    }

    bool mbEnableSweetners;
    SweetenerInfo maSweetenerInfo[E_SWEETENER_BANK_ELEMENT_COUNT];
    CgsSound::Logic::VoiceWrapper mExhaustPopsVoice;
    CgsSound::Logic::VoiceWrapper mSweetenerVoice;
    CgsNumeric::Random mRandomGenerator;
    PhysicsControl* mpPhysicsControl;
    ShiftControl* mpShiftingControl;
    CgsSound::Logic::Content mSweetenersBank;
    f32 mfTimeRemainingToPlaySputters;
    CgsSound::Utils::SqaureWave mPopsSquareWave;
    s16 mi16SweetenerSlot;
    s8 mi8UpdateState;
    s8 mi8CurrentBankElement;
    CgsSound::Utils::DataPoint<
        BrnWorld::RaceCarEntityModuleIO::EActiveRaceCarEngineState> meRaceCarEngineState;
    s8 miRaceCarIndex;
    const CgsSound::Logic::Content* mpFXBank;
    CgsSound::Logic::VoiceWrapper mDamageBangVoice;
    f32 mfDamageBangVolume;
    CgsSound::Logic::VoiceWrapper mDamagePopVoice;
    f32 mfDamagePopVolume;
    CgsSound::Utils::DataPoint<f32> mfDelayToBang;
    CgsSound::Utils::DataPoint<f32> mfDeleayToVFXFire;
    CgsSound::Logic::VoiceWrapper mCarStarting;
    CgsSound::Utils::PathLine<2u> mFadeOutEngine;
    f32 mfTimeIntoStart;
    ECarStartingState meCarStartingState;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif
