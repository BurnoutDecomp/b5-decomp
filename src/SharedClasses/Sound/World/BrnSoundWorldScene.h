#ifndef BRN_SOUND_LOGIC_WORLD_SOUND_WORLD_SCENE_H
#define BRN_SOUND_LOGIC_WORLD_SOUND_WORLD_SCENE_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::World::SoundWorldScene
//   Sound/World/BrnSoundWorldScene.h (DWARF home) +
//   Sound/World/BrnSoundWorldScene.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// SoundWorldScene is the sound-logic view of the active world scene. It holds (by
// pointer, at +0x388) the owning SoundLogicModule and forwards resource queries to
// the module's embedded scene resource provider.
//
// This TU bodies ONE ledger function:
//   BrnSound::Logic::World::SoundWorldScene::GetResource  @ 0x82686590
//
// X360 body (asm at 0x82686590):
//   lwz   r11, 0x388(this)     ; mpLogicModule           (assert "mpLogicModule",
//                                                          BrnSoundWorldScene.cpp)
//   addi  r3,  r11, 0x4C90     ; provider = embedded scene-resource provider sub-object
//   lwz   r11, 0(r3)           ; provider vtable
//   lwz   r11, 4(r11)          ; vtable slot 1 (GetResource)
//   bctrl                       ; return provider->GetResource()
//   return r3
//
// => return mpLogicModule->GetSceneResourceProvider().GetResource(), a virtual
// dispatch through the provider sub-object embedded in the SoundLogicModule.
//
// MINIMAL FLAGGED HOME: only mpLogicModule (+0x388) and GetResource() are
// materialised. The provider sub-object lives inside the committed (and largely
// DEFERRED) SoundLogicModule at +0x4C90; rather than reach a raw offset (forbidden)
// or grow that committed type's layout (must not reorder its members), this home
// models the access through the abstract ISceneResourceProvider interface below
// and a DECLARED-only accessor (GetSceneResourceProvider) whose body lands with the
// full SoundLogicModule TU. The virtual dispatch (slot 1) is preserved faithfully.
// =============================================================================

namespace BrnSound
{
namespace Module { struct SoundLogicModule; }

namespace Logic
{
namespace World
{

// The polymorphic scene-resource provider embedded in the SoundLogicModule. The
// X360 GetResource calls its second vtable slot (offset 4) to fetch the resource;
// the first slot (offset 0) is the destructor/RTTI hook. Modelled here as an
// abstract interface so the slot-1 virtual dispatch is reproduced faithfully
// without reaching a raw offset.
//
// FLAG: interface SHAPE recovered from the call (a polymorphic object whose 2nd
// virtual returns a resource pointer); its DWARF name/home and the resource's
// concrete type are UNVERIFIED and DEFERRED. GetResource is therefore typed to
// return an opaque `void*` resource handle (X360 returns the call result in r3).
struct ISceneResourceProvider
{
    virtual ~ISceneResourceProvider() {}      // vtable slot 0
    virtual void* GetResource() = 0;          // vtable slot 1 (called by SoundWorldScene::GetResource)
};

// BrnSoundWorldScene.h (DWARF home).
struct SoundWorldScene
{
    // BrnSoundWorldScene.cpp (DWARF assert site, "mpLogicModule"). Forward the
    // resource query to the logic module's embedded scene-resource provider.
    // @ 0x82686590.
    void* GetResource();

    // BrnSoundWorldScene.cpp:143 (DWARF). Bind the owning logic module + the resource
    // file extension into the scene; report readiness (true). @ 0x82686530. Asserts both
    // arguments non-null (non-gating tripwire), then stores mpcResourceExt then mpLogicModule.
    bool Prepare( BrnSound::Module::SoundLogicModule* lpLogicModule, const char* lpcResourceExt );

    // -- FLAGGED layout (offset is an X360 fact; leading span types DEFERRED) --
    u8                         maLeading[0x388]; // +0x000..+0x387 (opaque scene state)
    BrnSound::Module::SoundLogicModule* mpLogicModule; // +0x388 (asserted "mpLogicModule")
    const char*                mpcResourceExt;   // +0x38C (resource file extension; Prepare stores it)
};

// DECLARED-only accessor: returns the SoundLogicModule's embedded scene-resource
// provider (the X360 sub-object at SoundLogicModule + 0x4C90). Its body is DEFERRED
// to the full SoundLogicModule TU (which owns that sub-object's layout); declaring
// it here lets SoundWorldScene::GetResource express the virtual dispatch by NAME
// without reaching a raw offset or perturbing the committed SoundLogicModule layout.
ISceneResourceProvider& GetSceneResourceProvider( BrnSound::Module::SoundLogicModule& lrModule );

} // namespace World
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_WORLD_SOUND_WORLD_SCENE_H
