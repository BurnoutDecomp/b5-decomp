#pragma once

// BrnMapIconManager.h
// BrnGui::MapIconManager - owns the on-map icon set the sat-nav / crash-nav maps draw
// (per-icon sat-nav info, the crash-nav and sat-nav icon pools, the road-sign icon
// manager and the event-icon manager) and answers the map UI's queries about which
// icon sits at a selection index.
//
// Sources (source-of-truth ladder):
//   * X360 BURNOUT_X360_ARTIST.XEX (binary; AUTHORITATIVE on layout/widths/branches):
//       MapIconManager (ctor)            @ 0x827DF138
//       ResetOwnerParameter              @ 0x824EBF98
//       SetRoadRuleBatchData             @ 0x824B2F80
//       GetNumRivalIcons                 @ 0x824F7B60
//       GetDriveThroughOrJunkyardAtIndex @ 0x824FAC10  (IDA "GetDriveThroughOrJun")
//       AddTeamToNetworkRivals           @ 0x824F4FF8  (UpdateSatNavIcons network-rivals pass)
//     ...plus, for the declaration-only rows added by the wave-J screen states:
//       SetOwnerParameters               @ 0x82520CE8   ReleaseResources   @ 0x82520C40
//       UpdateSatNavInfo                 @ 0x825023D0   Update             @ 0x82525EF8
//       AppendExpectedAptComponents      @ 0x82502A80   SetupComponent     @ 0x825123C0
//       GetSatNavIconPositions           @ 0x8250A708   GetRivalIconAtIndex@ 0x824FA7E0
//       GetRoadSignNameAtIndex           @ 0x824FAA50   GetEventIDAtIndex  @ 0x824FAB38
//       GetDriveThroughAndJunkyardCount  @ 0x824F4888   SetIconsVisible    @ 0x82525F18
//   * references/DecFIGS/dwarfdump/GameSource/Gui/SatNav/BrnMapIconManager.h
//       names the nested enums, the member set/order and the method shapes.
//
// PARTIAL-LAYOUT NOTE (same convention as BrnSatNavIcon.h / BrnGameModule): the full
// class is a large aggregate (the X360 `this` spans 0xAA00+ bytes: a 50-entry sat-nav
// info array, a 50-entry crash-nav icon pool, a 16-entry sat-nav icon pool, the embedded
// road-sign and event icon managers, and a long tail of selection/flag state). Only the
// members the reconstructed methods touch are modelled, in their DWARF order and with
// their real types; the X360 byte offset of each is given in a comment as a reference
// (semantic parity, not byte-exact). The not-yet-reconstructed members (the icon pools,
// the event-icon manager, the bulk of the flag tail) land with the rest of the TU.

#include <cstddef>                                      // offsetof (the layout pins in _AssertLayout)

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector2 (GetSatNavIconPositions)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"          // GuiEventUpdateSatNav::SatNavIconInfo, GuiEventDrawEventIcons::EIconDisplayType
#include "GameSource/Gui/View/BrnRoadSignIconManager.h"  // BrnGui::RoadSignIconManager (embedded) + GuiEventRoadRuleBatchDataResponse fwd

// Pointer-only use in SetOwnerParameters/ReleaseResources -- forward-declared rather than
// pulling the whole state-interface header in. `struct` is the canonical tag
// (CgsGuiStateInterface.h:99); a class/struct split silently breaks the mangled names.
namespace CgsGui { struct StateInterface; }
namespace CgsModule { struct Event; }   // UpdateSatNavParams payload (pointer-only)

namespace BrnGui
{
    class GuiCache;                  // embedded by pointer (mpGuiCache); declared in BrnGuiCache.h
    struct OnlineGameRoomPlayerInfo; // friend (writes meIconSizeMode; see the friend note below)
    struct CrashNavMap;              // friend (wave J; BrnCrashNavMap.h declares it as a struct)
    struct PreRaceFlyByState;        // friend (wave J; BrnPreRaceFlyBy.h declares it as a struct)
    struct SatNavComponent;          // friend (H3a; the owner-change pokes + the Construct icon-count reset)

    class MapIconManager
    {
    public:
        // ---- nested enums (DWARF: BrnMapIconManager.h:52/58/78) ----
        enum IconSizeMode
        {
            E_ICONSIZE_SMALL = 0,
            E_ICONSIZE_LARGE = 1,
        };

