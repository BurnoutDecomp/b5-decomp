#ifndef BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_EFFECT_H
#define BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_EFFECT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // Vector3 (mPosition)
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // PassbyEffect base (committed, BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"             // CgsSound::Logic::Voice (mVoice)
#include "GameSource/AttribSys/Generated/classes/passbybin.h"        // Attrib::Gen::passbybin (mAttribs)
#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"           // PassbyStateManager::Passby

// =============================================================================
// BrnSound::Logic::Passby::Passby3DControl / PassbyEffect
//   GameSource/Sound/Passby/BrnPassbyEffect.h (DWARF home) +
//   GameSource/Sound/Passby/BrnPassbyEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. A pass-by is a one-shot splicer voice: a
// car, a prop or a piece of the world rushing past the microphone. The producers
// (AIPhysicsControl::PlayPassBy, PhysicsControl::UpdateCollisionPassbys,
// StaticPassbyControl, PassbyStateManager::UpdateDynamicPropBys) post a
// PassbyStateManager::Passby; the manager hands each posted record to a free PassbyState
// (8 of them), whose PassbyEffect plays one sample from the passbybin chosen by the
// record's type and follows the source with its Passby3DControl.
//
// The X360 objects (the ctor @0x826AFDC8, CreateObject @0x826BF678 allocates 0x90 bytes):
//   PassbyEffect  +0x00 IResourceRequester vptr (off_820B13CC) / +0x04 the EffectBase
//                 sub-object (vtable off_820B1398, every virtual below runs on it)
//                 +0x38 mVoice  +0x44 mAttribs  +0x60 mPosition  +0x70 meLifetime
//                 +0x74 mpPassby3DControl  +0x78 mePrepareState  +0x7C muSampleId
//                 +0x80 mfPitchScale  +0x84 mfVolumeScale  +0x88 mbPlayerIsBoosting
//                 +0x89 mbFirstUpdate
//   Passby3DControl  a Brn3DEffectControl with nothing of its own (0xD0 bytes; the
//                 CreateObject @0x826E8E18 ctor chain is Brn3DEffectControl() + the vptr).
// Both RTTI descriptors sit in the image's .data: PassbyEffect's sTypeInfo @0x82F2F98C
// {0x40000, "PassbyEffect", BrnEffectObject @0x82F2E7FC, CreateObject 0x826BF678} and
// Passby3DControl's @0x82F2F97C {0x40000, "Passby3DControl", Brn3DEffectControl @0x82F2E81C,
// CreateObject 0x826E8E18}; the CRT registers them at 0x82C633B4 / 0x82C633A4. ObjectID
// 0x40000 = state manager 4 (PassbyStateManager), effect 0.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME; the X360 offsets
// above are documentation only and are not static_asserted (pointer/vptr widths differ).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// BrnPassbyEffect.h:33 (DWARF). Reuses the Brn3DEffectControl base by name; the
// virtual (scalar/vector deleting) destructor @ 0x826E8ED0 runs the inherited
// teardown (incl. the Attrib::Instance member via Attrib::Instance::~Instance).
struct Passby3DControl : public BrnSound::Logic::Brn3DEffectControl
{
    Passby3DControl() {}
    virtual ~Passby3DControl();

    // BrnPassbyEffect.cpp:63 (DWARF) -- the RTTI surface. GetTypeInfo @0x82689008 returns
    // &sTypeInfo (0x82F2F97C), GetTypeName @0x82689018 "Passby3DControl", CreateObject
    // @0x826E8E18 the factory.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const override;
    virtual const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();   // h:35
    static CgsSound::Logic::EffectControl* CreateObject(u32 auAllocator);
};

// BrnPassbyEffect.h:42 (DWARF): PassbyEffect : public BrnSound::Logic::BrnEffectObject.
struct PassbyEffect : public BrnSound::Logic::BrnEffectObject
{
    // BrnPassbyEffect.h:107 (DWARF).
    enum EPassbyLifetime
    {
        E_NOT_PLAYED = 0,
        E_HAS_PLAYED = 1,
        E_CULLED     = 2,
    };

    // BrnPassbyEffect.h:116 (DWARF).
    enum ePrepareState
    {
        E_PREPARE_STATE_CONSTRUCT_VOICE = 0,
        E_PREPARE_STATE_CONNECT_VOICE   = 1,
    };

    PassbyEffect();              // h:188 -- @0x826AFDC8
    virtual ~PassbyEffect();     // cpp:86 -- @0x826BF6D8 (the scalar deleting destructor @0x826C9200)

    // BrnPassbyEffect.cpp:67 (DWARF) -- the RTTI surface.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    virtual const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();    // h:43
    static CgsSound::Logic::EffectObject* CreateObject(u32 auAllocator);

    virtual s32  GetController(s32 aiIndex) override;                                // cpp:106
    virtual void AttachController(CgsSound::Logic::EffectBase* apController) override; // cpp:135
    virtual bool Prepare(CgsSound::Logic::State* apState) override;                  // cpp:165
    virtual bool Attach() override;                                                  // cpp:432
    virtual void UpdateParams(f32 afDeltaTime) override;                             // cpp:224
    virtual void ProcessUpdate() override;                                           // cpp:306
    virtual bool Detach() override;                                                  // cpp:526

private:
    // BrnPassbyEffect.cpp:551 @0x82689108. The sample to play: the next of the bin's
    // boost / normal range, round robin over the shared smuSampleIndex.
    u32 ChooseSampleId(const PassbyStateManager::Passby& arPassby, bool abPlayerIsBoosting);

    // BrnPassbyEffect.cpp:605 -- inlined at every caller: the owning PassbyState's record
    // (asserts "lpState", cpp:616).
    const PassbyStateManager::Passby& GetPassbyData() const;

    // BrnPassbyEffect.cpp:624 @0x826BF778. Follow the source: the static position, or the
    // 3D control's current emitter position while that control is still on the same attach.
    bool UpdatePosition();

    // BrnPassbyEffect.cpp:660 @0x826BF828. |source velocity - listener velocity|.
    f32 GetRelativeVelocityMag() const;

    // BrnPassbyEffect.cpp:58 -- the shared round-robin cursor (.bss dword_82FFB944).
    static u32 smuSampleIndex;

    CgsSound::Logic::Voice mVoice;          // h:156  (+0x38)
    Attrib::Gen::passbybin mAttribs;        // h:158  (+0x44)
    Vector3                mPosition;       // h:159  (+0x60)
    EPassbyLifetime        meLifetime;      // h:160  (+0x70)
    Passby3DControl*       mpPassby3DControl; // h:161 (+0x74)
    ePrepareState          mePrepareState;  // h:162  (+0x78)
    u32                    muSampleId;      // h:164  (+0x7C)
    f32                    mfPitchScale;    // h:166  (+0x80)
    f32                    mfVolumeScale;   // h:167  (+0x84)
    bool                   mbPlayerIsBoosting; // h:169 (+0x88)
    bool                   mbFirstUpdate;   // h:170  (+0x89)
};

} // namespace Passby
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_EFFECT_H
