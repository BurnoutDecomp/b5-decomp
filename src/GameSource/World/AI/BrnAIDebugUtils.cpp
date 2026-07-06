// ============================================================================
// b5-decomp/src/GameSource/World/AI/BrnAIDebugUtils.cpp
//
// BrnAI::BrnAIDebugUtils -- AI-section debug-draw helpers (the "Main AI" overlay's
// world-space visualisers). Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
// This batch bodies the three verified functions:
//   DrawBoundryLine        0x82767608
//   DrawBoundryLineWithY   0x827674F8
//   DrawSectionHNGGeometry 0x827745C8
//
// DrawAllSectionData (0x8277F8C8) and DrawPortalGeometry (0x827744C0) are DEFERRED --
// their proposed bodies were not store-for-store faithful in this dossier (dropped the
// third bool arg / duplicated the inlined bounds-assert / fabricated a palette-entry
// name) -- so they are left declared-only in the header for their callers.
// ============================================================================

#include "GameSource/World/AI/BrnAIDebugUtils.h"

#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/BrnAIBoundaryLine.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "BrnCommonTypes.h"   // Vector3

namespace BrnAI
{
namespace BrnAIDebugUtils
{
    // ---- Module-global highlight controls (DWARF BrnAIDebugUtils.cpp:28-29). ----
    u32  _guLineToHighlight  = 0;     // dword_8300D538
    bool _gbHighlightOneLine = false; // byte_8300D53C

    // ---- Mutable module colour / counter words (read as data, not immediates). ----
    // Actual RGBA8 values live in the shipped .data and are not present in the asm
    // immediates, so they are defined here as zero-initialised words (value NOT
    // recoverable from this TU's asm; behaviourally the code only bit-twiddles them).
    // Modelled as raw u32 colour words -- the X360 reads/writes them with plain
    // lwz/stw + oris/clrlwi bit-ops, i.e. as the packed RGBA8 word (rw::RGBA::m_rgba),
    // not as a struct; the word is wrapped into an rw::RGBA at the draw call.
    u32  _guHNGColourHighlight = 0; // dword_82F303E0
    u32  _guHNGColourNormal    = 0; // dword_82F303EC
    u32  _guHNGWindowCounter   = 0; // dword_8300DC70

    // Number of HNG lines drawn per frame; the window cycles across the section's
    // mpaNoGoLines list on _guHNGWindowCounter. asm: li r28,0x7D0.
    static const s32 KI_HNG_LINES_PER_FRAME = 2000;

    // Alpha edits applied to the base colour word for the one-line highlight.
    static const u32 KU_HIGHLIGHT_ALPHA_MASK = 0xFF000000u; // oris 0xFF00
    static const u32 KU_HIGHLIGHT_DIM_ALPHA  = 0x46000000u; // oris 0x4600
    static const u32 KU_RGB_MASK             = 0x00FFFFFFu; // clrlwi ..,8

    // BrnAI::BrnAIDebugUtils::DrawBoundryLine @0x82767608.
    //
    // Draw a boundary line as a vertical quad at a single ground height lfY: the two top
    // vertices sit at lfY and the two bottom vertices sit at lfY - KF_BLINE_HALF_HEIGHT.
    // The X360 fills a 2-element top-Y array (both == lfY) and a 2-element bottom-Y array
    // (both == lfY - 40.0) and forwards them to DrawBoundryLineWithY (a4 = top, a5 = bottom).
    void DrawBoundryLine(CgsDev::Debug3DImmediateRender* lpRenderer,
                         const BoundaryLine* lpLine,
                         RGBA lColour,
                         f32 lfY)
    {
        f32 lafTopY[2];
        f32 lafBottomY[2];
        const f32 lfBottomY = lfY - KF_BLINE_HALF_HEIGHT;
        for (u32 lu = 0; lu < 2; ++lu)
        {
            lafTopY[lu]    = lfY;
            lafBottomY[lu] = lfBottomY;
        }
        DrawBoundryLineWithY(lpRenderer, lpLine, lColour, lafTopY, lafBottomY);
    }