        enum IconFilterMode
        {
            E_ICONFILTER_ALL                          = 0,
            E_ICONFILTER_LANDMARKS_ALL                = 1,
            E_ICONFILTER_LANDMARKS_PALM_BAY_HEIGHTS   = 2,
            E_ICONFILTER_LANDMARKS_SILVER_LAKE        = 3,
            E_ICONFILTER_LANDMARKS_HARBOR_TOWN        = 4,
            E_ICONFILTER_LANDMARKS_WHITE_MOUNTAIN     = 5,
            E_ICONFILTER_LANDMARKS_DOWNTOWN_PARADISE  = 6,
            E_ICONFILTER_PLAYER_ONLY                  = 7,
            E_ICONFILTER_NO_RIVALS                    = 8,
            E_ICONFILTER_WORLDOFFENCES                = 9,
        };

        enum OwnerId
        {
            E_OWNERID_INVALID                   = 0,
            E_SATNAV_MAP                        = 1,
            E_CRASHNAV_MAP                      = 2,
            E_PRERACE_FLYBY_MAP                 = 3,
            E_CRASHNAV_MAP_ONLINE               = 4,
            E_CRASHNAV_MAP_ONLINE_SELECT_ROUTE  = 5,
        };

        // ---- capacities (DWARF: BrnMapIconManager.h:88-92) ----
        static const s32 KI_MAX_CRASHNAV_MAP_ICONS = 50;
        static const s32 KI_MAX_SATNAV_MAP_ICONS   = 16;
        static const s32 KI_SATNAV_MAX_ICONS       = 50;

        // @ 0x827DF138 -- constructs the manager: brings each icon pool's elements up as
        // live (polymorphic) icon objects. In the X360 build this is the manual vtable
        // write across the three pools; in C++ it is the members' own construction.
        MapIconManager();

        // @ 0x824FA0F0 (DWARF h:110) -- bind the cache and reset the selection/flag
        // surface (GuiModule::Construct calls it right after GuiCache::Construct).
        void Construct(GuiCache* lpGuiCache);

        // @ 0x824B2F80 -- forward the road-rule batch response into the embedded road-sign
        // icon manager (the X360 asserts the pointer is non-null first).
        void SetRoadRuleBatchData(const GuiEventRoadRuleBatchDataResponse* lpRoadRules);

        // @ 0x824FAC10 -- return the drive-through / junkyard sat-nav icon at the given map
        // selection index (skipping hidden drive-throughs and the road-rage / marked-man
        // mode's paint-shop, and the non-free-burn-lobby junkyards). When rival selection is
        // active the rival icons occupy the front of the index space, so the rival count is
        // subtracted off the incoming index first.
        const GuiEventUpdateSatNav::SatNavIconInfo* GetDriveThroughOrJunkyardAtIndex(s32 liIndex) const;

        // -------------------------------------------------------------------------------
        // ADDITIVE GROW (wave J: CrashNavMap + PreRaceFlyByState). Every row below EXCEPT
        // the last (SetZoomFactor -- flagged at its own declaration) is an out-of-line X360
        // function with its own ledger entry; the shapes are the DWARF rows
        // (BrnMapIconManager.h line numbers quoted), listed in DWARF order.
        // DECLARATION-ONLY on purpose -- the bodies belong to this class's own TU
        // (BrnMapIconManager.cpp) and link when that slice lands.
        // -------------------------------------------------------------------------------

        // DWARF h:115, @0x82520CE8 -- take ownership of the shared map-icon set for one
        // screen and return the owner id actually granted (the caller stores the result
        // back over the id it asked for).
        //
        // The parameter list is MEASURED from the callee prologue + the PreRaceFlyBy call
        // site @0x824C68A8, not from Hex-Rays (which drops the tail): `this` plus SEVEN
        // args occupy r3..r10 and the last TWO travel in the caller's parameter save area
        // (callee `lwz ...,arg_54` @0x82521148 and `lwz ...,arg_5C` @0x82520E54; caller
        // `stw` of 5 and of the parent-name pointer to exactly those slots). Each argument
        // is named after the member it lands in, all measured inside this callee:
        //   liMaxNumberIcons        -> miMaxNumberIcons          (stw  +0x994  @0x825210A0)
        //   lbUseRoadSigns          -> mbUseRoadSigns            (stbx +0xA1B0 @0x82521154)
        //                              and RoadSignIconManager::SetSignsVisible
        //   lbShowingDriveThrus     -> mbShowingDriveThrus       (stbx +0xA9F4 @0x825210B0)
        //   lbAllowDriveThruSelection -> mbAllowDriveThruSelection, ANDed with the flag
        //                              above (stb  +0x7080 @0x825210D0)
        //   leEventIconDisplayType  -> meEventIconDisplayType    (stwx +0xA9F0 @0x825211C4)
        // The first two names are the X360's own assert strings ("lpStateInterface" @:263,
        // "lpcComponentName" @:264).
        OwnerId SetOwnerParameters(CgsGui::StateInterface* lpStateInterface,
                                   const char* lpcComponentName,
                                   s32 liMaxNumberIcons,
                                   OwnerId leOwnerId,
                                   bool lbUseRoadSigns,
                                   bool lbShowingDriveThrus,
                                   bool lbAllowDriveThruSelection,
                                   GuiEventDrawEventIcons::EIconDisplayType leEventIconDisplayType,
                                   const char* lpcIconParentName);

