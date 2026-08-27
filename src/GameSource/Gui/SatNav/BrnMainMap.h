#ifndef BRN_MAIN_MAP_H
#define BRN_MAIN_MAP_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector2 / Vector4 (rw::math::vpu)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent (base)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"           // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameSource/Gui/SatNav/BrnMapManager.h"              // BrnGui::MapManager (embedded by value)

#include <cstddef>                                            // offsetof (uncalled _AssertLayout)

namespace BrnGui { class GuiCache; }
namespace BrnGui { struct CrashNavMap; }  // friend of MainMapComponent (reads mfWorldZoomScaleFactor by name)  // mpGuiCache (pointer only; full type GameSource/Gui/BrnGuiCache.h)

// BrnMainMap.h
// BrnGui::MainMapComponent - the sat-nav "main map" GUI screen component. It owns a
// BrnGui::MapManager (the tile working set), drives the animated world-rect / zoom of the
// map view, and outputs a GuiEventRenderMainMap each frame for the custom map renderer.
//
// X360 authority (BURNOUT_X360_ARTIST.XEX):
//   Construct                  @ 0x8245E228   (bodied, BrnMainMap.cpp)
//   Prepare                    @ 0x8244F4A8   (bodied, BrnMainMap.cpp)
//   RecvEvent                  @ 0x82458370   (bodied, BrnMainMap.cpp)
//   SetStandardDefZoomParams   @ 0x82447ED8   (bodied, BrnMainMap.cpp)
//   Update                     @ 0x824696E8
//   SnapToLocation             @ 0x8245EBA0
//   ApplyZoom                  @ 0x8245EE78
//   SetZoom                    @ 0x82469A38
//   CalculatePositionedWorldRect @ 0x8245E5F0
//   CalculateViewPaddingOffset   @ 0x82447D38
// DWARF: references/DecFIGS/dwarfdump/GameSource/Gui/SatNav/BrnMainMap.h
//
// LAYOUT NOTE: the X360 `this` embeds MapManager at +0x8C (4-byte-pointer ABI). Members are
// reached BY NAME (semantic parity, not byte offsets) so the PC x64 widths differ from the
// documented X360 offsets -- every X360 offset in this file is a COMMENT, never arithmetic.
// The full DWARF instance-member run (BrnMainMap.h:209-232) is declared below. Construct and
// Prepare -- the two that run on the stunt-run fly-by path -- are bodied in BrnMainMap.cpp as
// of 2026-08-27; Update / SnapToLocation / ApplyZoom / SetZoom are still todo (they depend on
// the still-unrecovered sub_8245A080 vector helper, the CalculateViewPaddingOffset /
// CalculatePositionedWorldRect methods, and the OutputViewState<> / GuiAudioTriggerEvent
// collaborators).

namespace BrnGui
{
    // BrnMainMap.h:52 -- the per-frame "render the sat-nav map" GUI event (GuiEvent<223>).
    // Carries the current map + view rects, the flattened active-texture set to draw, the
    // interpolated zoom level, which map surface it targets, and whether the map is live.
    // EVENT ID (X360-attested = 223, corrected from an earlier reconstruction's 221): the
    // OutputViewState<GuiEventRenderMainMap> instantiation @0x82465E50 bakes T::GetEventType()==223
    // (miOutEventType word), and id 221 is independently owned by GuiEventSetBlackBars
    // (AddGuiEvent<GuiEventSetBlackBars> @0x823CEE50 passes AddEvent type 221) -- so this event is 223.
    struct GuiEventRenderMainMap : public CgsGui::GuiEvent<223>
    {
        // BrnMainMap.h:54
        enum EMapType
        {
            E_MAPTYPE_MAINMAP     = 0,
            E_MAPTYPE_PRERACE     = 1,
            E_MAPTYPE_LOADING_MAP = 2,
            E_MAPTYPE_COUNT       = 3,
        };

        Vector4                              mv4MapRect;       // BrnMainMap.h:64
        Vector4                              mv4ViewRect;      // BrnMainMap.h:65
        const MapManager::sActiveTextures*   mpActiveTextures; // BrnMainMap.h:66
        float                                mfZoomLevel;      // BrnMainMap.h:67
        EMapType                             meMapType;        // BrnMainMap.h:68
        bool                                 mbIsActive;       // BrnMainMap.h:69

        void Construct();                                      // BrnMainMap.h:63
    };

    // BrnMainMap.h:84 -- the sat-nav map screen component.
    class MainMapComponent : public CgsGui::GuiComponent
    {
    public:
        // BrnMainMap.h:84
        enum ZoomFactor
        {
            E_ZOOMFACTOR_LOW    = 0,
            E_ZOOMFACTOR_MEDIUM = 1,
            E_ZOOMFACTOR_HIGH   = 2,
            E_ZOOMFACTOR_CUSTOM = 3,
            E_ZOOMFACTOR_COUNT  = 4,
        };

