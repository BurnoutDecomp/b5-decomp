#include "GameSource/Sound/Global/BrnPresentationEffect.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::PresentationEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PresentationEffect is a multiple-inheritance effect-object leaf: BrnEffectObject dual
// base (@ +0/+4) + Streaming::IStreamUser (@ +0x38). See BrnPresentationEffect.h for the
// triple-vptr layout rationale, the maVoices[4] AgingVoice array, and the
// deferred-member FLAGs.
//
// This TU's SHIPPED function set:
//   PresentationEffect()                        @ 0x826E7628  (ctor)
//   FindFreeVoice()                             @ 0x82687D68  (DWARF cpp:554)
//   FindOrStealAVoice(const PresentationEntry&) @ 0x826D2AD8  (DWARF cpp:485)
//   `vector deleting destructor'                @ 0x826E78A8  (compiler-synthesised)
//   `vector deleting destructor'`adjustor{4}'   @ 0x826E7728  (compiler-synthesised)
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

// ---------------------------------------------------------------------------
// PresentationEffect::FindFreeVoice()  @ 0x82687D68
//   Return the first FREE maVoices[] slot, else null. A slot's per-VoiceWrapper
//   state word (slot+0x4C) reads 0 (unused) or 7 (idle) when available.
//   asm: v1=0; for(cur=this+0x8C;;cur+=0x80){ if(*cur==7||*cur==0) return
//        this+0x40+(v1<<7); if(++v1>=4) return 0; }
// FLAG (raw slot+0x4C state word): the availability word lives inside the slot's
// embedded VoiceWrapper and is read store-for-store through a u8* cursor at its
// attested byte offset (rule #4), NOT via a fabricated field name.
// ---------------------------------------------------------------------------
PresentationEffect::AgingVoice* PresentationEffect::FindFreeVoice()
{
    u8* lpThis = reinterpret_cast<u8*>(this);

    s32 liSlot = 0;                                       // r10
    for (u8* lpState = lpThis + 0x8C; ; lpState += 0x80)  // r9: slot+0x4C, stride 0x80
    {
        const u32 luState = *reinterpret_cast<const u32*>(lpState);
        if (luState == 7 || luState == 0)
            return reinterpret_cast<AgingVoice*>(lpThis + 0x40 + (liSlot << 7));
        if (++liSlot >= 4)
            return 0;
    }
}