        // DWARF h:121, @0x82520C40 -- give the icon set back. A no-op unless leOwnerId is
        // the current owner (the X360 compares against mOwnerId @+0xAA00 and returns);
        // otherwise it unregisters the 64 road-sign object controllers, releases the event
        // icons, stores 1 into the +0xAA22 flag byte (DWARF h:477 mbIsActive -- the stored
        // value is what the binary writes; the flag's role is not recovered here) and
        // calls ResetOwnerParameter.
        void ReleaseResources(CgsGui::StateInterface* lpStateInterface, OwnerId leOwnerId);

        // DWARF h:134, @0x825023D0 -- refresh the per-icon sat-nav info set from the
        // incoming sat-nav GUI event.
        void UpdateSatNavInfo(const GuiEventUpdateSatNav* lpSatNavEvent);

        // @0x824F4458 -- adopt the sat-nav parameter record (event id 200; the rotate /
        // trajectory / rival-visibility flags). ADDITIVE GROW (H3a 2026-08-25:
        // SatNavComponent::RecvEvent case 200 calls it); declaration-only this slice --
        // the body lands with the manager TU's remainder.
        void UpdateSatNavParams(const CgsModule::Event* lpParamsEvent);

        // DWARF h:143, @0x82525EF8 -- the per-frame icon update: tail-calls
        // UpdateSatNavIcons when mOwnerId == E_SATNAV_MAP, UpdateCrashNavIcons otherwise.
        void Update();

        // DWARF h:147, @0x82502A80 -- forward the "components this screen expects" pass
        // into the embedded road-sign icon manager.
        void AppendExpectedAptComponents();

        // DWARF h:151, @0x825123C0 -- bind the apt components; forwards into the road-sign
        // icon manager, and only when mbUseRoadSigns is set.
        void SetupComponent();

        // DWARF h:165, @0x8250A708 -- collect every on-screen icon position (device space)
        // into lpav2Positions and write the count to *lpiNumIcons. The order is the map
        // cursor's snap-index space: rivals, road signs, drive-throughs/junkyards, event
        // icons, then the local player last.
        void GetSatNavIconPositions(Vector2* lpav2Positions, s32* lpiNumIcons);

        // DWARF h:169, @0x824FA7E0 -- the rival icon at a map selection index.
        const GuiEventUpdateSatNav::SatNavIconInfo* GetRivalIconAtIndex(s32 liIndex);

        // DWARF h:174, @0x824FAA50 -- the interned road name of the road-sign icon at a
        // map selection index (callers compare the returned pointer by identity).
        const char* GetRoadSignNameAtIndex(s32 liIndex) const;

        // DWARF h:179, @0x824FAB38 -- the event id of the event icon at a map selection
        // index.
        u32 GetEventIDAtIndex(s32 liIndex) const;

        // DWARF h:188, @0x824F4888 -- how many drive-through / junkyard icons occupy the
        // front of the selection-index space.
        s32 GetDriveThroughAndJunkyardCount() const;

        // DWARF h:197, @0x82525F18 -- show/hide the whole on-map icon set. Stores
        // mbIconsVisible (+0xAA1E); when hiding, it also clears miNumUsedIcons and
        // re-runs the owner's icon pass.
        void SetIconsVisible(bool lbVisible);