        // BrnMainMap.h:202 -- which way CalculateOffsetWorldCentre pads the view rect.
        enum OffsetPadding
        {
            E_OFFSETPADDING_IN  = 0,
            E_OFFSETPADDING_OUT = 1,
        };

        // BrnMainMap.h:99 -- the construction parameters the owning state hands the component.
        struct MainMapParameterBundle
        {
            Vector4                             mv4ViewRect;    // BrnMainMap.h:102
            Vector4                             mv4PaddingRect; // BrnMainMap.h:103
            GuiEventRenderMainMap::EMapType     meMapType;      // BrnMainMap.h:104
        };

        // @0x8245E228
        void    Construct(CgsGui::StateInterface* lpStateInterface,
                          MainMapParameterBundle* lpParameters);
        // @0x8244F4A8 -- mark active and park the desired centre on the world-rect centre.
        bool    Prepare();
        bool    Release();
        // @0x824696E8 -- advance the animated map view and emit the render event.
        Vector2 Update(Vector2 lv2WorldCentre);
        // @0x82458370 -- receive a GUI event; latches the map-active flag and forwards to MapManager.
        void    RecvEvent(const CgsModule::Event* lpEvent, int32_t liEventType);
        // @0x8245EBA0 -- immediately centre the map on a location (no animation).
        void    SnapToLocation(Vector2 lv2Location);
        // @0x82469A38 -- select a zoom factor (optionally custom) and optionally apply it now.
        void    SetZoom(ZoomFactor leZoomFactor, float lfCustomZoom, bool lbApplyNow);
        void    IncreaseZoom();
        void    DecreaseZoom();

        // DWARF BrnMainMap.h:379 -- `void SetDesiredWorldCentre(Vector2)`, a HEADER-INLINE
        // method on the console: the dump gives it a BrnMainMap.h decl line (not a .cpp one)
        // and there is no out-of-line address for it in scratch/func_index.tsv, so its
        // faithful home is a body here.
        //
        // It is a straight 16-byte lane store into mv2DesiredCentre. Attested inlined at
        // CrashNavMapMain::HandleCrashNavInputPressed @0x824CCCB0-ish, which does exactly
        // `li r10, 1712 ; lvx128 v0, r0, &unk_82FB4C20 ; stvx128 v0, r31, r10` -- and 1712
        // is state+96 (the embedded component) + 1616 (mv2DesiredCentre). Whole-quadword,
        // so all four lanes of the caller's Vector2 are committed, not just x/y.
        void    SetDesiredWorldCentre(Vector2 lv2DesiredWorldCentre)   // param name: DWARF BrnGuiComponentUnity.cpp:3839
        {
            mv2DesiredCentre = lv2DesiredWorldCentre;
        }

        bool    IsZooming() const;

        // DWARF BrnMainMap.h:415 -- `void SetStickMapToScreenEdges(bool, bool, bool, bool)`,
        // header-inline on the console for the same two reasons as above. Writes the four
        // stick flags, whose offset ORDER is pinned by the _AssertLayout block below and by
        // CalculatePositionedWorldRect @0x8245E5F0, which reads +0x67A/+0x67B/+0x678/+0x679
        // as the left / right / up / down edge tests.
        //
        // The ARGUMENT-to-member mapping IS attested: the DWARF definition rows name the
        // parameters, in order, lbStickUp / lbStickDown / lbStickLeft / lbStickRight
        // (references/DecFIGS/dwarfdump/_compile/BrnGuiComponentUnity.cpp:6751, repeated at
        // BrnGuiOnlineScreenUnity.cpp:1158 and BrnGuiScreenUnity.cpp:3933), which pins each
        // to its like-named member with no call-site evidence needed. (Every inlined call
        // site in the export set happens to pass a uniform value anyway.)
        void    SetStickMapToScreenEdges(bool lbStickUp, bool lbStickDown,
                                         bool lbStickLeft, bool lbStickRight)
        {
            mbStickMapUp    = lbStickUp;
            mbStickMapDown  = lbStickDown;
            mbStickMapLeft  = lbStickLeft;
            mbStickMapRight = lbStickRight;
        }

