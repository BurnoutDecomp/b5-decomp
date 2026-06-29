// ============================================================================
// CgsSoundLogicModule.cpp -- CgsSound::Logic::Module runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   CgsSound::Logic::Module::Module()                         @ 0x827E0FD0
//   CgsSound::Logic::operator++(Module::EPrepareState&, int)  @ 0x82681D30
//
// MINIMAL-SLICE NOTE: the constructor (skeleton sub-object construction) and the
// prepare-stage post-increment operator are homed here. The rest of the Module keystone
// (Prepare/Release, the VariableEventQueue, the Environment) is left to its full owning TU.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"

#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"          // CgsSound::Logic::MicrophoneSystem (embedded sub-object)
#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"               // Nicotine::IDynamicMixer (embedded mixer base)
#include <eathread/eathread_rwmutex.h>                                  // EA::Thread::RWMutex (the two base locks)
#include <new>                                                          // placement new (construct owned sub-objects at their byte offsets)

namespace CgsSound
{
namespace Playback
{
// The embedded Playback::Module sub-object (constructed in place at this+0x238). Its real
// home is the separate CgsSound::Playback::Module::Module TU; declaration-only here so the
// per-TU `cl /c` gate is satisfied (the body resolves at link time), exactly as the sibling
// subsystem-module ctors forward-declare their owned sub-objects.
namespace Module
{
    struct Module { Module(); };
}
}

namespace Logic
{

// Vtable symbols the constructor installs (defined elsewhere -- external data emitted by
// the compiler for the corresponding classes; declaration-only here):
//   gpModuleBaseVTable -- the base-most CgsModule subsystem-module vtable (off_820CE500),
//                         installed first at +0x000 (shared with the sibling module ctors).
//   gpLogicModuleVTable -- Module's most-derived vtable (off_820D00E0).
//   gpModuleMixerVTable -- the Module-owned mixer vtable (off_820CDC40), overriding the
//                          embedded IDynamicMixer's vtable at the mixer sub-object @+0x2C30.
extern void* const gpModuleBaseVTable;
extern void* const gpLogicModuleVTable;
extern void* const gpModuleMixerVTable;

// X360 byte offsets the ctor writes (authoritative, from the @0x827E0FD0 asm).
static const int KI_BASE_VTABLE        = 0x0000;  // stw r11, 0(r31)
static const int KI_BASE_RWMUTEX_A     = 0x0010;  // addi r3, r31, 0x10  -> RWMutex(NULL, true)
static const int KI_BASE_RWMUTEX_B     = 0x0118;  // addi r3, r31, 0x118 -> RWMutex(NULL, true)
static const int KI_PLAYBACK_MODULE    = 0x0238;  // addi r3, r31, 0x238
static const int KI_MICROPHONE_SYSTEM  = 0x29A0;  // r30 = r31 + 0x2950; addi r3, r30, 0x50
static const int KI_MIXER_BASE         = 0x2C30;  // r30 += 0x2E0  -> Nicotine::IDynamicMixer ctor
static const int KI_MIXER_FIELD_0x20   = 0x2C50;  // stw r10, 0x20(r30)   (mixer-base +0x20 = 0)
static const int KI_TRAILING_FLAG      = 0x2C74;  // stb r10, 0x2C74(r31) (single byte = 0)

// ---------------------------------------------------------------------------
// CgsSound::Logic::Module::Module() @0x827E0FD0
//   *(this+0x000)   = gpModuleBaseVTable                     ; base-most vtable
//   RWMutex(this+0x010, NULL, true)                          ; base read/write lock A
//   RWMutex(this+0x118, NULL, true)                          ; base read/write lock B
//   *(this+0x000)   = gpLogicModuleVTable                    ; most-derived vtable hand-off
//   CgsSound::Playback::Module::Module(this+0x238)           ; mPlaybackModule
//   CgsSound::Logic::MicrophoneSystem(this+0x29A0)           ; mMicrophoneSystem
//   Nicotine::IDynamicMixer(this+0x2C30)                     ; embedded mixer base
//   *(this+0x2C50)  = 0                                      ; mixer-derived field (+0x20)
//   *(this+0x2C30)  = gpModuleMixerVTable                    ; override the mixer vtable
//   *(this+0x2C74)  = (u8)0                                  ; trailing byte flag
// The X360 r4=0/r5=1 args to RWMutex map to RWMutex(pParameters=NULL, bDefaultParameters=true).
// ---------------------------------------------------------------------------
Module::Module()
{
    char* lpcThis = reinterpret_cast<char*>(this);

    // --- CgsModule subsystem-module base sub-object (inlined) ---
    // Install the base-most vtable, build the two base read/write locks, then hand off to
    // the most-derived (Module) vtable -- the standard subsystem-module ctor sequence.
    *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) =
        const_cast<void*>(gpModuleBaseVTable);
    new (lpcThis + KI_BASE_RWMUTEX_A) EA::Thread::RWMutex(0, true);
    new (lpcThis + KI_BASE_RWMUTEX_B) EA::Thread::RWMutex(0, true);
    *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) =
        const_cast<void*>(gpLogicModuleVTable);

    // --- mPlaybackModule (CgsSound::Playback::Module::Module) at +0x238 ---
    new (lpcThis + KI_PLAYBACK_MODULE) CgsSound::Playback::Module::Module();

    // --- mMicrophoneSystem (CgsSound::Logic::MicrophoneSystem) at +0x29A0 ---
    new (lpcThis + KI_MICROPHONE_SYSTEM) MicrophoneSystem();

    // --- embedded Nicotine::IDynamicMixer-derived mixer at +0x2C30 ---
    // Construct the IDynamicMixer base in place, clear the derived-mixer field at +0x20, then
    // override the just-installed IDynamicMixer vtable with the Module-owned mixer vtable.
    new (lpcThis + KI_MIXER_BASE) Nicotine::IDynamicMixer();
    *reinterpret_cast<s32*>(lpcThis + KI_MIXER_FIELD_0x20) = 0;
    *reinterpret_cast<void**>(lpcThis + KI_MIXER_BASE) =
        const_cast<void*>(gpModuleMixerVTable);

    // --- trailing byte flag at +0x2C74 ---
    *reinterpret_cast<u8*>(lpcThis + KI_TRAILING_FLAG) = 0;
}

// ---------------------------------------------------------------------------
// operator++(Module::EPrepareState&, int)  @ 0x82681D30
//   v1 = *a1; *a1 = v1 + 1; if (v1 + 1 > 6) <assert>; return v1;
// (asm: lwz r31,0(r3) [old]; addi r11,r31,1 [new]; cmpwi cr6,r11,6; stw r11,0(r3)
//  [store new UNCONDITIONALLY]; ble cr6,skip; Begin/Fire/End; mr r3,r31 [return old].)
// The store of the incremented value happens before the guard, so the increment is
// applied even on the assert path; the return is always the saved old value.
// ---------------------------------------------------------------------------
Module::EPrepareState operator++(Module::EPrepareState& leEnumIndex, int)
{
    const Module::EPrepareState leOldEnumIndex = leEnumIndex; // lwz r31, 0(r3)
    leEnumIndex = static_cast<Module::EPrepareState>(static_cast<s32>(leEnumIndex) + 1); // addi/stw

    CGS_ASSERT(leEnumIndex <= Module::eModulePrepareDone,
               "leEnumIndex <= Module::eModulePrepareDone");

    return leOldEnumIndex; // mr r3, r31
}

} // namespace Logic
} // namespace CgsSound
