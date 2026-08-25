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
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h" // the REAL Playback::Module::Module (embedded @+0x238)
#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"               // Nicotine::IDynamicMixer (embedded mixer base)
#include <eathread/eathread_rwmutex.h>                                  // EA::Thread::RWMutex (the two base locks)
#include <new>                                                          // placement new (construct owned sub-objects at their byte offsets)

namespace CgsSound
{
namespace Logic
{

// The three vtable slots the X360 ctor installs (off_820CE500 base-most /
// off_820D00E0 most-derived / off_820CDC40 the Module-owned mixer override).
// (2026-08-25 wave 5: these used to be three INVENTED undefined `extern void*
// const` symbols -- link blockers defined nowhere. Per the wave-1 DebugComponent
// precedent, the slots are written as null with the guest vtable addresses kept
// HERE in the comments only: a guest address in a host vptr slot would arm a wild
// jump, and the host has no synthesized vtables for this raw-offset skeleton.
// When the class grows real C++ virtuals, the installs become compiler synthesis
// and these explicit stores disappear.)

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
    // Install the base-most vtable slot (X360 off_820CE500; null on the host -- see
    // the note above), build the two base read/write locks, then hand off to the
    // most-derived vtable slot (X360 off_820D00E0) -- the standard subsystem-module
    // ctor sequence.
    *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) = nullptr;   // X360: off_820CE500
    new (lpcThis + KI_BASE_RWMUTEX_A) EA::Thread::RWMutex(0, true);
    new (lpcThis + KI_BASE_RWMUTEX_B) EA::Thread::RWMutex(0, true);
    *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) = nullptr;   // X360: off_820D00E0

    // --- mPlaybackModule (the REAL CgsSound::Playback::Module::Module) at +0x238 ---
    new (lpcThis + KI_PLAYBACK_MODULE) CgsSound::Playback::Module::Module();

    // --- mMicrophoneSystem (CgsSound::Logic::MicrophoneSystem) at +0x29A0 ---
    new (lpcThis + KI_MICROPHONE_SYSTEM) MicrophoneSystem();

    // --- embedded Nicotine::IDynamicMixer-derived mixer at +0x2C30 ---
    // Construct the IDynamicMixer base in place, clear the derived-mixer field at
    // +0x20, then the X360 overrides the just-installed IDynamicMixer vtable with the
    // Module-owned mixer vtable (off_820CDC40) -- on the host that would clobber the
    // REAL IDynamicMixer vptr the placement-new just installed with a guest address,
    // so the override store is documented, not performed (the host object keeps its
    // own valid vtable until the derived mixer class exists).
    new (lpcThis + KI_MIXER_BASE) Nicotine::IDynamicMixer();
    *reinterpret_cast<s32*>(lpcThis + KI_MIXER_FIELD_0x20) = 0;
    // X360: *(this+0x2C30) = off_820CDC40  (see the note above -- not performed)

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
