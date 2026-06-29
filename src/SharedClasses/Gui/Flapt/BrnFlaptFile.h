#ifndef BRN_FLAPT_FILE_H
#define BRN_FLAPT_FILE_H

#include "types.hpp"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"   // CgsUnicode::CgsUtf8 (FlaptFile string table)
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h" // FlaptFile::GuiVertex element type

namespace rw { struct Resource; }   // fix-up base (relocation) — see FlaptFile::FixUp/FixDown
namespace renderengine { class Texture; }   // FlaptFile::GuiTexture == renderengine::Texture (mpapTextures element)

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
    struct TextField;   // forward: MovieClip::mpaTextFields (defined below)
    struct FontStyle;   // forward: FlaptFile::mpaFontStyles (defined below)
    struct FlaptFile;   // forward: MovieClip::mpFile (defined below)

    // A 2D point, standing in for the serialised SmoothStep::Vector2 the DWARF
    // names (BrnFlaptFile.h:195/196). SmoothStep::Vector2 has no reconstructed
    // home; the only use here is the two text-field corner points, accessed as
    // two floats each (TextField+0x10..0x1F in the X360 asm), so a minimal
    // by-value 2-float point reproduces the field set exactly.
    struct Vector2
    {
        f32 mfX;   // +0x00
        f32 mfY;   // +0x04
    };

    // BrnFlapt::HashedString (DWARF BrnFlaptFile.h:70) — a 32-bit name hash plus an
    // optional debug-only literal. Embedded by value as the leading member of
    // TextField (mName), so the field set must be exact to position the rest.
    struct HashedString
    {
        u32         muHash;            // +0x00
        const char* mpacDEBUGString;   // +0x04 (debug builds only; opaque otherwise)
    };

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

    // BrnFlapt::TextField (DWARF BrnFlaptFile.h:165) — one static text field inside a
    // MovieClip. Used by BrnFlapt::TextFieldInstance::SetUpAptStringParams, whose X360
    // asm reads:
    //   +0x08  muInitialStringId  (lhz 8;  indexes FlaptFile::mpapStrings)
    //   +0x0A  muFontStyleIndex   (lbz 0xA; indexes FlaptFile::mpaFontStyles)
    //   +0x0B  mxFlags            (lbz 0xB; bit0 -> bWordWrap, bit1 -> bMultiline)
    //   +0x0C  muAlignment        (lbz 0xC; -> eAlignment / eBoxAlignment)
    //   +0x10  mTopLeft.{x,y}     (lfs 0x10/0x14)
    //   +0x18  mBottomRight.{x,y} (lfs 0x18/0x1C)
    struct TextField
    {
        HashedString mName;              // +0x00 (8 bytes)
        u16          muInitialStringId;  // +0x08
        u8           muFontStyleIndex;   // +0x0A
        u8           mxFlags;            // +0x0B
        u8           muAlignment;        // +0x0C
        Vector2      mTopLeft;           // +0x10 (corner box, top-left)
        Vector2      mBottomRight;       // +0x18 (corner box, bottom-right)
    };

    // BrnFlapt::FontStyle (DWARF BrnFlaptFile.h:377) — a named font + colour + size.
    // SetUpAptStringParams reads all three (FontStyle+0x00/0x04/0x08).
    struct FontStyle
    {
        char* mpacFontName;   // +0x00  -> AptAllocateStringParameters::szFontName
        u32   muColour;       // +0x04  -> AptAllocateStringParameters::nColour
        f32   mfFontHeight;   // +0x08  -> AptAllocateStringParameters::fFontHeight
    };

    // BrnFlapt::Mesh (DWARF BrnFlaptFile.h:218) — one drawable quad/triangle-strip
    // primitive inside a MovieClip's render layer: which texture it uses, plus the
    // span of the owning FlaptFile's shared vertex pool it draws from. Read by
    // FlaptRenderer::RenderMesh / RenderMask in the X360 ARTIST asm:
    //   +0x00  miTextureId  (lbz 0(mesh) + extsb; signed — <0 means "no texture")
    //   +0x01  muNumVerts   (lbz 1(mesh); vertex count, drives the strip length)
    //   +0x02  muVertOffset (lhz 2(mesh); first vertex, index into FlaptFile::mpaVerts)
    struct Mesh
    {
        s8  miTextureId;     // +0x00 (signed: negative => render with no texture)
        u8  muNumVerts;      // +0x01
        u16 muVertOffset;    // +0x02
    };

    // BrnFlapt::MovieClip — one timeline within a FlaptFile.
    //
    // Attested offsets (X360 ARTIST):
    //   +0x03  muNumTextFields       (lbz 3(this); TextFieldInstance::Construct bound)
    //   +0x05  mu8NumLabels          (lbz 5(this); FindLabelledFrameIndex loop bound)
    //   +0x08  muNumFramesInTimeline (lhz 8(this);  GetKeyframeForFrame bound check)
    //   +0x0C  mpFile                (lwz 0xC(this); owning FlaptFile, font/string tables)
    //   +0x10  mpau16KeyframeRemap   (lwz 0x10(this); u16[] keyframe-remap, lhzx)
    //   +0x30  mpaTextFields         (lwz 0x30(this); TextField[], 0x20 stride)
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

        u8  mau8Opaque00[3];          // +0x00..0x02  (interior not attested here)
        u8  muNumTextFields;          // +0x03 (DWARF BrnFlaptFile.h:378)
        u8  mau8Opaque04;             // +0x04
        u8  mu8NumLabels;             // +0x05
        u8  mau8Opaque06[2];          // +0x06..0x07  pad to +0x08
        u16 muNumFramesInTimeline;    // +0x08 (DWARF BrnFlaptFile.h:384)
        u8  mau8Opaque0A[2];          // +0x0A..0x0B  pad to +0x0C
        FlaptFile* mpFile;            // +0x0C (DWARF BrnFlaptFile.h:387)
        u16* mpau16KeyframeRemap;     // +0x10  (0 => identity mapping)
        u8  mau8Opaque14[0x1C];       // +0x14..0x2F  pad to +0x30
        TextField* mpaTextFields;     // +0x30 (DWARF BrnFlaptFile.h:404)
        u8  mau8Opaque34[4];          // +0x34..0x37  pad to +0x38
        MovieClipLabel* mpaLabels;    // +0x38
        u16* mpau16LabelledFrameIds;  // +0x3C
    };

    // BrnFlapt::FlaptFile — the serialised GUI resource container (the on-disk
    // 0x10020 movie file). Reconstructed from BURNOUT_X360_ARTIST.XEX.
    //
    // Attested offsets (X360 ARTIST):
    //   +0x14  muNumTextures        (lwz 0x14; via FlaptRenderer::RenderMesh bound check)
    //   +0x18  mpapTextures         (lwz 0x18; GuiTexture*[], indexed by Mesh::miTextureId)
    //   +0x1C  muNumVerts           (lwz 0x1C; vertex-pool bound)
    //   +0x20  mpaVerts             (lwz 0x20; the shared GuiVertex pool Mesh spans index)
    //   +0x28  mpaFontStyles        (lwz 0x28; FontStyle[], indexed by TextField::muFontStyleIndex)
    //   +0x44  mpapStrings          (lwz 0x44; CgsUtf8*[], indexed by TextField::muInitialStringId)
    //   +0x48  muNumSpecialTextures (lwz 0x48; trailing slots of mpapTextures are "special")
    //
    // Members up to +0x48 are named at their DWARF offsets (BrnFlaptFile.h:549..582);
    // the interior bytes between named members are opaque padding sized to land the
    // tables at their attested displacements. Grow it additively — the heavy
    // FixUp/FixDown/SetSpecialTexture bodies land with FlaptFile's own member TU.
    struct FlaptFile
    {
        // BrnFlaptFile.h:61/62 typedefs: a GUI texture is a renderengine::Texture; a GUI
        // vertex is the screen-space coloured+textured immediate-mode vertex.
        typedef renderengine::Texture                    GuiTexture;
        typedef CgsGraphics::Basic2dColouredTexturedVertex GuiVertex;

        // FixUp @ 0x824712F0 / FixDown @ 0x82470FD8 : (un)relocate the serialised movie
        // image's internal self-references when the resource is loaded / unloaded. The
        // X360 FlaptFileResourceType wrapper tail-calls these on the resource image; the
        // rw::Resource reference supplies the relocation base, matching the CgsResource
        // fix-up family (cf. CgsResource::FontResourceType). Declared here so the wrapper
        // compiles; the bodies are reconstructed with FlaptFile's own member TU.
        void FixUp(const rw::Resource& lrResource);
        void FixDown(const rw::Resource& lrResource);

        // SetSpecialTexture @ 0x8246CF20 (X360): bind a runtime-supplied texture into one of
        // this movie's trailing "special" texture slots, keyed by the texture's logical name
        // (e.g. "CustomComponentTexture.tif"). The custom-render components
        // (BrnNetworkPlayerImageRenderer Prepare/SwapBuffers) call it on the FlaptFile their
        // render-output slot returns, so the GUI mesh that references that special-texture
        // name samples the component's live frame. Returns the FlaptFile (the X360 passes r3
        // through). Body reconstructed with FlaptFile's own member TU. ADDITIVE GROW.
        FlaptFile* SetSpecialTexture(const char* lpcSpecialTextureName);

        u8   mau8Opaque00[0x14];                 // +0x00..0x13  (header + movie-clip table)
        u32  muNumTextures;                      // +0x14 (DWARF BrnFlaptFile.h:563)
        GuiTexture** mpapTextures;               // +0x18 (DWARF BrnFlaptFile.h:564)
        u32  muNumVerts;                         // +0x1C (DWARF BrnFlaptFile.h:566)
        GuiVertex*   mpaVerts;                    // +0x20 (DWARF BrnFlaptFile.h:567)
        u32  muNumFontStyles;                    // +0x24 (DWARF BrnFlaptFile.h:569)
        FontStyle* mpaFontStyles;                // +0x28 (DWARF BrnFlaptFile.h:570)
        u8   mau8Opaque2C[0x18];                 // +0x2C..0x43  pad to +0x44
        const CgsUnicode::CgsUtf8** mpapStrings; // +0x44 (DWARF BrnFlaptFile.h:580)
        u32  muNumSpecialTextures;               // +0x48 (DWARF BrnFlaptFile.h:582)
    };

    // BrnFlaptFile.h:457 (DWARF) -- the per-clip "trigger parameters": up to four
    // name strings the animator/icon clips publish (e.g. the names of the child
    // movie clips an animator drives). The clip iterates this array until a NULL
    // entry. Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 animator code reads
    // mapcParameters[i] as the child-clip name) with the DWARF member name/type.
    struct TriggerParameters
    {
        const char* mapcParameters[4];
    };
}

#endif // BRN_FLAPT_FILE_H
