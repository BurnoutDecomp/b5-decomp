#ifndef BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_EFFECT_H
#define BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_EFFECT_H

#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"
#include "rw/math/vpu/types.h"

namespace BrnSound { namespace World { struct StaticSoundEntity; } }

namespace BrnSound
{
namespace Logic
{
namespace World
{

class Emitter3dControl;

// World-map effect object.  The member list and order are the DecFIGS shape;
// ARTIST supplies the attach/update/detach behaviour in the implementation.
struct EmitterEffect : public BrnEffectObject
{
    EmitterEffect();
    virtual ~EmitterEffect();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
        GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    virtual s32 GetController(s32 aiIndex);
    virtual void AttachController(CgsSound::Logic::EffectBase* apController);
    virtual bool Attach();
    virtual void ProcessUpdate();
    virtual bool Detach();

    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
        GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 au32Param);

protected:
    const BrnSound::World::StaticSoundEntity& GetSoundEntity() const;

    CgsSound::Logic::VoiceWrapper mVoice;
    rw::math::vpu::Vector3 mPos;
    Emitter3dControl* mp3dControl;
    s16 mi16PitchOutput;
};

} // namespace World
} // namespace Logic
} // namespace BrnSound

#endif
