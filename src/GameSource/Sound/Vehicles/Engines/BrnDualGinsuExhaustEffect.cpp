#include "GameSource/Sound/Vehicles/Engines/BrnDualGinsuExhaustEffect.h"

// =============================================================================
// BrnSound::Vehicles::Engines::DualGinsuExhaustEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF: DualGinsuExhaustEffect : public
// DualGinsuEffect. Recon'd function set:
//   Creat(s32)                     @ 0x826E4178  (the factory hook)
//   DualGinsuExhaustEffect()       @ 0x826E0E90  (leaf ctor)
//   `vector deleting destructor'   @ 0x826EC000  (-> ~DualGinsuExhaustEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// DualGinsuExhaustEffect::Creat  @ 0x826E4178   (the factory hook)
//
// The X360 allocates an 832-byte (0x340) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "DualGinsuExhaustEffect" and placement-constructs a
// DualGinsuExhaustEffect, handing it back as the +4 IResourceRequester sub-object view.
// `a1` only selects the operator-new flavour (0/1); both arms use the same size + ctor.
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the +4 adjust is the static_cast to the
// IResourceRequester base. The 0x340 size is documentation only.
// ---------------------------------------------------------------------------
BrnSound::Logic::IResourceRequester* DualGinsuExhaustEffect::Creat( s32 /*aiFlavour*/ )
{
    DualGinsuExhaustEffect* lpEffect = new DualGinsuExhaustEffect();
    if ( lpEffect == nullptr )
    {
        return nullptr;
    }
    // addi r3, r3, 4 -- hand the object back as its IResourceRequester sub-object.
    return static_cast<BrnSound::Logic::IResourceRequester*>(lpEffect);
}

// ---------------------------------------------------------------------------
// DualGinsuExhaustEffect::DualGinsuExhaustEffect  @ 0x826E0E90  (leaf ctor)
//
//   bl   DualGinsuEffect::DualGinsuEffect()      ; construct the DualGinsuEffect base
//   stw  off_820B57C8, 0(r31)                    ; primary leaf vptr    (this+0)
//   stw  off_820B5794, 4(r31)                    ; IResourceRequester vptr (this+4)
//   stw  0,            0x2EC(r31)                 ; meState = E_CONSTRUCTED (0)
//   bl   CgsSound::Logic::VoiceWrapper::VoiceWrapper(r31 + 0x2F0)  ; mReverseWhineVoice
//   return this
//
// MSVC's inlined full-object constructor: `bl`s the DualGinsuEffect base ctor, installs
// the two leaf vptrs (produced structurally by the derived class's dual-vtable +
// virtual dtor), zero-seeds meState (E_CONSTRUCTED), and constructs the embedded
// VoiceWrapper member mReverseWhineVoice.
// ---------------------------------------------------------------------------
DualGinsuExhaustEffect::DualGinsuExhaustEffect()
    : DualGinsuEffect()          // bl DualGinsuEffect::DualGinsuEffect()
    , meState(E_CONSTRUCTED)     // stw 0, +0x2EC(r31)
    , mReverseWhineVoice()       // bl VoiceWrapper::VoiceWrapper(this+0x2F0)
{
}

// ---------------------------------------------------------------------------
// ~DualGinsuExhaustEffect  @ 0x826EC000  (anchor for the X360 `vector deleting destructor').
// Destroys the embedded VoiceWrapper (mReverseWhineVoice) + runs the DualGinsuEffect base
// destructor (compiler-synthesised from the declared member + base); this leaf adds
// nothing of its own. The (a2 & 1) allocator-free tail is left to the host toolchain
// (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
DualGinsuExhaustEffect::~DualGinsuExhaustEffect()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