        // DWARF h:207 -- feed the manager the map's current zoom-derived icon scale.
        // FLAG inline-folded: this is the ONE DWARF row with no X360 ledger entry that
        // this header declares. It is attested indirectly but unambiguously -- the X360
        // fully inlined it, and CrashNavMap::UpdateIconManager @0x824CBAEC emits the
        // resulting single store `stfsx f0, mpIconManager, 0xA198`. That target is
        // RoadSignIconManager::mfZoomFactor (DWARF BrnRoadSignIconManager.h:273) inside
        // the embedded mRoadSignIconManager, NOT a member of this class: the road-sign
        // manager starts at +0x7090, its 64-entry 0xC0-stride icon pool ends at +0xA090,
        // its 64 object-controller pointers end at +0xA190, then (DWARF
        // BrnRoadSignIconManager.h:270-273) mpGuiCache +0xA190, mbIconsTransformed +0xA194,
        // mbComponentVisible +0xA195 -- and that last one is the master flag the committed
        // BrnRoadSignIconManager.h already pins at manager+0x3105, which corroborates the
        // whole chain -- putting mfZoomFactor at +0xA198 exactly.
        // Why a declared method here and not the friend+member treatment the flag tail
        // gets: mRoadSignIconManager is private and the target member belongs to ANOTHER
        // class, so no friendship of this class could reach it -- the original caller must
        // have gone through a public method, and this is the only float-taking one the
        // DWARF gives. Body (a one-line forward to mRoadSignIconManager.SetZoomFactor)
        // lands with this class's own TU.
        void SetZoomFactor(f32 lfZoomFactor);

    private:
        // The online game-room screen's Update stores meIconSizeMode directly (the X360
        // inlines the raw stwx at 0x824B1418; no DWARF accessor row) -- friendship, not
        // a fabricated setter, is the honest exposure (wave-H keystone; same rule as
        // GuiCache's consumer friends).
        friend struct OnlineGameRoomPlayerInfo;

        // Same rule, wave J. The DWARF declares a setter for nearly every member of the
        // selection/flag tail (SetIconFilter h:212, SetRotateSatNav h:220,
        // SetCurrentEventIndex h:224, SetSelectedCheckpointInMenu h:244, ...), and NONE of
        // them has an X360 ledger entry -- they were inline one-liners the compiler folded,
        // which is exactly why these two screens show raw stwx/stbx into the manager. The
        // stores are real; the setters have no bodies to link. Friendship keeps the stores
        // honest without minting inline bodies this project cannot attest.
        friend struct CrashNavMap;         // mi8CurrentEventIndex, mbIsDisplayingEventInfo,
                                           // meIconFilterMode, meIconSizeMode, mbRotateSatNav,
                                           // miSelectedCheckpoint, muSelectedJunctionID,
                                           // mbShowingCrashNavRoute, miNumUsedIcons
        friend struct PreRaceFlyByState;
        friend struct SatNavComponent;   // H3a: Update's owner-change pokes (mbIsDisplayingEventInfo /
                                         // mbRotateSatNav / meIconSizeMode) + Construct's miNumUsedIcons reset
        friend struct FBurnMainHudState; // H3b: the freeburn HUD's per-frame pre-pass clears
                                         // miNumUsedIcons through the component's manager pointer
                                         // (X360 UpdateRunning @0x8247B660 head)   // meIconFilterMode, mbIsDisplayingEventInfo,
                                           // miSelectedCheckpoint, muSelectedJunctionID,
                                           // mbRotateSatNav, meIconSizeMode,
                                           // mbShowingPreRaceRoute, miNumUsedIcons

        // @ 0x824F7B60 -- count the rival icons in the sat-nav info set (network rivals,
        // marked men and ordinary rivals).
        s32 GetNumRivalIcons() const;

        // @ 0x824F4FF8 -- the network-rivals pass of UpdateSatNavIcons: for every
        // E_SATNAVICON_NETWORKRIVAL icon, stamp it with the player's current online team
        // (looked up from the cache by the player's active-race-car index).
        void AddTeamToNetworkRivals();

        // @ 0x824EBF98 -- reset the owner id back to E_OWNERID_INVALID (with a debug trace).
        void ResetOwnerParameter();

        // @ 0x824FAE60 -- is this rival-type icon currently active (game mode not
        // road-rage, filter not PLAYER_ONLY/NO_RIVALS)? Asserts the icon IS a rival type.
        bool IsActiveRival(const GuiEventUpdateSatNav::SatNavIconInfo* lpIcon) const;

        // The two per-owner icon passes Update dispatches to (X360 @0x82522588 /
        // @0x825212C0). [H3b NAMED GATE -- see the bodies: the apt icon pools +
        // road-sign / event-icon / world-icon passes they drive are not reconstructed.]
        void UpdateSatNavIcons();
        void UpdateCrashNavIcons();

