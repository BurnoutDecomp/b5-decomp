#include "GameSource/Sound/Vehicles/Environment/BrnCrashStreamEffect.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

// =============================================================================
// BrnSound::Vehicles::Environment::CrashStreamEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CrashStreamEffect()            @ 0x826B9CB8  (MSVC inlined full-object ctor)
//   CreateObject(u32)              @ 0x826B9E50  (the RTTI factory hook)
//   GetContentSpecToPlay(bool)     @ 0x8269B908
//   `vector deleting destructor'   @ 0x826D0E98  (-> ~CrashStreamEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// ---------------------------------------------------------------------------
// CrashStreamEffect::CrashStreamEffect()  @ 0x826B9CB8
//
// MSVC's INLINED full-object constructor: it inlines the base member zero-inits and
// installs THREE leaf vptrs directly (this+0 primary + this+4 IResourceRequester
// sub-object == the committed BrnEffectObject dual base; this+0x38 == the IStreamUser
// interface sub-object). In reconstructed C++ those three vptr installs + the base
// member zero-inits are produced structurally by the two base sub-objects' own default
// constructors (BrnEffectObject BY NAME + IStreamUser BY NAME). The only attested leaf
// effect modelled by name is mePrepareState = E_PREPARE_STATE_CONSTRUCT_VOICE.
//
// FLAG (un-homed leaf members): the inlined leaf zero-inits across +0x3C..+0x74 and the
// -1 @ +0x68 target CrashStreamEffect's OWN leaf members whose concrete types have NO
// committed home (mParams == VoiceWrapper::CreateParams; mbShowTime/mbIsCrashing ==
// DataPoint<bool>; mSubmix == CgsSound::Logic::Voice; mpPhysicsControl == opaque ptr).
// DECLARATION-ONLY / DEFERRED -- NOT re-emitted as raw-offset stores or invented fields.
// ---------------------------------------------------------------------------
CrashStreamEffect::CrashStreamEffect()
    : BrnEffectObject()                         // primary vptr @+0 + IResourceRequester vptr @+4, base zero-init (BY NAME)
    , BrnSound::Logic::Streaming::IStreamUser() // IStreamUser interface vptr @+0x38 (BY NAME)
    , mePrepareState(E_PREPARE_STATE_CONSTRUCT_VOICE) // stw 0, 0x80
{
    // The remaining inlined leaf zero-inits (mParams / mbShowTime / mbIsCrashing /
    // mpPhysicsControl / mSubmix region, +0x3C..+0x74, incl. the -1 @ +0x68) target
    // un-homed leaf-member types (DECLARATION-ONLY / DEFERRED; see header FLAG).
}

// ---------------------------------------------------------------------------
// CrashStreamEffect::CreateObject(u32)  @ 0x826B9E50   (the RTTI factory hook)
// Allocates a 132-byte (0x84) block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "CrashStreamEffect", placement-constructs a CrashStreamEffect, and
// returns the EffectObject base pointer (+4). The incoming u32 only selects the
// operator-new flavour (0/1). DWARF (BrnCrashStreamEffect.h:62): EffectObject*
// CreateObject(uint32_t).
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. The 0x84 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectObject* CrashStreamEffect::CreateObject( u32 /*luType*/ )
{
    return new CrashStreamEffect();
}

// ---------------------------------------------------------------------------
// CrashStreamEffect::GetContentSpecToPlay(bool bShowTime) const  @ 0x8269B908
//
// Formats a rotating content-spec name ("ShowTime00".. modulo 4, or "Crash00".. modulo
// 15) from a per-variant static counter and returns the interned Name for it. The two
// counters are function-local statics, each a u16 that post-increments every call so
// successive calls cycle through the take indices.
//
// FLAG: the X360 else-branch's 0x88888889 constant is the compiler's signed-division-
// by-15 reciprocal used to compute counter % 15; the modulo is expressed directly here.
// ---------------------------------------------------------------------------
const CgsSound::Playback::Name CrashStreamEffect::GetContentSpecToPlay(bool bShowTime) const
{
    static u16 su16ShowTimeCounter; // word_8300C6C4
    static u16 su16CrashCounter;    // word_8300C6C8

    const char* lkpcFormat;
    u32         luIndex;

    if (bShowTime)
    {
        luIndex    = static_cast<u32>(su16ShowTimeCounter++) % 4u;  // ShowTime%02u, mod 4
        lkpcFormat = "ShowTime%02u";
    }
    else
    {
        luIndex    = static_cast<u32>(su16CrashCounter++) % 15u;    // Crash%02u, mod 15
        lkpcFormat = "Crash%02u";
    }

    char lacName[32]; // v24 [sp+0x50]
    CgsCore::SPrintf(lacName, 32, lkpcFormat, luIndex);

    return CgsSound::Playback::Name(CgsSound::Playback::Name::MakeHash(lacName));
}

// ---------------------------------------------------------------------------
// ~CrashStreamEffect  @ 0x826D0E98  (anchor for the X360 `vector deleting destructor').
// All observable member teardown lives in the committed BrnEffectObject / IStreamUser
// base chains + the (deferred) leaf members (mParams/mSubmix), so this anchor body is
// empty; the host toolchain re-synthesises the vector deleting destructor + operator
// delete from this class's MI base list + virtual destructor. The (a2 & 1) allocator-
// free tail is left to the host toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
CrashStreamEffect::~CrashStreamEffect()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
