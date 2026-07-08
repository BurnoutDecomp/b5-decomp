#pragma once

// ===========================================================================
// XAUDIO::CReverbEffect -- the XAudio I3DL2/EAX reverb effect object in
// BURNOUT_X360_ARTIST.XEX. It is a relative of the homed XAUDIO::CEffectManager
// / CSourceEffect / CVoice family and, like them, sits on the shared
// XAUDIO::CEffect base; its DSP is the princeton_digital reverb bank
// (stereo_room_3dl2_t over stereo_room_t -- see princeton_digital.h). This
// header is the canonical OWNING home for the XAUDIO::CReverbEffect TU
// (9 ledger functions).
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed purely from the X360 asm. `XAUDIO` is an external middleware
// boundary, so its identifiers are preserved verbatim per the naming
// convention.
//
// ---------------------------------------------------------------------------
// STATUS OF THE 9 LEDGER FUNCTIONS
// ---------------------------------------------------------------------------
// IMPLEMENTED here (fully asm-grounded, layout-independent):
//     XAUDIO::CReverbEffect::GetInfo                @ 0x8295F410
//
// BLOCKED -- declared/documented but NOT reconstructed, because each needs a
// collaborator this build has not yet homed. A wrong offset/vtable slot is
// worse than an honest gap (AGENTS.md faithfulness gate), so these are deferred:
//
//   * CReverbEffect::CReverbEffect                  @ 0x82962BB8
//       The ctor lays out the entire ~310 KB object: it seats two vtables
//       (a primary reverb-interface vptr @+0x00 = off_8210905C and the CEffect
//       base vptr @+0x04 = off_82109040), the refcount (muReserved4 @+0x08 = 1)
//       and mpContext (@+0x0C), then constructs the princeton_digital DSP room
//       (`stereo_room_t<float>` @+0xD0, the `stereo_room_3dl2_t<float>` wrapper
//       @+0x10 with most-derived-flag 0 so the room is its shared virtual base),
//       a `stereo_room_t::properties_t` @+0x44, and zeroes two 64-bit fields at
//       +0x4BAA0 / +0x4BAA8. Modelling that faithfully requires the
//       still-unrecovered `stereo_room_t()` ctor (@0x829664D8) and the DSP
//       sub-filter bank layout (+104..~+301000), which princeton_digital.h
//       explicitly leaves un-laid-out. Fabricating the layout/ctor is a
//       fidelity violation, so the ctor is blocked pending that work.
//
//   * CReverbEffect::GetParam                       @ 0x8295F4B8
//   * CReverbEffect::SetParam                       @ 0x8295F6C8
//       Both drive the DSP room through unrecovered offsets (the room's
//       `stereo_room_3dl2_t` vbtable dereference `*(*(*(this+0x10)+4)+this)`,
//       the room fields at +0x6C, and the two 64-bit accumulators at
//       +0x4BAA0/+0x4BAA8), convert an i3dl2 parameter block, and run the
//       Xbox-kernel recursive spin lock (KeRaiseIrqlToDpcLevel /
//       KeAcquireSpinLockAtRaisedIrql / KeReleaseSpinLockFromRaisedIrql /
//       KfLowerIrql over the un-homed global lock state at dword_83222C28..).
//       Blocked with the princeton_digital room layout.
//
//   * CReverbEffect::QueryInterface                 @ 0x82962FF0
//       COM interface adjustment: `*ppOut = (u8*)this - 4; return this`. The
//       `-4` is meaningful only within the (un-modelled) multiple-inheritance
//       base layout (the CEffect subobject stepping back to the outer reverb
//       interface); blocked with the ctor layout rather than committed to a raw
//       pointer poke whose meaning we cannot ground.
//
//   * CReverbEffect::Release                        @ 0x82962988
//       Intrusive-refcount release: `if (--refcount == 0)` it dispatches the
//       deleting destructor through the CEffect subobject's vtable slot +0x0C
//       (`(*(*(this+4)+12))(this+4)`). Per the TU guidance, a function that
//       dispatches through an un-reconstructed CEffect vtable slot is blocked
//       (the CRoutedVoice W28 precedent).
//
//   * CReverbEffect::Release`adjustor{4}'           @ 0x829629D0
//   * CReverbEffect::`scalar deleting destructor'   @ 0x82962C68
//   * CReverbEffect::`vector deleting destructor'`adjustor{4}' @ 0x82962980
//       Compiler-synthesised MI thunks over Release / the virtual destructor
//       (the scalar deleting destructor reseats the four vtables off_8210905C /
//       off_82109040 / off_82109024 / off_82109030 / off_82108FF4 during the
//       base/derived teardown chain; the adjustor thunks step `this` by -4).
//       Like the CSourceEffect / CEffectManager deleting-destructor thunks these
//       are emitted by the host compiler from the virtual dtor + the class
//       `operator delete`; they are not hand-written, and here they additionally
//       depend on the blocked layout/vtable order above.
//
// The reverb-interface base, the CEffect base wiring, and the class layout are
// therefore intentionally NOT declared as members yet: with the ctor blocked we
// do not fabricate a layout (AGENTS.md: "If the ctor is not a ledger target and
// has no asm, leave the members as opaque padding and omit the ctor" -- here the
// ctor exists but its layout deps are unrecovered, so the same no-fabrication
// rule applies). GetInfo needs none of it -- it writes only its out-parameter.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// The info block CReverbEffect::GetInfo fills for the caller. The X360 asm
// writes exactly two constants: a byte at +0x00 (= 6) and a halfword at +0x02
// (= 3750); +0x01 is the alignment gap between them. Their precise XAudio
// semantics are not recovered (6 looks like a channel/format count; 3750, ~78 ms
// of 48 kHz samples, like a reverb tail/latency), so the fields are modelled as
// the two attested words with neutral, width-faithful names. FLAG: only these
// two fields are attested by this TU.
struct SReverbEffectInfo
{
    u8  muInfoByte0; // +0x00  written = 6      (`stb 6, 0(out)`)
    u8  mPad01;      // +0x01  alignment gap (untouched by the asm)
    u16 muInfoWord2; // +0x02  written = 3750   (`sth 0xEA6, 2(out)`)
};

class CReverbEffect
{
public:
    // @ 0x8295F410 -- report the effect's fixed info block. Writes the two baked
    // constants into `*apInfo` and returns 0 (S_OK). Touches no object state.
    s32 GetInfo(SReverbEffectInfo* apInfo);
};

} // namespace XAUDIO
