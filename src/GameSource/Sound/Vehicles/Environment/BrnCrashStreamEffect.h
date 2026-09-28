#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_CRASH_STREAM_EFFECT_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_CRASH_STREAM_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"             // committed IStreamUser third base (BY NAME)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"       // CgsSound::Playback::Name (MakeHash) (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"            // CgsSound::Logic::Voice (mSubmix)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"             // CgsSound::Utils::DataPoint

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
// The effect owns a submix voice (connected to the global collision submix) and streams
// a rotating crash or show-time take through it whenever the player's fatal-crash state
// or the show-time mode flips.
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
    bool Prepare(CgsSound::Logic::State* apState) override;
    void UpdateParams(f32 afTimeStep) override;
    bool Detach() override;

    const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const override;
    void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afElapsedTime) override;

    // Members in declaration order.
    CgsSound::Logic::VoiceWrapper::CreateParams  mParams;
    CgsSound::Utils::DataPoint<bool>             mbShowTime;
    CgsSound::Utils::DataPoint<bool>             mbIsCrashing;
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl;
    CgsSound::Logic::Voice                       mSubmix;
    ePrepareState                                mePrepareState;
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_CRASH_STREAM_EFFECT_H
