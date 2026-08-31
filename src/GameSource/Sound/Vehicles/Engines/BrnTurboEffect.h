#ifndef BRN_SOUND_VEHICLES_ENGINES_TURBO_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_TURBO_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

// =============================================================================
// BrnSound::Vehicles::Engines::TurboEffect
//   GameSource/Sound/Vehicles/Engines/BrnTurboEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Turbo spool/blowoff engine sound
// EFFECT OBJECT. Reuses the committed BrnEffectObject dual base BY NAME.
//
namespace CgsSound { namespace Io { class MessageHeader; } }
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct EngineControl;
struct HybridExhaustControl;

struct TurboEffect : public BrnSound::Logic::BrnEffectObject
{
    enum eTurboState
    {
        E_TURBO_NONE = 0,
        E_TURBO_SPOOLING = 1,
        E_TURBO_PLAY_BLOWOFF = 2,
    };

    TurboEffect();
    virtual ~TurboEffect();

    virtual const char* GetTypeName() const;
    virtual s32 GetController(s32 aiSlot);
    virtual void AttachController(CgsSound::Logic::EffectBase* apController);
    virtual void SetupLoadData();
    virtual bool Attach();
    virtual void UpdateParams(f32 afTimeStep);
    virtual void ProcessUpdate();
    virtual bool Detach();
    virtual void Notify(const CgsSound::Io::MessageHeader* apMessage);

    CgsSound::Logic::VoiceWrapper mTurboVoice;
    CgsSound::Utils::DataPoint<eTurboState> mTurboState;
    f32 mfTurboVolume;
    f32 mfTurboSpool;
    f32 mfTurboSpoolScale;
    u8 mu8TurboBlowoff;
    f32 mfTimeAtPeak;
    EngineControl* mpEngineControl;
    HybridExhaustControl* mpHybridControl;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_TURBO_EFFECT_H
