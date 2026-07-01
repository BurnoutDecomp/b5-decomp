#include "GameSource/Sound/Vehicles/Engines/BrnSweetenersEffect.h"

#include <cstring>   // std::memcpy

// =============================================================================
// BrnSound::Vehicles::Engines::SweetenersEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CreateObjec(u32)               @ 0x826CF5A0  (the factory hook)
//   SweetenersEffect()             @ 0x826CF290  (MSVC inlined full-object ctor)
//   `vector deleting destructor'   @ 0x826E4B18  (-> ~SweetenersEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// SweetenersEffect::CreateObjec  @ 0x826CF5A0   (the factory hook)
//
// The X360 allocates an 800-byte (0x320) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "SweetenersEffect" and placement-constructs a
// SweetenersEffect, returning `this + 4` (the IResourceRequester secondary-base view).
// `luFlavour` only selects the operator-new flavour (0/1).
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the +4 adjust is the static_cast to the
// IResourceRequester base. The 0x320 size is documentation only.
// ---------------------------------------------------------------------------
BrnSound::Logic::IResourceRequester* SweetenersEffect::CreateObjec( u32 luFlavour )
{
    (void)luFlavour; // selects only the operator-new flavour (0/1) on the X360
    return static_cast<BrnSound::Logic::IResourceRequester*>( new SweetenersEffect() );
}

// ---------------------------------------------------------------------------
// SweetenersEffect::SweetenersEffect  @ 0x826CF290   (MSVC inlined full-object ctor)
//
// The dual-vptr install + base-region zero stores are the compiler-emitted
// BrnEffectObject base sub-object construction, reproduced by the base default ctor
// (reused BY NAME). The body then constructs the five embedded VoiceWrappers and the
// PathLine<2>, and seeds the per-effect RNG jitter table.
//
// FLAG (un-homed leaf members -- DEFERRED): the X360 ctor also installs an inner
// {off_820B3250,0,0} sub-object @ +0x158 and zero-/one-inits leaf scalar fields whose
// names/types (and the +0x158 vtable identity) are un-homed. Those are DECLARATION-ONLY
// / DEFERRED (see header) and NOT fabricated. The +0x158 sub-object is NOT bound to the
// committed CgsSound::Playback::Content (unproven identity).
// ---------------------------------------------------------------------------
SweetenersEffect::SweetenersEffect()
    : BrnEffectObject()   // installs the base vptrs + zero-inits the base members (BY NAME)
    , mVoice0()           // bl VoiceWrapper::VoiceWrapper(this+0x7C)
    , mVoice1()           // bl VoiceWrapper::VoiceWrapper(this+0xCC)
    , mVoice2()           // bl VoiceWrapper::VoiceWrapper(this+0x1D4)
    , mVoice3()           // bl VoiceWrapper::VoiceWrapper(this+0x228)
    , mVoice4()           // bl VoiceWrapper::VoiceWrapper(this+0x28C)
    , mPathLine()         // bl PathLine<2>::PathLine/ClearStages(this+0x2DC)
{
    // ----- per-effect random-jitter table seed (attestable; store-for-store) --------
    // X360 (0x826CF3B0..0x826CF57C): slot[0] = 1.0f, then SEVEN computed writes fill
    // slots ((++counter)&7) = 1..7 with random floats in [1.0, 2.0): each slot =
    // reinterpret(0x3F800000 | ((state>>32) & 0x7FFFFF)). The LCG is
    // state = state * 0x5851F42D4C957F2D + 1, seeded 0x1AD0891BC87CD8C9. After the 7
    // writes a final bare (++counter)&7 runs with NO store. slot[0] is NOT overwritten.
    mafJitterTable[0] = 1.0f;
    u64 lu64State = 0x1AD0891BC87CD8C9ULL;
    u32 luCounter = 0;
    for ( s32 li = 0; li < 7; ++li )
    {
        const u32 luHigh = static_cast<u32>( lu64State >> 32 ); // srdi r,state,32
        luCounter = ( luCounter + 1 ) & 7;                       // addi+1 ; clrlwi ,29
        const u32 luBits = 0x3F800000u | ( luHigh & 0x007FFFFFu ); // inslwi ,23,9
        f32 lfValue;
        std::memcpy( &lfValue, &luBits, sizeof(lfValue) );
        mafJitterTable[luCounter] = lfValue;                     // stwx r,r*4,r11
        lu64State = lu64State * 0x5851F42D4C957F2DULL + 1ULL;     // mulld ; addi+1
    }
    luCounter = ( luCounter + 1 ) & 7; // trailing bare counter bump (0x826CF570); no write
    (void)luCounter;
}

// ---------------------------------------------------------------------------
// ~SweetenersEffect  @ 0x826E4B18  (anchor for the X360 `vector deleting destructor').
// The member teardown -- the five embedded VoiceWrappers, the PathLine<2>, and the
// inherited BrnEffectObject dual-base settle -- is produced by the base destructor
// chain + the embedded member dtors (BY NAME), so this leaf body adds nothing. The
// (a2 & 1) allocator-free tail is left to the host toolchain (off_82FFB954 not homed).
// ---------------------------------------------------------------------------
SweetenersEffect::~SweetenersEffect()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
