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

// DWARF BrnEmitterEffect.cpp:42 -- the world-emitter debug switch (rodata-free .bss bool
// 0x82FFB8CB). The console's sound debug menu registers it as "Emitters"/"Debug"
// (BrnSound::Debug::DebugComponent::OnActivate @0x826D5CB0, 0x826D70E4..0x826D70F8) and it
// defaults to false. It gates EmitterEffect::Attach's luEmitter assert and is the lbDrawDebug
// EmitterStateManager::UpdateParams hands SoundWorldScene::Query.
extern bool KB_DEBUG_WORLD_EMITTERS;

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
