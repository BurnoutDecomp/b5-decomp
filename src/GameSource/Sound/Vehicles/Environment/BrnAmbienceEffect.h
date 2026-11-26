#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_EFFECT_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"             // IStreamUser third base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceEffect
//   GameSource/Sound/Vehicles/Environment/BrnAmbienceEffect.{h,cpp}  (DWARF home)
//
// The per-car environmental ambience EFFECT OBJECT. Like the speed and crash stream
// effects it is a BrnEffectObject (primary + IResourceRequester sub-objects) plus an
// IStreamUser interface sub-object: it streams the ambience bed of the region the
// ambience control reports, fading each new bed in over two seconds.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct AmbienceControl;

struct AmbienceEffect : public BrnSound::Logic::BrnEffectObject,
                        public BrnSound::Logic::Streaming::IStreamUser
{
    // AmbienceEffect() is implicit (DWARF BrnAmbienceEffect.h lists it without a line): ARTIST's
    // 0x826B9600 builds the bases and mParams and leaves mpAmbienceControl unset.
    // No user-declared destructor: DWARF declares ~AmbienceEffect without a line (implicit); ARTIST's
    // deleting destructor stores no AmbienceEffect vftable.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 luAllocator);
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo;   // DWARF static data member (ARTIST 0x82F2F7A8)

    s32 GetController(s32 aiIndex) ;
    void AttachController(CgsSound::Logic::EffectBase* apController) ;
    void UpdateParams(f32 afTimeStep) ;
    bool Detach() ;

    const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const ;
    void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afElapsedTime) ;

    // Members in declaration order.
    CgsSound::Logic::VoiceWrapper::CreateParams mParams;
    AmbienceControl*                            mpAmbienceControl;
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_EFFECT_H
