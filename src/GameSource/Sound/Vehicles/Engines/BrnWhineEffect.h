#ifndef BRN_SOUND_VEHICLES_ENGINES_WHINE_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_WHINE_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"

// =============================================================================
// BrnSound::Vehicles::Engines::WhineEffect
//   GameSource/Sound/Vehicles/Engines/BrnWhineEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Engine-whine sound EFFECT OBJECT.
// Reuses the committed BrnEffectObject dual base BY NAME.
//
namespace CgsSound { namespace Io { class MessageHeader; } }
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct PhysicsControl;

struct WhineEffect : public BrnSound::Logic::BrnEffectObject
{
    WhineEffect();
    virtual ~WhineEffect();

    virtual const char* GetTypeName() const;
    virtual s32 GetController(s32 aiSlot);
    virtual void AttachController(CgsSound::Logic::EffectBase* apController);
    virtual void SetupLoadData();
    virtual bool Attach();
    virtual void UpdateParams(f32 afTimeStep);
    virtual void ProcessUpdate();
    virtual bool Detach();
    virtual void Notify(const CgsSound::Io::MessageHeader* apMessage);

    CgsSound::Logic::VoiceWrapper mWhineVoice;
    PhysicsControl* mpPhysicsControl;
    f32 mfWhineVolume;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_WHINE_EFFECT_H
