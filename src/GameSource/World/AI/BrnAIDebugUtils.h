#pragma once

// BrnAI::BrnAIDebugUtils -- free-function namespace of AI-section debug-draw helpers
// (the in-game "Main AI" overlay's world-space visualisers for sections, portals and
// boundary lines). DWARF home: GameSource/World/AI/BrnAIDebugUtils.{h,cpp} (namespace
// BrnAI::BrnAIDebugUtils, BrnAIDebugUtils.h:32). These are plain namespace-scope
// functions, NOT class methods.
//
// Signatures are DWARF-authoritative (BrnAIDebugUtils.cpp:61/88/485/520). NOTE: the
// DWARF spells DrawPortalGeometry as a THREE-arg function taking a trailing bool; the
// X360 body ignores it but the caller (DrawAllSectionData @0x8277F8C8) sets up r5=a3
// before the branch, so the bool is really passed. Struct types (AISection, Portal,
// BoundaryLine) are already homed: AISection in SharedClasses/AI/AISectionsResourceType.h,
// Portal in BrnAIPortal.h, BoundaryLine in BrnAIBoundaryLine.h.
//
// The TU bodies all five helpers: DrawBoundryLine (0x82767608), DrawBoundryLineWithY
// (0x827674F8), DrawSectionHNGGeometry (0x827745C8), DrawPortalGeometry (0x827744C0)
// and DrawAllSectionData (0x8277F8C8).

#include "types.hpp"
#include "rw/rwcore_structs.h"                             // rw::RGBA
#include "SharedClasses/AI/AISectionsResourceType.h"       // BrnAI::AISection
#include "GameSource/World/AI/BrnAIPortal.h"               // BrnAI::Portal
#include "GameSource/World/AI/BrnAIBoundaryLine.h"         // BrnAI::BoundaryLine

namespace CgsDev { struct Debug3DImmediateRender; }

namespace BrnAI
{
    // Packed debug-render colours (AARRGGBB, the li/lis/ori immediates the AI debug components
    // pass to the 2D/3D renderers). Shared by the RaceBalancing and ResetOnTrack debug components,
    // which ARTIST compiles in one unity TU (BrnWorldAIUnity), so they are defined once here.
    const u32 K_WHITE = 0xFFFFFFFFu;
    const u32 K_GREEN = 0xFF00FF00u;
    const u32 K_BLUE  = 0xFF0000FFu;

    namespace BrnAIDebugUtils
    {
        using rw::RGBA;

        // @0x8277F8C8 -- draw all debug geometry for one AI section: the HNG (hard-
        // no-go) lines then every portal's boundary lines. The bool is forwarded to
        // DrawSectionHNGGeometry and DrawPortalGeometry.
        void DrawAllSectionData(CgsDev::Debug3DImmediateRender* lpRenderer,
                                const AISection* lpSection,
                                bool lbDebugFlag);

        // @0x827744C0 -- draw one portal's boundary lines as vertical quads at the
        // portal's Y height. Third bool arg is part of the DWARF signature but unused
        // by the body.
        void DrawPortalGeometry(CgsDev::Debug3DImmediateRender* lpRenderer,
                                const Portal* lpPortal,
                                bool lbDebugFlag);

        // @0x82767608 -- draw a boundary line as a vertical quad at a single ground
        // height lfY (bottom edge = lfY - KF_BLINE_HALF_HEIGHT).
        void DrawBoundryLine(CgsDev::Debug3DImmediateRender* lpRenderer,
                             const BoundaryLine* lpLine,
                             RGBA lColour,
                             f32 lfY);

        // @0x827674F8 -- draw a boundary line as a vertical quad with per-endpoint top
        // (lapfTopY[0..1]) and bottom (lapfBottomY[0..1]) heights.
        void DrawBoundryLineWithY(CgsDev::Debug3DImmediateRender* lpRenderer,
                                  const BoundaryLine* lpLine,
                                  RGBA lColour,
                                  const f32* lapfTopY,
                                  const f32* lapfBottomY);

        // @0x827745C8 -- draw the section's HNG (hard-no-go) boundary lines, windowed
        // KI_HNG_LINES_PER_FRAME at a time, with the optional one-line highlight.
        void DrawSectionHNGGeometry(CgsDev::Debug3DImmediateRender* lpRenderer,
                                    const AISection* lpSection,
                                    bool lbDebugFlag);
    }
}
