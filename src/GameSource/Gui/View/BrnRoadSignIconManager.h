#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 / Vector4, CgsID
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base), CgsGui::StateInterface
#include "GameSource/Gui/BrnGuiShared.h"   // BrnGui::ERoadIcon (+ gapcRoadIconNames, the 64 icon names)

// BrnGui::RoadSignIcon / BrnGui::RoadSignIconManager -- the 64 road-rule signs on the big
// map. Every sign is a GuiComponent named after its road's junction id; the manager binds an
// apt ObjectController to each one, keeps their world positions, colours them from the
// road-rule batch data and publishes the whole bank to the crash-nav icon renderer (GUI
// event 562). Declaration shape from the original BrnRoadSignIconManager.h; the member set
// is gated on the console ledger. Members are accessed by name; the console offsets in the
// comments are documentation (the host GuiComponent base is wider).
namespace CgsGui { struct ObjectController; }   // pointer-only (mapObjectController)

namespace BrnGui
{
    class GuiCache;
    struct GuiEventRoadRuleBatchDataResponse;

    // Console size 0xC0 (the pool stride every walker uses).
    struct RoadSignIcon : public CgsGui::GuiComponent
    {
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

        enum ESignColour
        {
            E_SIGN_COLOUR_GREEN  = 0,
            E_SIGN_COLOUR_RED    = 1,
            E_SIGN_COLOUR_SILVER = 2,
            E_SIGN_COLOUR_GOLD   = 3,
            E_SIGN_COLOUR_COUNT  = 4,
        };

        // The colour bound the crash-nav renderer's sign switch checks against.
        static const u32 KU_NUM_SIGN_COLOURS = E_SIGN_COLOUR_COUNT;

        // The component construct plus the sign's own defaults. lbCommunicateWithApt
        // false (the manager's pool) keeps every setter below apt-silent: they only store.
        void Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, bool lbCommunicateWithApt);

        // Place the sign (no standalone console symbol: inlined into
        // RoadSignIconManager::SetupComponent, whose "leIcon >= 0 && leIcon < E_ROADICON_COUNT"
        // assert is this method's).
        void SetIcon(ERoadIcon leIcon, const Vector2* lpv2WorldPos, EIconType leIconType, CgsID lRoadId);

        void SetVisible(bool lbVisible);

        // Show a road's sign artwork: by icon (builds "RD_<id>") or by
        // the frame name itself. lbShowPost drives the sign's post.
        void DisplayRoad(ERoadIcon leIcon, bool lbShowPost);
        void DisplayRoad(const char* lpcRoadName, bool lbShowPost);

        // The icon whose name is lpIconName (E_ROADICON_COUNT, after an assert,
        // when there is none). Reads no member.
        ERoadIcon FindRoadFromName(const char* lpIconName);

        void SetScreenPosition(Vector2 lv2ScreenPos);
        void SetColour(ESignColour leColour);
        void SetScale(Vector2 lv2Scale);

        // Originally `Vector2 mv2WorldPos` (console +0x90): the sign's 2D world position
        // {x, z} in lanes {0, 1}. FLAG: kept under the committed name and type the crash-nav
        // renderer already reads (BrnCrashNavIconRenderer.cpp RenderRoadSign); the rename to
        // the original spelling is a two-line change there.
        Vector4     mv4WorldPosition;
        EIconType   meIconType;             // console +0xA0
        CgsID       mRoadId;                // console +0xA8
        bool        mbShowPost;             // console +0xB0
        ESignColour meSignColour;           // console +0xB4
        bool        mbOfflineTimeRuled;     // console +0xB8
        bool        mbOfflineCrashRuled;    // console +0xB9
        bool        mbOnlineTimeRuled;      // console +0xBA
        bool        mbOnlineCrashRuled;     // console +0xBB

    private:
        friend class RoadSignIconManager;   // SetSignsVisible walks the two flags below