    // BrnAI::BrnAIDebugUtils::DrawBoundryLineWithY @0x827674F8.
    //
    // The core boundary-line draw: emit a single vertical quad spanning the boundary
    // line's two 2D endpoints, with a per-endpoint top height (lapfTopY[0..1]) and
    // bottom height (lapfBottomY[0..1]). The line's 2D endpoints map to the world
    // ground plane as (x, z); the Y arrays supply the world height (y). The four quad
    // corners handed to DrawQuad in winding order: top-start, top-end, bottom-end,
    // bottom-start.
    //
    // X360: the line is loaded whole as one 16-byte register (startX, startY, endX,
    // endY) and re-read lane-by-lane while building each corner: for endpoint i (0 =
    // start, 1 = end) the boundary X comes from lane 2*i and the boundary Y (world z)
    // from lane 2*i+1. Each Vector3's w lane is cleared to 0.
    void DrawBoundryLineWithY(CgsDev::Debug3DImmediateRender* lpRenderer,
                              const BoundaryLine* lpLine,
                              RGBA lColour,
                              const f32* lapfTopY,
                              const f32* lapfBottomY)
    {
        // The two boundary-line endpoints as (x, z) ground positions.
        const f32 lafX[2] = { lpLine->mfStartX, lpLine->mfEndX };
        const f32 lafZ[2] = { lpLine->mfStartY, lpLine->mfEndY };

        Vector3 laTop[2];
        Vector3 laBottom[2];
        for (u32 lu = 0; lu < 2; ++lu)
        {
            laTop[lu].x = lafX[lu];
            laTop[lu].y = lapfTopY[lu];
            laTop[lu].z = lafZ[lu];
            laTop[lu].w = 0.0f;

            laBottom[lu].x = lafX[lu];
            laBottom[lu].y = lapfBottomY[lu];
            laBottom[lu].z = lafZ[lu];
            laBottom[lu].w = 0.0f;
        }

        lpRenderer->DrawQuad(laTop[0], laTop[1], laBottom[1], laBottom[0], lColour);
    }

    // BrnAI::BrnAIDebugUtils::DrawSectionHNGGeometry @0x827745C8.
    void DrawSectionHNGGeometry(CgsDev::Debug3DImmediateRender* lpRender,
                                const AISection* lpSection, bool lbHighlightPlanes)
    {
        const s32 liNumHNGLines = lpSection->muNumNoGoLines;   // lhz 0x12(section) -- +18
        if (liNumHNGLines == 0)
        {
            return;
        }

        // Base colour word for this section's HNG lines (raw packed RGBA8; bit-twiddled below).
        u32 luColour = lbHighlightPlanes ? _guHNGColourHighlight : _guHNGColourNormal;

        // Window this frame: ceil(count / perFrame) chunks, cycled on the rolling counter.
        // The X360 signed divisions carry twllei/twlgei div-by-zero / INT_MIN traps; those
        // are compiler-emitted PPC traps with no observable behaviour and are not reproduced.
        const s32 liNumChunks   = (liNumHNGLines + (KI_HNG_LINES_PER_FRAME - 1)) / KI_HNG_LINES_PER_FRAME;
        const u32 luCounter     = _guHNGWindowCounter + 1;
        _guHNGWindowCounter     = luCounter;
        const s32 liWindowStart = KI_HNG_LINES_PER_FRAME * (static_cast<s32>(luCounter) % liNumChunks);

        s32 liWindowCount = KI_HNG_LINES_PER_FRAME;
        if ((liNumHNGLines - liWindowStart) <= KI_HNG_LINES_PER_FRAME)
        {
            liWindowCount = liNumHNGLines - liWindowStart;
        }

        if (liWindowCount <= 0)
        {
            return;
        }

        for (s32 liIndex = 0; liIndex < liWindowCount; ++liIndex)
        {
            const u32 luLine = static_cast<u32>(liIndex + liWindowStart);
            CGS_ASSERT(luLine < lpSection->muNumNoGoLines, "luHNGLineIndex < muNumNoGoLines");

            const BoundaryLine* lpLine = &lpSection->mpaNoGoLines[luLine];   // *(+4) + 16*luLine

            // One-line highlight: the selected loop index becomes opaque, the rest dim. The
            // colour word is mutated in place (matching the X360 -- the mutation carries to
            // later lines).
            if (_gbHighlightOneLine)
            {
                if (static_cast<u32>(liIndex) == _guLineToHighlight)
                {
                    luColour |= KU_HIGHLIGHT_ALPHA_MASK;
                }
                else
                {
                    luColour = (luColour & KU_RGB_MASK) | KU_HIGHLIGHT_DIM_ALPHA;
                }
            }

            // Reference Y for the quad is the section's first portal's Y position. (The X360
            // builds a full (x,y,z,0) vector on the stack and consumes only lane 1 = Y.)
            const Portal* lpPortal = lpSection->GetPortal(0);
            const f32 lfPortalY = lpPortal->GetPositionY();

            // Wrap the packed word into an rw::RGBA (its sole field is the u32 word).
            RGBA lLineColour;
            lLineColour.m_rgba = luColour;
            DrawBoundryLine(lpRender, lpLine, lLineColour, lfPortalY);
        }
    }
}
}
