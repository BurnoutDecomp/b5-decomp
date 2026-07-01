#include "GameSource/Sound/Global/BrnSpeechEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::SpeechEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// SpeechEffect is a multiple-inheritance effect-object leaf: BrnEffectObject dual base
// (@ +0/+4) + Streaming::IStreamUser (@ +0x38). See BrnSpeechEffect.h for the base
// rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set:
//   SpeechEffect()                            @ 0x826BB4E8
//   `vector deleting destructor'              @ 0x826BB668  (compiler-synthesised)
//   `vector deleting destructor'`adjustor{4}' @ 0x826BB5D0  (compiler-synthesised)
//   CompassDirectionToString(int)             @ 0x82687000
//   GameModeToString(int,int)                 @ 0x82687148
//   GetCr()                                   @ 0x82687F78
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// SpeechEffect::SpeechEffect()  @ 0x826BB4E8  (MINIMAL -- see FLAGs)
//
// X360 STORE SEQUENCE (0x826BB4E8-0x826BB5B0):
//   lfs 0.0f ; stfs 0.0f, 0x20 ; stfs 0.0f, 0x1C     ; leaf f32 = 0.0f (flt_82001CC0 == 0.0f)
//   stw off_820AE954, 4                               ; (transient) base-ctor vptr @+4
//   sth 0,0x10 ; stw 0,0xC ; stw 0,0x34 ; stw 0,8 ; stb 0,0x30 ; stw 0,0x28 ; stw 0,0x24 ; sth 0,0x12
//   stw off_820ABB88, 0x38                            ; (transient) vptr @+0x38
//   stw off_820B23B0,0 ; stw off_820B237C,4 ; stw off_820B2370,0x38  ; final 3 vptrs
//   stw 0, 0x40 .. 0x68 ; stw -1, 0x6C                ; scalar block + muQueuedContentSpec = -1
//   bl  Attrib::Gen::speechdata::speechdata(this+0x74, 0, 0)
//   return this
//
// The three vptr installs (@0/@4/@0x38) are produced STRUCTURALLY by the two
// BrnEffectObject base sub-objects (primary @ +0, IResourceRequester @ +4) + the
// IStreamUser interface sub-object (@ +0x38), NOT by hand-written stores. The two
// f32=0.0f and scalar zero-inits target members whose real types/names are still
// un-homed here (mParams has no committed home); per the anti-fabrication rule and
// the committed ExplosionEffect/StreamingEffect precedent those remain DECLARATION-ONLY
// / DEFERRED and are NOT re-emitted as raw-offset stores or invented fields. The
// embedded Attrib::Gen::speechdata sub-object (this+0x74) is NOW a real named member
// (mSpeechData, see header) -- its default ctor call (speechdata(nullptr, nullptr))
// exactly matches the asm's `Attrib::Gen::speechdata::speechdata(this+0x74, 0, 0)`, so
// the compiler-generated member construction reproduces it with no explicit code here.
//
// FLAG: flt_82001CC0 == 0.0f (NOT 1.0f) -- attested project-wide and by the committed
// sibling BrnExplosionEffect.cpp which loads the same symbol.
// ---------------------------------------------------------------------------
SpeechEffect::SpeechEffect()
    : BrnEffectObject()                       // primary @+0 + IResourceRequester @+4 vptrs, base zero-init (BY NAME)
    , BrnSound::Logic::Streaming::IStreamUser() // IStreamUser interface vptr @+0x38 (BY NAME)
    // mSpeechData default-constructs here too (BY NAME, matches the asm's
    // Attrib::Gen::speechdata::speechdata(this+0x74, 0, 0) call exactly).
{
    // The leaf scalar zero-inits (two f32 = 0.0f, several word/byte/s16 = 0) and the
    // `stw -1, +0x6C` (muQueuedContentSpec = 0xFFFFFFFF) target un-homed member
    // sub-objects and are DEFERRED (see FLAGs) -- NOT fabricated as named field
    // assignments in this minimal home.
}