        // ---- the DWARF accessor block (BrnMainMap.h:297-465) --------------------------
        // Every entry below carries a DWARF decl line in BrnMainMap.h itself, never a
        // BrnMainMap.cpp line (contrast Construct / Update / SnapToLocation / SetZoom /
        // ApplyZoom / SetZoomLevel / CalculateOffsetWorldCentre..., which the same dump
        // gives .cpp lines) -- that is how the dump marks a function defined inline in the
        // header, which is also why none of them has an out-of-line X360 address. Only the
        // declarations are reproduced: the campaign rule is that a body lands with its own
        // TU, and writing "return mv4WorldRect;" here would be a guess about which member
        // each accessor reads. Signatures transcribed verbatim from
        // references/DecFIGS/dwarfdump/GameSource/Gui/SatNav/BrnMainMap.h.
        void       SetMapManager(MapManager* lpMapManager);   // DWARF h:297
        // ⭐ CORRECTED 2026-08-27 (stunt-race UI wave): the Vector4 at .data 0x82FB31F0 that
        // the DWARF models as the instance member mv4WorldRect is
        // `BrnGui::MapTransform::smv4WorldRect` -- already homed and valued at
        // SharedClasses/Gui/SatNav/BrnMapUtils.cpp ({-4375.42, -5842.42, 5363.15, 3904.74}),
        // and declared `const` there. It is READ-ONLY: the previous note here called
        // Prepare @0x8244F4D0 / Construct @0x8245E514 / CalculatePositionedWorldRect
        // @0x8245E61C its WRITERS, but every one of those addresses is the `addi rN, rN,
        // flt_82FB31F0@l` half of an @ha/@l address formation feeding an `lvx`, i.e. a LOAD.
        // Grepping every `st*` to that symbol across the export set returns ZERO writers; the
        // other two sites (CrashNavMap::MoveCursor @0x824BF2F4, RoadSignIconManager::
        // SetupComponent @0x8250AEFC) are reads too. Construct copies it into the member
        // below; Prepare reads the global directly (same value either way).
        // No static member is declared for it here because the DWARF has none; the instance
        // shape below is the attested one.
        Vector4    GetWorldRect() const;                      // DWARF h:314
        Vector4    GetViewRect() const;                       // DWARF h:330
        // DWARF spells this return type fully qualified (`const rw::math::vpu::Vector4&`);
        // `Vector4` is the file-wide typedef of exactly that type (BrnCommonTypes.h:14).
        const Vector4& GetDisplayRect() const;                // DWARF h:346
        ZoomFactor GetZoomLevel() const;                      // DWARF h:362
        // DWARF h:435, return type float32_t. This is the DWARF-supplied name for the
        // float-valued zoom accessor; wave-J CrashNavMap::UpdateIconManager reads that
        // quantity inline at `lfs f13, 0x6C0(state)` == mMainMapComponent+1632, which is
        // mfWorldZoomScaleFactor by the DWARF member order below. Which of the two zoom
        // floats the accessor actually returns is NOT independently attested (the accessor
        // is inlined everywhere and the DWARF carries no body hint) -- callers that need
        // specifically the +1632 quantity should say so at the call site.
        f32        GetZoomFactor() const;                     // DWARF h:435
        void       SetActive(bool lbActive);                  // DWARF h:449
        bool       IsActive() const;                          // DWARF h:465

    private:
        // CrashNavMap::UpdateIconManager @0x824CBAC4 reads mfWorldZoomScaleFactor directly
        // (`lfs f13, 0x6C0(r31)`, and 0x6C0 - mMainMapComponent@+96 == 1632 == the member's
        // X360 offset). The DWARF declares the member private and supplies NO accessor for
        // it, so friendship -- not a fabricated getter -- is the honest exposure, exactly as
        // BrnGuiCache.h and BrnMapIconManager.h do for their own consumer screens.
        friend struct CrashNavMap;

        Vector2 CalculateOffsetWorldCentre(Vector2 lv2Centre, OffsetPadding lePadding);
        Vector2 CalculateViewPaddingOffset();
        Vector2 CalculatePositionedWorldRect(Vector2 lv2Centre);
        Vector2 ApplyZoom(Vector2 lv2WorldCentre, float lfZoom);
        void    SetZoomLevel(float lfZoom);
        // @0x82447ED8 -- reset the active zoom-scale table to the compile-time defaults.
        void    SetStandardDefZoomParams();

        // ---- instance state, verbatim DWARF member order (BrnMainMap.h:209-232) -------
        // The X360 offsets in the trailing comments are DOCUMENTARY ONLY: this is the
        // 32-bit console layout and the host is LLP64, so mpMapManager / mpGuiCache widen
        // and everything after them shifts. Nothing here may be used as arithmetic.
        // The offsets that ARE quoted were measured this wave off the CrashNavMap and
        // PreRaceFlyBy call sites (mMainMapComponent at CrashNavMap+96 / PreRaceFlyBy+0x9A0)
        // and they corroborate this exact ordering, including the mpGuiCache slot between
        // mbIsZooming and the stick flags.

        // +0x8C -- the tile working set (embedded by value; MainMapComponent pokes its
        // map-active flag directly on the cache-ready event, see RecvEvent).
        MapManager  mMapManager;                    // DWARF h:209, X360 comp+0x8C
        MapManager* mpMapManager;                   // DWARF h:210 (SetMapManager's target)