        static const char* KAPC_SIGN_COLOURS[E_SIGN_COLOUR_COUNT];
        static const char* mpRoadPrefix;
        static const char* mpShowSign;
        static const char* mpTrue;
        static const char* mpFalse;
        static const char* mpacRoadFrameName;
        static const char* mpacRoadColour;

        bool        mbSignVisible;          // console +0xBC
        bool        mbCommunicateWithApt;   // console +0xBD
    };

    class RoadSignIconManager
    {
    public:
        static const u32 KU_NUM_SIGN_ICONS = E_ROADICON_COUNT;

        // Inlined into MapIconManager::Construct: clear the controller table
        // and the manager tail.
        void Construct();

        // Construct the 64 sign components (named after their roads) and give
        // each an apt ObjectController, registered with apt.
        void Prepare(CgsGui::StateInterface* lpStateInterface, GuiCache* lpGuiCache);

        // Inlined into MapIconManager::ReleaseResources: unregister and forget
        // every controller.
        void ReleaseResources();

        // The per-frame sign pass: screen position, zoom scale and road-rule
        // colour of every sign, then the bank handed to the crash-nav renderer (GUI event 562).
        void Update();

        // The 64 sign components into the screen's expected-apt list.
        void AppendExpectedComponents();

        // Every sign's world position in device space, and the count (always 64).
        void GetRoadSignIconPositions(Vector2* lpav2Positions, s32* lpiNumIcons) const;

        // The sign component's own name.
        const char* GetIconNameAtIndex(u32 luIndex) const;

        // Read each sign's authored stage position off its apt object into the
        // world, then show its road.
        void SetupComponent();

        // Adopt the road-rule batch response's per-road ruled flags.
        void SetRoadRuleBatchData(const GuiEventRoadRuleBatchDataResponse* lpRoadRules);

        // Latch which road-rule score class the colours follow. The original types the
        // parameter BrnStreetData::ScoreType; kept s32 to match the committed caller
        // (CrashNavPanel::GetPanelActiveRoadRuleType).
        void SetRoadIconFilter(s32 leRoadRuleType);

        // Inline on the console (GetRoadSignNameAtIndex compares against 64).
        s32 GetNumIcons() const { return static_cast<s32>(KU_NUM_SIGN_ICONS); }

        // Broadcast the visible flag across every sign.
        void SetSignsVisible(bool lbVisible);

        // Inline on the console (the map screens store the zoom scale straight
        // into mfZoomFactor).
        void SetZoomFactor(f32 lfZoomFactor) { mfZoomFactor = lfZoomFactor; }

    private:
        // Update's zoom-to-scale ramp: the HD set and the SD set.
        static const f32 KF_ZOOM_RANGE;
        static const f32 KF_ZOOM_ADJUST;
        static const f32 KF_MIN_SCALE_FACTOR;
        static const f32 KF_MAX_SCALE_FACTOR;
        static const f32 KF_MIN_VIEW_LERP;
        static const f32 KF_MAX_VIEW_LERP;
        static f32       KF_SD_MIN_SCALE_FACTOR;
        static f32       KF_SD_MAX_SCALE_FACTOR;
        static f32       KF_SD_MIN_VIEW_LERP;
        static f32       KF_SD_MAX_VIEW_LERP;

        RoadSignIcon               mIcons[KU_NUM_SIGN_ICONS];                // console +0x0000
        CgsGui::ObjectController*  mapObjectController[KU_NUM_SIGN_ICONS];   // console +0x3000
        GuiCache*                  mpGuiCache;                               // console +0x3100
        bool                       mbIconsTransformed;                       // console +0x3104
        bool                       mbComponentVisible;                       // console +0x3105
        f32                        mfZoomFactor;                             // console +0x3108
        // Originally BrnStreetData::ScoreType; s32 to match SetRoadIconFilter.
        s32                        meRoadRuleType;                           // console +0x310C
        CgsGui::StateInterface*    mpStateInterface;                         // console +0x3110
    };
}