        // -------------------------------------------------------------------------------
        // Modelled members (DWARF order + types; X360 byte offsets are references, see the
        // partial-layout note above). Members the reconstructed methods do not touch are
        // omitted; they land with the rest of the TU.
        // -------------------------------------------------------------------------------

        // @0x0000 -- the per-icon sat-nav info set (validity loop / rival count / team stamp).
        GuiEventUpdateSatNav::SatNavIconInfo mSatNavIconInfo[KI_SATNAV_MAX_ICONS]; // X360 +0x0000
        GuiEventUpdateSatNav::SatNavIconInfo mPlayerIconInfo;                       // X360 +0x0960
        s32                                  miNumUsedIcons;                        // X360 +0x0990 (count for the loops)
        // ADDITIVE GROW (wave J). DWARF h:430/h:432, both offsets MEASURED:
        //   +0x994 SetOwnerParameters `stw r16, 0x994(r25)` @0x825210A0 (the liMaxNumberIcons
        //          argument, asserted 0 <= n <= 50 at :302/:303)
        //   +0x998 CrashNavMap::UpdateIconManager `stb r10, 0x998(r11)` @0x824CBAF8 -- a
        //          BYTE store, matching the DWARF's int8_t
        s32                                  miMaxNumberIcons;                      // X360 +0x0994
        s8                                   mi8CurrentEventIndex;                  // X360 +0x0998

        // [icon pools + event-icon manager + the bulk of the flag tail: not modelled here]

        bool                mbAllowDriveThruSelection; // X360 +0x7080 (DWARF order; SetOwnerParameters ANDs it with the show flag)
        bool                mbAllowRivalSelection;     // X360 +0x7081 (rivals occupy the front of the selection list)
        RoadSignIconManager mRoadSignIconManager;      // X360 +0x7090 (SetRoadRuleBatchData target)
        bool                mbUseRoadSigns;            // X360 +0xA1B0 (DWARF order after the embedded manager; SetOwnerParameters stbx @0x82521154)
        bool                mbAllowPlayerSelection;    // X360 +0xA1B1 (SetOwnerParameters stores ownerId != E_CRASHNAV_MAP_ONLINE_SELECT_ROUTE)

        // [mEventIconManager (X360 +0xA1B4..+0xA9EF): not modelled here -- its Prepare/
        //  Release passes ride the parked icon slice]

        GuiEventDrawEventIcons::EIconDisplayType meEventIconDisplayType; // X360 +0xA9F0 (DWARF h:453)
        bool                mbShowingDriveThrus;       // X360 +0xA9F4 (DWARF h:454; SetOwnerParameters stbx @0x825210B0)
        GuiCache*           mpGuiCache;                // X360 +0xA9F8 (drive-through list + player team lookups)
        CgsGui::StateInterface* mpStateInterface;      // X360 +0xA9FC (DWARF h:456; SetOwnerParameters stw @0x82520D..)
        OwnerId             mOwnerId;                  // X360 +0xAA00 (reset to invalid on release)
        // ADDITIVE GROW (OnlineGameRoomPlayerInfo keystone, wave H): the icon size mode
        // (DWARF h:458, the member right after mOwnerId). X360 +0xAA04 -- the game-room
        // screen's Update stores E_ICONSIZE_LARGE (stwx 1, iconmgr+0xAA04 @0x824B1418)
        // once the map cursor first snaps to the local player.
        IconSizeMode        meIconSizeMode;            // X360 +0xAA04

