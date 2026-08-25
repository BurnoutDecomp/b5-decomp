#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"

// BrnGui::RoadSignIconManager - owns the fixed pool of on-map road-sign icon
// components and broadcasts a single "signs visible" toggle across all of them.
// Layout/method reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824F5778 (no prior
// source, no DecFIGS DWARF for this TU).
//
// The manager holds KU_NUM_SIGN_ICONS road-sign icon components inline (observed
// element stride 0xC0 bytes, array starting @this+0xBC, 64 elements) and a master
// visibility flag (@this+0x3105). SetSignsVisible walks the pool: when an icon's
// own visible flag differs from the requested value and that icon has an apt view
// bound, it drives the icon's apt "_visible" view-state to "true"/"false" through
// the component's AddOutputAptViewState, then stores the new per-icon flag. The
// master flag is updated once at the end.
namespace BrnGui
{
    // The road-rule batch-data response GUI event payload (DWARF: the road-sign
    // icon manager takes it by const pointer). Defined in another TU; forward-
    // declared here so the declared-only SetRoadRuleBatchData below can name it.
    struct GuiEventRoadRuleBatchDataResponse;

    // One on-map road-sign icon. Derives from the GUI component base because the
    // X360 build calls CgsGui::GuiComponent::AddOutputAptViewState on the element
    // pointer directly (call site @0x824F57F0, this = element base). The visible /
    // has-apt flags observed @element+0xBC / +0xBD; the bytes the component base
    // and unrelated icon state occupy are left as named reserved storage so the
    // flags land at their observed offsets without any raw-offset casts.
    struct RoadSignIcon : public CgsGui::GuiComponent
    {
        // Component base (vptr + name + hash + state-interface) occupies the first
        // 0xBC bytes; this TU only reads the two flags that follow it.
        u8  maReservedBaseTail[0xBC - sizeof(CgsGui::GuiComponent)];
        // @element+0xBC : per-icon visibility flag (0/1).
        u8  mbVisible;
        // @element+0xBD : 1 if this icon has an apt view bound and may be toggled.
        u8  mbHasAptView;
        // @element+0xBE .. +0xBF (stride padding to 0xC0).
        u8  maReservedTail[0xC0 - 0xBE];
    };

    class RoadSignIconManager
    {
    public:
        static const u32 KU_NUM_SIGN_ICONS = 64;

        // @ 0x824F5778 : broadcast the visible flag across every road-sign icon.
        void SetSignsVisible(u8 lbVisible);

        // [H3b] Feed the map's zoom-derived icon scale (DWARF BrnRoadSignIconManager.h:273
        // mfZoomFactor @this+0x3108 == manager+0xA198 -- the store MapIconManager's
        // SetZoomFactor note pins). X360-inlined at every call site; the setter is the
        // committed exposure over the named member.
        void SetZoomFactor(f32 lfZoomFactor) { mfZoomFactor = lfZoomFactor; }

        // ADDITIVE GROW (BrnGui::MapIconManager TU): MapIconManager::SetRoadRuleBatchData
        // (@0x824B2F80) forwards the road-rule batch response straight into the embedded
        // RoadSignIconManager (X360 call BrnGui__RoadSignIconManager__SetRoadRuleBatchData
        // on this+0x7090). DWARF (BrnRoadSignIconManager.h:90) gives the signature. The
        // body lives in the RoadSignIconManager TU (not yet reconstructed); declared-only
        // here so the manager TU links against the real declaration.
        void SetRoadRuleBatchData(const GuiEventRoadRuleBatchDataResponse* lpRoadRules);

    private:
        // @this+0x0 : inline pool of road-sign icon components (stride 0xC0). The
        // per-icon visible flag of element 0 lands @this+0xBC, matching the asm.
        RoadSignIcon maIcons[KU_NUM_SIGN_ICONS];
        // @this+0x3000 .. +0x3104 : manager state this TU does not read by name
        // (the array ends at 0x3000; the master flag sits at 0x3105).
        u8           maReservedManagerState[0x3105 - (0xC0 * KU_NUM_SIGN_ICONS)];
        // @this+0x3105 : master "signs visible" flag (last broadcast value).
        u8           mbSignsVisible;
        // [H3b additive tail] @this+0x3106..0x3107 stride pad, then the zoom scale the
        // map screens feed (DWARF h:273; MapIconManager::Construct seeds it to 1.0).
        u8           mauPad3106[2];
        f32          mfZoomFactor;   // @this+0x3108 (== manager+0xA198)
    };
}
