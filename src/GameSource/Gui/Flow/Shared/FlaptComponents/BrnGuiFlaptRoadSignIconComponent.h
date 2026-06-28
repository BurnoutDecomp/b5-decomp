#ifndef BRN_GUI_FLAPT_ROAD_SIGN_ICON_COMPONENT_H
#define BRN_GUI_FLAPT_ROAD_SIGN_ICON_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector2, Vector4, CgsID
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"   // BrnFlapt::MovieClipRef (embedded by value)
#include "GameSource/Gui/BrnGuiShared.h"                 // BrnGui::ERoadIcon

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptRoadSignIconComponent.h
//
// BrnGui::FlaptRoadSignIconComponent -- an apt-driven sat-nav road sign. It drives
// one apt movie clip (mAptRef) holding the road-sign artwork plus a second clip
// (mPostMovieClip, the "SignPole") and exposes Display*/SetColour entry points that
// pick the sign label by road id / icon name and tint the sign text. Derives (in the
// PS3 DWARF) from BrnFlaptComponent. Reconstructed from BURNOUT_X360_ARTIST.XEX;
// member names/types/enums pinned from the DecFIGS DWARF
// (BrnGuiFlaptRoadSignIconComponent.h), gated on the X360 ledger.
//
// LAYOUT NOTE (X360-attested offsets, from the Construct / Prepare / DisplayRoad /
// SetColour asm). The DWARF declares this as
//   struct FlaptRoadSignIconComponent : public BrnFlaptComponent { vptr; ... }
// but -- exactly like the sibling FlaptTimerFieldComponent -- the X360 build inserts a
// 12-byte gap between the leading vptr (+0x00) and the BrnFlaptComponent base subobject,
// which the PS3 DWARF does not show. Per the layout-recovery rule we trust the X360 asm
// offsets and model the class FLAT (named members at their attested displacements, with
// explicit padding for the gap) rather than via C++ inheritance, which would misplace
// the base at +0x04. All member access is BY NAME; there are no raw-offset hacks.
//
//   +0x00  vptr + 12-byte X360 lead gap (opaque)
//   +0x10  mpStateInterface  (stw SI, 0x10(this))          -- BrnFlaptComponent base
//   +0x14  mAptRef           (MovieClipRef; the road-sign clip)
//   +0x20  mv2WorldPos       (Vector2; zeroed by the vector store at +0x20)
//   +0x30  mRoadId           (CgsID / u64)
//   +0x38  meIconType        (EIconType; Construct installs E_ICON_TYPE_COUNT == 7)
//   +0x3C  meSignColour      (ESignColour)
//   +0x40  mbTimeRuled       (bool)
//   +0x41  mbCrashRuled      (bool)
//   +0x42  mbShowPost        (bool)
//   +0x43  mbSignVisible     (bool; DisplayRoad sets it once the sign clip is shown)
//   +0x44  mPostMovieClip    (MovieClipRef; the "SignPole" child clip)
//   sizeof 0x4C
// ============================================================================

namespace CgsGui
{
    struct StateInterface;   // stored by-pointer only (see BrnGuiFlaptComponent.h)
}

namespace BrnFlapt
{
    struct FileRef;          // by const-reference in Prepare; full type via the .cpp
}

namespace BrnGui
{
    struct FlaptRoadSignIconComponent
    {
        // BrnGuiFlaptRoadSignIconComponent.h:52 (DWARF) -- which of the seven sign
        // shapes/layouts the sign uses. E_ICON_TYPE_COUNT is the "unset" sentinel
        // Construct installs.
        enum EIconType
        {
            E_ICON_TYPE_A     = 0,
            E_ICON_TYPE_B     = 1,
            E_ICON_TYPE_C     = 2,
            E_ICON_TYPE_D     = 3,
            E_ICON_TYPE_E     = 4,
            E_ICON_TYPE_F     = 5,
            E_ICON_TYPE_G     = 6,
            E_ICON_TYPE_COUNT = 7,
        };

