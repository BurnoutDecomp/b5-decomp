#include "GameSource/Sound/Global/BrnPresentationEffect.h"

// =============================================================================
// BrnSound::Logic::PresentationEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PresentationEffect is a multiple-inheritance effect-object leaf: BrnEffectObject dual
// base (@ +0/+4) + Streaming::IStreamUser (@ +0x38). See BrnPresentationEffect.h for the
// triple-vptr layout rationale, the maVoices[4] AgingVoice array, and the
// deferred-member / blocked-function FLAGs.
//
// This TU's SHIPPED function set:
//   PresentationEffect()                      @ 0x826E7628  (ctor)
//   `vector deleting destructor'              @ 0x826E78A8  (compiler-synthesised)
//   `vector deleting destructor'`adjustor{4}' @ 0x826E7728  (compiler-synthesised)
// FindFree() @ 0x82687D68 and FindOrStealAVoice() @ 0x826D2AD8 are BLOCKED (see header).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// PresentationEffect::PresentationEffect()  @ 0x826E7628  (MINIMAL -- see FLAGs)
//
// X360 STORE ORDER (0x826E7628-0x826E7720):
//   stfs 0.0f, 0x20 ; stfs 0.0f, 0x1C ; sth 0,0x10 ; stw 0,0xC ; stw 0,0x34 ; stw 0,8
//   stb 0,0x30 ; stw 0,0x28 ; stw 0,0x24 ; sth 0,0x12       ; leaf scalars (un-homed)
//   stw off_820AE954,4 ; stw off_820ABB88,0x38               ; (transient base vptrs)
//   stw off_820B5F44,0 ; stw off_820B5F10,4 ; stw off_820B5F04,0x38 ; (final 3 vptrs)
//   for (i = 3; i >= 0; --i) over v2 = this+0x40, v2 += 0x80: ; maVoices[4]
//       sth 0, 0(v2)                            ; AgingVoice::mu16Age = 0  @slot+0x00
//       CgsSound::Logic::VoiceWrapper::VoiceWrapper(v2 + 4);   ; mVoice ctor @slot+0x04
//   stw 0, 0x25C..0x284 ; stw -1, 0x288         ; leaf words (un-homed)
//   Attrib::Gen::presentationactionlist::presentationactionlist(this+0x28C, 0, 0);
//   stw 0, 0x29C ; stw 0, 0x2A0                 ; leaf words (un-homed)
//   return this
//
// MSVC's INLINED full-object constructor: it does NOT `bl` a base ctor -- it inlines the
// base member zero-inits and installs THREE leaf vptrs directly (this+0 primary + this+4
// IResourceRequester sub-object == the committed BrnEffectObject dual base; this+0x38 ==
// the IStreamUser interface sub-object). In reconstructed C++ those three vptr installs +
// base member zero-inits are produced structurally by the two base sub-objects' own
// default constructors (BrnEffectObject BY NAME + IStreamUser BY NAME). The only
// hand-written tail effect is the attested maVoices[] AgingVoice array (each slot zeroes
// mu16Age and default-constructs its embedded VoiceWrapper).
//
// FLAG (un-homed leaf members): the inlined leaf scalar zero-inits (+0x08..+0x34) and the
// words in the +0x25C..+0x2A0 span (all zero except the single -1 @ +0x288) target
// PresentationEffect's OWN un-homed leaf members (mau8DataOffsets/mau8DataEnds/
// mStreamParams/mMode/mu8StreamOutput). They are DECLARATION-ONLY / DEFERRED per the
// anti-fabrication rule -- NOT re-emitted as raw-offset stores or invented fields.
//
// FLAG (Attrib::Gen::presentationactionlist mActions DEFERRED): the tail
// `presentationactionlist(this+0x28C, 0, 0)` is an AttribSys-generated table ctor with
// the SAME (ptr,0,0) shape as the committed StreamingEffect::streamsettings sibling; no
// generated-class home exists for it, so it is DEFERRED (not materialised, not fabricated).
// ---------------------------------------------------------------------------
PresentationEffect::PresentationEffect()
    : BrnEffectObject()                       // primary vptr @+0 + IResourceRequester vptr @+4, base zero-init (BY NAME)
    , BrnSound::Logic::Streaming::IStreamUser() // IStreamUser interface vptr @+0x38 (BY NAME)
    , maVoices()                              // 4x { mu16Age = 0 ; VoiceWrapper ctor } -- this+0x40, stride 0x80
{
    // The inlined leaf scalar zero-inits (mau8DataOffsets/mau8DataEnds/mStreamParams/
    // mMode/mu8StreamOutput region) and the Attrib::Gen::presentationactionlist mActions
    // tail construction are DECLARATION-ONLY / DEFERRED (see FLAGs); NOT fabricated here.
}

// ---------------------------------------------------------------------------
// ~PresentationEffect  (the out-of-line anchor the vector deleting destructor
// @ 0x826E78A8 and its adjustor{4} @ 0x826E7728 forward to).
//
//   0x826E78A8 vector deleting destructor:
//     bl ~PresentationEffect ; if (a2 & 1) { free via off_82FFB954 (slot +0x14) } ; return this
//   0x826E7728 adjustor{4}: addi r3,r3,-4 ; b <vector deleting destructor>
//     -> recovers the primary PresentationEffect `this` from the +4 IResourceRequester
//        sub-object pointer, then tail-forwards to the vector deleting destructor.
//
// Both are MSVC compiler-synthesised thunks over this virtual destructor. All observable
// member teardown lives in the committed BrnEffectObject / IStreamUser base chains + the
// leaf members (maVoices), so this anchor body is empty; the host toolchain re-synthesises
// the vector deleting destructor + the +4 MI adjustor from this class's MI base list +
// virtual dtor. The (a2 & 1) tail frees the object through the global sound MemBase
// allocator (off_82FFB954); that allocator is not homed here, so the `delete` half is
// left to the host operator delete (no fabricated allocator).
// ---------------------------------------------------------------------------
PresentationEffect::~PresentationEffect()
{
}

} // namespace Logic
} // namespace BrnSound