// ---------------------------------------------------------------------------
// ~SpeechEffect  (the out-of-line anchor the vector deleting destructor @ 0x826BB668
// and its adjustor{4} @ 0x826BB5D0 forward to).
//
//   0x826BB668 vector deleting destructor:
//     bl ~SpeechEffect ; if (a2 & 1) { free via off_82FFB954 (slot +0x14) } ; return this
//   0x826BB5D0 adjustor{4}: addi r3,r3,-4 ; b <vector deleting destructor>
//     -> recovers the primary SpeechEffect `this` from the +4 IResourceRequester
//        sub-object pointer, then tail-forwards to the vector deleting destructor.
//
// Both are MSVC compiler-synthesised thunks over this virtual destructor. The X360
// asm's ONLY non-vptr-reinstall action is `Attrib::Instance::~Instance(this+0x74)` --
// tearing down the embedded speechdata sub-object. Since mSpeechData is now a real
// named member (see header), the compiler-generated member destruction reproduces
// that exact call automatically; no explicit teardown code is needed here. The rest
// of the leaf teardown is the inherited BrnEffectObject / IStreamUser base settle (BY
// NAME), so this anchor body is empty; the host toolchain re-synthesises the vector
// deleting destructor + the +4 MI adjustor from this class's MI base list + virtual
// dtor. The (a2 & 1) tail frees the object through the global sound MemBase allocator
// (off_82FFB954); that allocator is not homed here, so the `delete` half is left to
// the host operator delete (no fabricated allocator).
// ---------------------------------------------------------------------------
SpeechEffect::~SpeechEffect()
{
}

// ---------------------------------------------------------------------------
// SpeechEffect::GetCr  @ 0x82687F78
//
//   return &maCr[0]           ; addi r3, r3, 8 ; blr
//
// Store-for-store with the X360: the whole body is `this + 8` returned in r3 -- the
// address of the sub-object at +0x08. No load, no assert, no branch: a pure
// address-of accessor. Its element TYPE is UNVERIFIED (the asm merely takes the
// address) and is modelled as the opaque placeholder `Cr`.
// ---------------------------------------------------------------------------
SpeechEffect::Cr* SpeechEffect::GetCr()
{
    return reinterpret_cast<Cr*>( &maCr[0] );
}

// ---------------------------------------------------------------------------
// SpeechEffect::CompassDirectionToString  @ 0x82687000
//   Pure switch-to-string leaf (the `this` r3 arg is never dereferenced).
// ---------------------------------------------------------------------------
const char* SpeechEffect::CompassDirectionToString( int liDirection )
{
    switch ( liDirection )
    {
        case 0: return "n";
        case 1: return "nw";
        case 2: return "w";
        case 3: return "sw";
        case 4: return "s";
        case 5: return "se";
        case 6: return "e";
        case 7: return "ne";
        default:
            // Assert message reproduced VERBATIM from X360 rodata (aUnknownCompass ==
            // "Unknown compass direction"; NO trailing '\n').
            CGS_ASSERT( false, "Unknown compass direction" );
            return "";
    }
}

// ---------------------------------------------------------------------------
// SpeechEffect::GameModeToString  @ 0x82687148
//   Pure switch-to-string leaf (a1 = `this`, unused; a2 = game-mode enum). The default
//   case returns the X360 &unk_820046A7 empty-string rodata literal ("") -- no assert
//   is fired (the asm has no BeginAssert/FireAssert/EndAssert).
// ---------------------------------------------------------------------------
const char* SpeechEffect::GameModeToString( int /*a1*/, int a2 )
{
    switch ( a2 )
    {
        case 0: return "r";
        case 3: return "rr";
        case 5: return "pc";
        case 7: return "sr";
        case 8: return "mm";
        default: return "";   // X360 &unk_820046A7 == empty-string rodata literal
    }
}

} // namespace Logic
} // namespace BrnSound