        // BrnGuiFlaptRoadSignIconComponent.h:65 (DWARF) -- the sign's colour scheme,
        // selecting both the "bg" clip's colour label and the road-text tint.
        enum ESignColour
        {
            E_SIGN_COLOUR_GREEN  = 0,
            E_SIGN_COLOUR_RED    = 1,
            E_SIGN_COLOUR_SILVER = 2,
            E_SIGN_COLOUR_GOLD   = 3,
            E_SIGN_COLOUR_COUNT  = 4,
        };

        // ---- bodied in BrnGuiFlaptRoadSignIconComponent.cpp ----------------------

        // Construct @ 0x8241CB68 -- bind the state interface, clear both clip handles,
        // zero the world position / road id, default the icon type to the unset
        // sentinel and the colour/flags to 0. DWARF shape
        // Construct(const void*, StateInterface*, const void*); the body uses only the
        // state interface (r5).
        virtual void Construct(const void* lpDEBUGName,
                               CgsGui::StateInterface* lpStateInterface,
                               const void* lpcParentName);

        // Prepare @ 0x8241CC18 -- resolve the named component out of lFile, bind+reset
        // its movie clip into mAptRef, then bind the "SignPole" child clip into
        // mPostMovieClip. DWARF shape Prepare(const char*, const FileRef&).
        virtual void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);

        // DisplayRoad @ 0x82427E58 -- play the road clip on the lpcRoadName label, show
        // the post if it exists (lbVisible), and make the sign visible the first time.
        void DisplayRoad(const char* lpcRoadName, bool lbVisible);

        // DisplayRoadFromCgsID @ 0x8242DBD0 -- format the road id (special-cased to the
        // "RD_EXIT" sign for the EXIT road) and route to the matching DisplayRoad.
        void DisplayRoadFromCgsID(CgsID lRoadId, bool lbVisible);

        // FindRoadFromName @ 0x8241CD08 -- linear-search the road-icon name table for
        // lpIconName and return the matching ERoadIcon (asserts on no match).
        ERoadIcon FindRoadFromName(const char* lpIconName);

        // SetColour @ 0x82427F30 -- store the sign colour, then (if the sign clip is
        // bound) recolour the "bg" clip's colour label and tint the "RoadText" clip
        // from the colour tables.
        void SetColour(ESignColour leSignColour);

        // ---- X360-attested siblings, bodied in their own TUs --------------------
        // DisplayRoad(ERoadIcon, bool) @ 0x8242DAE8 -- the icon-id overload that
        // DisplayRoadFromCgsID routes to once it has resolved the icon id; declared
        // here so the call resolves against this single home.
        void DisplayRoad(ERoadIcon leRoadIcon, bool lbVisible);

    private:
        // ---- layout (see LAYOUT NOTE; flat by attested offset) ------------------
        u8                      mauPadHead[0x10];   // +0x00 vptr + X360 lead gap (opaque)
        CgsGui::StateInterface* mpStateInterface;   // +0x10  BrnFlaptComponent base
        BrnFlapt::MovieClipRef  mAptRef;            // +0x14
        u8                      mauPad1C[0x04];     // +0x1C  align Vector2 to +0x20
        Vector2                 mv2WorldPos;        // +0x20
        CgsID                   mRoadId;            // +0x30
        EIconType               meIconType;         // +0x38
        ESignColour             meSignColour;       // +0x3C
        bool                    mbTimeRuled;        // +0x40
        bool                    mbCrashRuled;       // +0x41
        bool                    mbShowPost;         // +0x42
        bool                    mbSignVisible;      // +0x43
        BrnFlapt::MovieClipRef  mPostMovieClip;     // +0x44
    };
}

#endif // BRN_GUI_FLAPT_ROAD_SIGN_ICON_COMPONENT_H
