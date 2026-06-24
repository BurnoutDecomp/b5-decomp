#ifndef BRN_FLAPT_FILE_H
#define BRN_FLAPT_FILE_H

#include "types.hpp"

// ============================================================================
// SharedClasses/Gui/Flapt/BrnFlaptFile.h
//
// The serialised "Flapt" (Flash-derived) GUI resource: BrnFlapt::FlaptFile and
// the per-clip BrnFlapt::MovieClip sub-structure it owns. Reconstructed from the
// BURNOUT_X360_ARTIST.XEX. The X360-baked file/line cites in the asserts name
// "..\\..\\..\\SharedClasses\\Gui/Flapt/BrnFlaptFile.h", which fixes the home of
// MovieClip (and the FlaptFile container) here.
//
// Only the members proven by the attested method bodies are named at fixed
// offsets; the surrounding interior bytes are honestly opaque padding sized only
// to land the named fields at their asm-attested displacements. Growing this
// header additively (appending more named members in their padding regions) is
// the supported way to flesh the type out as further TUs land.
// ============================================================================

namespace BrnFlapt
{
    // A keyframe label entry inside a MovieClip's label table. The attested
    // FindLabelledFrameIndex loop advances the table pointer by 8 bytes per entry
    // (`r31 += 8`) and compares the entry's FIRST dword against the wanted frame
    // id (`lwz r11, 0(r31)`); the trailing dword is not touched by that method, so
    // it is modeled opaque.
    struct MovieClipLabel
    {
        u32 muLabelId;     // +0x00  compared against the wanted label/frame id
        u32 muOpaque04;    // +0x04  (not referenced by the attested methods)
    };

    // BrnFlapt::MovieClip — one timeline within a FlaptFile.
    //
    // Attested offsets (X360 ARTIST):
    //   +0x05  mu8NumLabels          (lbz 5(this); FindLabelledFrameIndex loop bound)
    //   +0x08  mu16NumFrames         (lhz 8(this);  GetKeyframeForFrame bound check)
    //   +0x10  mpau16KeyframeRemap   (lwz 0x10(this); u16[] keyframe-remap, lhzx)
    //   +0x38  mpaLabels             (lwz 0x38(this); MovieClipLabel[])
    //   +0x3C  mpau16LabelledFrameIds(lwz 0x3C(this); u16[] frame ids, lhzx)
    struct MovieClip
    {
        // GetKeyframeForFrame @ 0x8246B098 : map a logical frame index to its
        // backing keyframe index via the optional remap table.
        u32 GetKeyframeForFrame(u32 luFrame) const;

        // FindLabelledFrameIndex @ 0x8246B190 : linear-search the label table for
        // luLabelId; return the labelled frame id, or -1 (and assert) if absent.
        // lpcLabelText is the human-readable label, used only for the failure msg.
        s32 FindLabelledFrameIndex(u32 luLabelId, const char* lpcLabelText) const;

        u8  mau8Opaque00[5];          // +0x00..0x04  (interior not attested here)
        u8  mu8NumLabels;             // +0x05
        u8  mau8Opaque06[2];          // +0x06..0x07  pad to +0x08
        u16 mu16NumFrames;            // +0x08
        u8  mau8Opaque0A[6];          // +0x0A..0x0F  pad to +0x10
        u16* mpau16KeyframeRemap;     // +0x10  (0 => identity mapping)
        u8  mau8Opaque14[0x24];       // +0x14..0x37  pad to +0x38
        MovieClipLabel* mpaLabels;    // +0x38
        u16* mpau16LabelledFrameIds;  // +0x3C
    };

    // BrnFlapt::FlaptFile — the serialised GUI resource container. Declared as a
    // complete (opaque-bodied) type so CgsResource::ResourcePtr<FlaptFile> can be
    // instantiated. No member of FlaptFile is touched by any TU in this group
    // (the ResourcePtr accessor only static_casts the base resource pointer), so
    // the interior is intentionally left unspecified here; grow it additively as
    // FlaptFile's own member TUs (SetSpecialTexture / FixUp / FixDown) land.
    struct FlaptFile;
}

#endif // BRN_FLAPT_FILE_H
