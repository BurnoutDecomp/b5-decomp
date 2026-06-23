#include "SharedClasses/Sound/World/BrnSoundWorldScene.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (mpLogicModule tripwire)

// =============================================================================
// BrnSound::Logic::World::SoundWorldScene -- out-of-line body for the single
// ledger function owned by this TU:
//   SoundWorldScene::GetResource  @ 0x82686590
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// See BrnSoundWorldScene.h for the layout map, the virtual-dispatch model, and the
// minimal-flagged-home / DEFERRED-accessor notes.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// ---------------------------------------------------------------------------
// SoundWorldScene::GetResource  @ 0x82686590
//
//   lwz   module, 0x388(this)           ; mpLogicModule
//   if (!module) assert("mpLogicModule", BrnSoundWorldScene.cpp)  ; non-gating
//   addi  provider, module, 0x4C90      ; &embedded scene-resource provider
//   lwz   vtbl, 0(provider)             ; provider vtable
//   lwz   fn,   4(vtbl)                  ; vtable slot 1
//   bctrl                                ; return provider->GetResource()
//
// Faithful virtual dispatch: fetch the logic module's embedded scene-resource
// provider (the X360 sub-object at module + 0x4C90, reached here BY NAME via the
// DEFERRED GetSceneResourceProvider accessor) and invoke its slot-1 virtual to
// return the resource. The mpLogicModule null check is the CGS_ASSERT-vacuous
// tripwire (non-gating).
// ---------------------------------------------------------------------------
void* SoundWorldScene::GetResource()
{
    CGS_ASSERT( mpLogicModule != nullptr, "mpLogicModule" );
    return GetSceneResourceProvider( *mpLogicModule ).GetResource();
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
