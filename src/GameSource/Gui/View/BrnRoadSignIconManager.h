#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // [F3] Vector2 / Vector4 (GetRoadSignIconPositions)
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

        // ADDITIVE GROW [F3, MapIconManager closure]. The sign's cached WORLD position
        // lane, X360-attested twice on the SAME 0xC0-stride element:
        //   * SetupComponent  @0x8250AE90 : `stvx128 v127, r0, r31` with r31 = element+0x90
        //   * GetRoadSignIconPositions @0x82502B70 : `lvx128 v0, r0, r30`, r30 = element+0x90,
        //     then lanes {0,1} are re-packed as a world {x, 0, z, 0} triple for WorldToDevice.
        // On the console it lives at element+0x90, INSIDE the bytes maReservedBaseTail
        // models; the host GuiComponent base is already wider than 0x90 (its name buffer
        // plus a 64-bit interface pointer), so the field is appended by name instead --
        // a raw console offset applied to a host object is exactly the defect class this
        // project bans. Only lanes {x, y} are written by the console (a 2D world point).
        Vector4 mv4WorldPosition;   // X360 element+0x90 (host placement differs)

        // ADDITIVE GROW [crash-nav FIX2]. Which of the four authored sign-plate COLOURS this
        // sign draws with. X360-attested as the word at element+0xB4, read by
        // BrnGui::CrashNavIconRenderer::RenderRoadSign at all four of its sign-size arms
        // (`lwz r11, 0x1544(r31) / add r11, r30, r11 / lwz r11, 0xB4(r11)` -- r30 == 192*i,
        // i.e. this 0xC0-stride element), where it drives a 4-way switch that picks both the
        // plate's atlas rect and the text colour, and whose default fires the assert
        // "Unknown sign colour" (BrnCrashNavIconRenderer.cpp:2050/2096/2142/2188).
        // Same host-placement caveat as mv4WorldPosition above: the console offset sits inside
        // the bytes maReservedBaseTail models, so the field is appended BY NAME.
        // ⚠️ No producer writes it yet on this build (the sign pool is parked, see
        // RoadSignIconManager::Update below) -- consumers must treat it as 0 until one does.
        u32 meSignColour;           // X360 element+0xB4 (host placement differs)

        // The bound the RenderRoadSign switch checks (`cmplwi r11, 3 / bgt default`).
        static const u32 KU_NUM_SIGN_COLOURS = 4;
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

        // [H3c] MapIconManager::UpdateSatNavIcons @0x82522588 calls the per-frame sign
        // pass (X360 BrnGui__RoadSignIconManager__Update on this+0x7090) when road signs
        // are enabled. The body drives the parked 64-sign pool -- one-shot-logged park in
        // the cpp; only reachable with mbUseRoadSigns set (the big-map screens -- the
        // sat-nav HUD owner claims the set with road signs OFF).
        void Update();

        // ---------------------------------------------------------------------------
        // ADDITIVE GROW [F3 2026-08-29, the MapIconManager closure wave]. Four rows the
        // MapIconManager forwarders reach; each is its own X360 ledger function.
        // ---------------------------------------------------------------------------

        // @0x8250AE90 -- bind the 64 sign components: per icon compress its name to a
        // CgsID, take the sign's world position from its object controller (or keep the
        // cached lane once the pool has already been set up), stamp the id/flag pair and
        // drive the controller's "_visible" object variable true. Body in this TU.
        void SetupComponent();

        // @0x824FB1D8 -- append every sign component's hashed name to the owning screen's
        // expected-apt-component list (and re-register each object controller with the
        // cache). Body in this TU.
        void AppendExpectedComponents();

        // @0x824F56C0 -- the interned component name of the sign at liIndex (the caller
        // compares the returned pointer by identity). The X360 asserts the index and then
        // returns `element + 4` == the component's own name buffer.
        const char* GetIconNameAtIndex(u32 luIndex) const;

        // @0x82502B70 -- transform all 64 sign world positions into device space and
        // report the count (always KU_NUM_SIGN_ICONS -- the X360 stores the literal 64).
        void GetRoadSignIconPositions(Vector2* lpav2Positions, s32* lpiNumIcons) const;

        // The icon count the MapIconManager index asserts compare against ("liRoadSignIndex
        // < mRoadSignIconManager.GetNumIcons()", BrnMapIconManager.cpp:2602). Inline on the
        // console -- GetRoadSignNameAtIndex @0x824FAA50 compares against the literal 64.
        s32 GetNumIcons() const { return static_cast<s32>(KU_NUM_SIGN_ICONS); }

        // ADDITIVE GROW (2026-08-29, the CrashNavMap base TU). @0x824F5920 -- latch which
        // road-rule score class the sign pool should draw. The whole X360 body is a
        // change-guarded single store to this+12556 == 0x310C, the word directly after
        // mfZoomFactor, so the member is appended below at that offset. DWARF spells the
        // parameter `BrnStreetData::ScoreType` (BrnMapIconManager.h:306 forwards the same
        // type); kept s32 here to match the committed
        // CrashNavPanel::GetPanelActiveRoadRuleType, whose result is the only value any
        // reconstructed caller passes. DECLARATION-ONLY -- body belongs to this TU.
        void SetRoadIconFilter(s32 leScoreType);

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
        // [base-TU wave] @this+0x310C (== 12556, the SetRoadIconFilter store target above).
        // FLAG consumer-named: no DWARF member row is pinned to this offset; the type is
        // whatever CrashNavPanel::GetPanelActiveRoadRuleType returns.
        s32          meRoadIconFilter;   // @this+0x310C
    };
}
