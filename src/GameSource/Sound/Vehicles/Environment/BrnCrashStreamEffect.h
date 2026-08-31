#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_CRASH_STREAM_EFFECT_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_CRASH_STREAM_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"             // committed IStreamUser third base (BY NAME)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"       // CgsSound::Playback::Name (MakeHash) (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::CrashStreamEffect
//   GameSource/Sound/Vehicles/Environment/BrnCrashStreamEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The crash/show-time stream EFFECT OBJECT.
// The X360 ctor installs THREE leaf vptrs (this+0 primary + this+4 IResourceRequester
// == the committed BrnEffectObject dual base; this+0x38 == the IStreamUser interface
// sub-object), so it multiply-inherits the committed BrnEffectObject + IStreamUser,
// matching the committed SpeechEffect / PresentationEffect triple-base pattern.
//
// FLAG (MINIMAL home): the inlined leaf zero-inits across +0x3C..+0x74 (incl. the -1
// @ +0x68) target un-homed leaf-member types (mParams == VoiceWrapper::CreateParams;
// mbShowTime/mbIsCrashing == DataPoint<bool>; mSubmix == CgsSound::Logic::Voice;
// mpPhysicsControl == opaque ptr) -- DECLARATION-DEFERRED, NOT fabricated. Only the
// bases (BY NAME) + the attested mePrepareState are materialised.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines { struct PhysicsControl; }
namespace Environment
{

struct CrashStreamEffect : public BrnSound::Logic::BrnEffectObject,
                           public BrnSound::Logic::Streaming::IStreamUser
{
    // DWARF BrnCrashStreamEffect.h:10.
    enum ePrepareState
    {
        E_PREPARE_STATE_CONSTRUCT_VOICE = 0,
        E_PREPARE_STATE_CONNECT_VOICE   = 1,
        E_PREPARE_STATE_DONE            = 2,
    };

    CrashStreamEffect();            // @ 0x826B9CB8
    virtual ~CrashStreamEffect();   // anchor for the vector deleting destructor @ 0x826D0E98

    // @ 0x826B9E50 -- RTTI factory hook. Returns the EffectObject* base view.
    static CgsSound::Logic::EffectObject* CreateObject( u32 luType );

    // @ 0x8269B908 (DWARF h:135). Build a rotating content-spec name and intern it.
    const CgsSound::Playback::Name GetContentSpecToPlay( bool bShowTime ) const;

    s32 GetController(s32 aiIndex) override;
    void AttachController(CgsSound::Logic::EffectBase* apController) override;

    const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const override;
    void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afElapsedTime) override;

    CgsSound::Logic::VoiceWrapper::CreateParams mParams;
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl;

    ePrepareState mePrepareState;   // @ +0x80 (DWARF h:47; ctor seeds E_PREPARE_STATE_CONSTRUCT_VOICE)
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_CRASH_STREAM_EFFECT_H
