#ifndef BRN_MAIN_MAP_H
#define BRN_MAIN_MAP_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector2 / Vector4 (rw::math::vpu)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent (base)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"           // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameSource/Gui/SatNav/BrnMapManager.h"              // BrnGui::MapManager (embedded by value)

// BrnMainMap.h
// BrnGui::MainMapComponent - the sat-nav "main map" GUI screen component. It owns a
// BrnGui::MapManager (the tile working set), drives the animated world-rect / zoom of the
// map view, and outputs a GuiEventRenderMainMap each frame for the custom map renderer.
//
// X360 authority (BURNOUT_X360_ARTIST.XEX):
//   Construct                  @ 0x8245E228
//   RecvEvent                  @ 0x82458370
//   Update                     @ 0x824696E8
//   SnapToLocation             @ 0x8245EBA0
//   ApplyZoom                  @ 0x8245EE78
//   SetZoom                    @ 0x82469A38
//   SetStandardDefZoomParams   @ 0x82447ED8
// DWARF: references/DecFIGS/dwarfdump/GameSource/Gui/SatNav/BrnMainMap.h
//
// LAYOUT NOTE: the X360 `this` embeds MapManager at +0x8C (4-byte-pointer ABI). Members are
// reached BY NAME (semantic parity, not byte offsets) so the PC x64 widths differ from the
// documented X360 offsets. Only the members reached by the reconstructed bodies are declared
// here; the animated world-rect / zoom instance state is added as the Construct / Update /
// SnapToLocation / ApplyZoom / SetZoom bodies are reconstructed (those TUs are still todo:
// they depend on the still-unrecovered sub_8245A080 vector helper, the CalculateViewPaddingOffset
// / CalculatePositionedWorldRect methods, and the OutputViewState<> / GuiAudioTriggerEvent /
// MapTransform collaborators).

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
        // @0x82464F?? (Prepare/Release are separate todo funcs of this class)
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
        void    SetDesiredWorldCentre(Vector2 lv2Centre);
        bool    IsZooming() const;
        void    SetStickMapToScreenEdges(bool lbTop, bool lbBottom, bool lbLeft, bool lbRight);

    private:
        Vector2 CalculateOffsetWorldCentre(Vector2 lv2Centre, OffsetPadding lePadding);
        Vector2 CalculateViewPaddingOffset();
        Vector2 CalculatePositionedWorldRect(Vector2 lv2Centre);
        Vector2 ApplyZoom(Vector2 lv2WorldCentre, float lfZoom);
        void    SetZoomLevel(float lfZoom);
        // @0x82447ED8 -- reset the active zoom-scale table to the compile-time defaults.
        void    SetStandardDefZoomParams();

        // +0x8C -- the tile working set (embedded by value; MainMapComponent pokes its
        // map-active flag directly on the cache-ready event, see RecvEvent).
        MapManager mMapManager;
    };
}

#endif // BRN_MAIN_MAP_H