        Vector4 mv4WorldRect;                       // DWARF h:212
        Vector4 mv4ViewRect;                        // DWARF h:213
        Vector4 mv4PaddingRect;                     // DWARF h:214
        Vector4 mv4DisplayRect;                     // DWARF h:215
        // X360 comp+1616: `stvx128 v0, r26, 0xFF0` in PreRaceFlyBy::CalculateZoomFactor
        // (0x9A0 + 0x650) and the CrashNavMap+1712 store are both this lane -- the
        // SetDesiredWorldCentre target.
        Vector2 mv2DesiredCentre;                   // DWARF h:216, X360 comp+1616

        // Two float[4] zoom-scale tables the DWARF places INSIDE the class as private
        // statics (h:218 / h:219). ⭐ DEFINED as of 2026-08-27 in BrnMainMap.cpp, alongside
        // SetStandardDefZoomParams (which copies flt_82F259EC -> flt_82F259DC). The values
        // were never in the IDA export because they are .data, not code: read from the raw
        // image (scratch/postfx_step9_final/envfix/work/image.bin, big-endian, file offset =
        // VA - 0x82000000) -- flt_82F259DC = {6500, 3500, 2500, 0} (the live/HD table) and
        // flt_82F259EC = {5000, 2750, 1000, 0} (the standard-def source). Indexed by
        // ZoomFactor; Construct seeds both zoom scalars from [E_ZOOMFACTOR_HIGH].
        static f32 mfZoomScalFactors[4];            // DWARF h:218
        static f32 mfStandardDefZoomScalFactors[4]; // DWARF h:219

        // X360 comp+1632 -- read inline by CrashNavMap::UpdateIconManager (`lfs f13,
        // 0x6C0(state)` @0x824CBACC, state+96 == the component).
        f32 mfWorldZoomScaleFactor;                 // DWARF h:220, X360 comp+1632
        f32 mfDesiredWorldZoomFactor;               // DWARF h:221
        ZoomFactor                      meCurrentZoomFactor; // DWARF h:222
        GuiEventRenderMainMap::EMapType meMapType;           // DWARF h:223
        bool mbIsZooming;                           // DWARF h:224, X360 comp+1648
        GuiCache* mpGuiCache;                       // DWARF h:225

        bool mbStickMapUp;                          // DWARF h:227, X360 comp+1656
        bool mbStickMapDown;                        // DWARF h:228, X360 comp+1657
        bool mbStickMapLeft;                        // DWARF h:229, X360 comp+1658
        bool mbStickMapRight;                       // DWARF h:230, X360 comp+1659
        bool mbIsActive;                            // DWARF h:232, X360 comp+1660

        // Never called. Pins only the RELATIVE deltas inside the two pointer-free scalar
        // runs -- those hold identically on the 32-bit console and the LLP64 host, which
        // absolute offsets do not.
        static void _AssertLayout()
        {
            // Run 1: the zoom/type/flag block between mv2DesiredCentre and mpGuiCache.
            static_assert(offsetof(MainMapComponent, mfDesiredWorldZoomFactor) -
                          offsetof(MainMapComponent, mfWorldZoomScaleFactor) == 4,
                          "mfDesiredWorldZoomFactor follows mfWorldZoomScaleFactor");
            static_assert(offsetof(MainMapComponent, meCurrentZoomFactor) -
                          offsetof(MainMapComponent, mfWorldZoomScaleFactor) == 8,
                          "meCurrentZoomFactor follows the two zoom floats");
            static_assert(offsetof(MainMapComponent, meMapType) -
                          offsetof(MainMapComponent, mfWorldZoomScaleFactor) == 12,
                          "meMapType follows meCurrentZoomFactor");
            static_assert(offsetof(MainMapComponent, mbIsZooming) -
                          offsetof(MainMapComponent, mfWorldZoomScaleFactor) == 16,
                          "mbIsZooming closes the pointer-free zoom run");

            // Run 2: the five trailing bools after mpGuiCache.
            static_assert(offsetof(MainMapComponent, mbStickMapDown) -
                          offsetof(MainMapComponent, mbStickMapUp) == 1, "stick flag order");
            static_assert(offsetof(MainMapComponent, mbStickMapLeft) -
                          offsetof(MainMapComponent, mbStickMapUp) == 2, "stick flag order");
            static_assert(offsetof(MainMapComponent, mbStickMapRight) -
                          offsetof(MainMapComponent, mbStickMapUp) == 3, "stick flag order");
            static_assert(offsetof(MainMapComponent, mbIsActive) -
                          offsetof(MainMapComponent, mbStickMapUp) == 4,
                          "mbIsActive closes the trailing bool run");
        }
    };
}

#endif // BRN_MAIN_MAP_H