// ---------------------------------------------------------------------------
// PresentationEffect::FindOrStealAVoice(rEntry)  @ 0x826D2AD8
//   Pick the AgingVoice slot to (re)use for the new presentation entry rEntry:
//     1. If a busy STREAM request (query mu8Behaviour == 2) content-matches the
//        stored entry field-for-field, the exact stream is already playing ->
//        return 0 (do NOT allocate a voice). [asm: match -> break -> result = 0]
//     2. Track a steal candidate: nonzero choke-group -> if it equals the query
//        choke-group Release() its voice + prefer; if it is LOWER, prefer;
//        choke-group 0 -> prefer the OLDEST (max mu16Age).
//     3. After the 4-slot walk: prefer FindFreeVoice()'s free slot; else the
//        tracked steal candidate (Release its voice first); else 0.
//
//   X360 walks cur=this+0x8C (maVoices[0] slot+0x4C) in 0x80 strides. The stored
//   PresentationEntry sits at slot+0x58; its compared fields line up with the query
//   entry at +0x10 (mContentSpec leading word), +0x14 (mu16Splice), +0x16
//   (mu8ChokeGroup), +0x17 (mu8Valid), +0x18 (mu8Behaviour), +0x19 (mu8MixerOutput).
//   mu16Age is slot+0x00; the embedded VoiceWrapper (Release target) is slot+0x04.
//   The steal branch keys off the STORED entry's mu8ChokeGroup at slot+0x6E.
//
// FLAG (raw byte offsets): the per-slot state word (slot+0x4C) and the stored
// PresentationEntry live inside the embedded AgingVoice/VoiceWrapper span, so they
// are read store-for-store through a u8* cursor at their attested byte offsets
// (rule #4); the DWARF field names are recorded in comments, nothing is fabricated.
// ---------------------------------------------------------------------------
PresentationEffect::AgingVoice* PresentationEffect::FindOrStealAVoice(const PresentationEntry& rEntry)
{
    typedef CgsSound::Logic::VoiceWrapper VoiceWrapper;

    u8*       lpThis  = reinterpret_cast<u8*>(this);
    const u8* lpQuery = reinterpret_cast<const u8*>(&rEntry);

    AgingVoice* lpFree = FindFreeVoice();                 // r25

    s32 liCandidate = -1;                                 // r28: best steal-slot index
    u32 luBestAge   = 0;                                  // r27: oldest mu16Age (choke 0)
    u32 luSlot      = 0;                                  // r29: current slot index
    u8* lpState     = lpThis + 0x8C;                      // r31: maVoices[luSlot] slot+0x4C

    while (true)
    {
        const u32  luState = *reinterpret_cast<const u32*>(lpState);   // slot+0x4C
        const bool lbBusy  = !(luState == 7 || luState == 0);

        // A busy STREAM request whose stored entry content-matches rEntry means the
        // exact stream is already playing -> the X360 breaks the walk and returns 0.
        if (lbBusy && lpQuery[0x18] == 2)                 // query mu8Behaviour == E_PRESENTATION_CONTENT_STREAM
        {
            const bool lbMatch =
                   *reinterpret_cast<const u32*>(lpQuery + 0x10) == *reinterpret_cast<const u32*>(lpState + 0x1C)   // mContentSpec  (slot+0x68)
                && *reinterpret_cast<const u16*>(lpQuery + 0x14) == *reinterpret_cast<const u16*>(lpState + 0x20)   // mu16Splice    (slot+0x6C)
                && lpQuery[0x16] == lpState[0x22]         // mu8ChokeGroup (slot+0x6E)
                && lpQuery[0x17] == lpState[0x23]         // mu8Valid      (slot+0x6F)
                && lpState[0x24] == 2                     // stored mu8Behaviour (slot+0x70)
                && lpQuery[0x19] == lpState[0x25];        // mu8MixerOutput(slot+0x71)
            if (lbMatch)
                return 0;                                 // asm: break -> result = 0
        }

        // Steal-candidate selection, keyed on the STORED entry's choke group.
        const u8 lu8ChokeGroup = lpState[0x22];           // stored mu8ChokeGroup (slot+0x6E)
        if (lu8ChokeGroup != 0)
        {
            const u8 lu8QueryChoke = lpQuery[0x16];       // query mu8ChokeGroup
            if (lu8ChokeGroup == lu8QueryChoke)
            {
                reinterpret_cast<VoiceWrapper*>(lpState - 0x48)->Release();   // maVoices[i].mVoice (slot+0x04)
                liCandidate = static_cast<s32>(luSlot);
            }
            else if (lu8ChokeGroup < lu8QueryChoke)
            {
                liCandidate = static_cast<s32>(luSlot);
            }
        }
        else
        {
            const u16 lu16Age = *reinterpret_cast<const u16*>(lpState - 0x4C);   // mu16Age (slot+0x00)
            if (lu16Age > luBestAge)
            {
                luBestAge   = lu16Age;
                liCandidate = static_cast<s32>(luSlot);
            }
        }

        ++luSlot;
        lpState += 0x80;
        if (luSlot >= 4)
        {
            if (lpFree != 0)
                return lpFree;
            if (liCandidate >= 0)
            {
                CGS_ASSERT(liCandidate < 4, "liCandidate < E_POLYPHONY");
                u8* lpSlot = lpThis + (liCandidate << 7) + 0x40;   // &maVoices[liCandidate]
                reinterpret_cast<VoiceWrapper*>(lpSlot + 0x04)->Release();
                return reinterpret_cast<AgingVoice*>(lpSlot);
            }
            return 0;
        }
    }
}

} // namespace Logic
} // namespace BrnSound