        // ADDITIVE GROW (wave J: CrashNavMap + PreRaceFlyByState). The rest of the
        // selection/flag tail, in DWARF order (h:459..h:475). It is one contiguous
        // pointer-free scalar run on the console AND on the host, and nine of its fifteen
        // console offsets are measured stores/loads:
        //   +0xAA08 PreRaceFlyByState::UpdateIconManager  stwx 0            @0x824C7C64
        //   +0xAA10 PreRaceFlyByState::UpdateIconManager  stwx GetJunctionID@0x824C7C90
        //   +0xAA14 PreRaceFlyByState::UpdateIconManager  stwx 0            @0x824C7C80
        //   +0xAA18 PreRaceFlyByState::UpdateIconManager  stbx 0            @0x824C7C78
        //   +0xAA1C PreRaceFlyByState::OnEnter            stbx 0            @0x824C68CC
        //   +0xAA1E MapIconManager::SetIconsVisible       stbx lbVisible    @0x82525F28
        //   +0xAA1F MapIconManager::GetSatNavIconPositions lbzx (gate)      @0x8250A87C
        //   +0xAA20 PreRaceFlyByState::OnEnter            stbx 1            @0x824C68DC
        //   +0xAA21 CrashNavMap::UpdateEvent              stbx 0            @0x824CC4B0
        // The six unmeasured rows are boxed between measured neighbours by the DWARF
        // order, so their placement is forced rather than guessed. (The console offsets
        // are references only -- every access here is by name.)
        IconFilterMode      meIconFilterMode;              // X360 +0xAA08 (DWARF h:459)
        // DWARF h:461 types this `LightTriggerId`. That handle's committed home is
        // BrnGameState::BrnGameModeParams.h (`typedef u32 LightTriggerId`); spelled as its
        // underlying u32 here so this GUI header does not drag the game-mode header in --
        // the same choice BrnGameEvents.h:218 already made. FLAG: retype if the handle ever
        // becomes a real class.
        u32                 mSelectedLightTriggerID;       // X360 +0xAA0C (DWARF h:461)
        u32                 muSelectedJunctionID;          // X360 +0xAA10 (DWARF h:462)
        s32                 miSelectedCheckpoint;          // X360 +0xAA14 (DWARF h:463)
        bool                mbIsDisplayingEventInfo;       // X360 +0xAA18 (DWARF h:465)
        bool                mbRivalFovFreeburn;            // X360 +0xAA19 (DWARF h:466)
        bool                mbRivalFovRace;                // X360 +0xAA1A (DWARF h:467)
        bool                mbUseTrajectory;               // X360 +0xAA1B (DWARF h:468)
        bool                mbRotateSatNav;                // X360 +0xAA1C (DWARF h:469)
        bool                mbShowOffLineRivalsOnSatNav;   // X360 +0xAA1D (DWARF h:470)
        bool                mbIconsVisible;                // X360 +0xAA1E (DWARF h:472; SetIconsVisible target)
        bool                mbShowingOnlineRoute;          // X360 +0xAA1F (DWARF h:473)
        bool                mbShowingPreRaceRoute;         // X360 +0xAA20 (DWARF h:474)
        bool                mbShowingCrashNavRoute;        // X360 +0xAA21 (DWARF h:475)
        bool                mbIsActive;                    // X360 +0xAA22 (DWARF h:477; ReleaseResources `stbx 1` @0x82520CD8, Construct/SetOwnerParameters store 1)

        // Never called; the compiler evaluates the assertions. Only RELATIVE deltas inside
        // pointer-free scalar runs are pinned -- absolute console offsets are meaningless
        // on the LLP64 host because every pointer above these runs widens.
        static void _AssertLayout()
        {
            // run A: the icon-count trio (X360 +0x990..+0x998)
            static_assert(offsetof(MapIconManager, miMaxNumberIcons)     - offsetof(MapIconManager, miNumUsedIcons) == 4, "run A order");
            static_assert(offsetof(MapIconManager, mi8CurrentEventIndex) - offsetof(MapIconManager, miNumUsedIcons) == 8, "run A order");
            // run B: the selection/flag tail (X360 +0xAA08..+0xAA21)
            static_assert(offsetof(MapIconManager, mSelectedLightTriggerID) - offsetof(MapIconManager, meIconFilterMode) == 0x04, "run B order");
            static_assert(offsetof(MapIconManager, muSelectedJunctionID)    - offsetof(MapIconManager, meIconFilterMode) == 0x08, "run B order");
            static_assert(offsetof(MapIconManager, miSelectedCheckpoint)    - offsetof(MapIconManager, meIconFilterMode) == 0x0C, "run B order");
            static_assert(offsetof(MapIconManager, mbIsDisplayingEventInfo) - offsetof(MapIconManager, meIconFilterMode) == 0x10, "run B order");
            static_assert(offsetof(MapIconManager, mbRotateSatNav)          - offsetof(MapIconManager, meIconFilterMode) == 0x14, "run B order");
            static_assert(offsetof(MapIconManager, mbIconsVisible)          - offsetof(MapIconManager, meIconFilterMode) == 0x16, "run B order");
            static_assert(offsetof(MapIconManager, mbShowingOnlineRoute)    - offsetof(MapIconManager, meIconFilterMode) == 0x17, "run B order");
            static_assert(offsetof(MapIconManager, mbShowingPreRaceRoute)   - offsetof(MapIconManager, meIconFilterMode) == 0x18, "run B order");
            static_assert(offsetof(MapIconManager, mbShowingCrashNavRoute)  - offsetof(MapIconManager, meIconFilterMode) == 0x19, "run B order");
        }
    };
}
