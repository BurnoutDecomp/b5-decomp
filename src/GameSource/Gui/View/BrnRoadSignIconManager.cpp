#include "GameSource/Gui/View/BrnRoadSignIconManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [H3b] the SetRoadRuleBatchData park print
#include "GameShared/GameClasses/Core/CgsAssert.h"           // [F3] CGS_ASSERT
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"            // [F3] MapTransform::WorldToDevice

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824F5778 (no prior source, no
// DecFIGS DWARF for this TU). Store-for-store with the X360 asm: the broadcast is
// skipped entirely when the requested value already matches the master flag; the
// per-icon apt "_visible" view-state is only driven for icons whose current flag
// differs AND that have an apt view bound, but the per-icon flag is always stored
// when it differs. The master flag is written once after the walk.

namespace BrnGui
{
    // @ 0x824F5778
    void RoadSignIconManager::SetSignsVisible(u8 lbVisible)
    {
        if (lbVisible == mbSignsVisible)
            return;

        for (u32 i = 0; i < KU_NUM_SIGN_ICONS; ++i)
        {
            RoadSignIcon& lIcon = maIcons[i];
            if (lbVisible != lIcon.mbVisible)
            {
                if (lIcon.mbHasAptView == 1)
                {
                    const char* lpacState = (lbVisible == 1) ? "true" : "false";
                    lIcon.AddOutputAptViewState("_visible", lpacState, true);
                }
                lIcon.mbVisible = lbVisible;
            }
        }

        mbSignsVisible = lbVisible;
    }

    // @ 0x824F5818 -- adopt a road-rule batch-data response into the 64-sign pool.
    // [H3b NAMED GATE]: the body walks the sign icon pool's per-icon name/score fields,
    // which are still reserved storage here (the pool slice is parked). One-shot-logged,
    // not a silent stub; the freeburn boundary upstream (no road-rules producer) means
    // no batch response is produced yet either.
    void RoadSignIconManager::SetRoadRuleBatchData(const GuiEventRoadRuleBatchDataResponse* lpRoadRules)
    {
        (void)lpRoadRules;
        static bool sbLogged = false;
        if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: RoadSignIconManager::SetRoadRuleBatchData @0x824F5818 "
                   "(the 64-sign pool walk) is unreconstructed\n";
        }
    }

    // @ 0x824F5??? [H3c NAMED GATE] -- the per-frame road-sign pass UpdateSatNavIcons
    // calls when mbUseRoadSigns is set. The body drives the parked 64-sign pool
    // (unreconstructed); the sat-nav HUD owner claims the icon set with road signs OFF,
    // so this leg is only reachable from the big-map screens. One-shot-logged park.
    void RoadSignIconManager::Update()
    {
        static bool sbLogged = false;
        if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: RoadSignIconManager::Update (the 64-sign pool pass) "
                   "is unreconstructed\n";
        }
    }

    // ================= [F3 2026-08-29] the MapIconManager forwarder targets =================

    // @ 0x8250AE90 -- bind the 64 sign components.
    // [F3 NAMED GATE] The console body needs the 64 CgsGui::ObjectController pointers at
    // manager+0x3000 and CgsGui::ObjectController::GetPos / SetObjectVariableBoolean, plus
    // the world->map point transform at 0x8245A080. The controller pool is the parked
    // 64-sign slice (see SetRoadRuleBatchData / Update above); no controller is registered
    // on this build, so the console loop would assert-then-deref a null on its first
    // iteration. The two flag stores the body ends with (+0x3104 setup-done, +0x3105
    // signs-visible) are NOT applied here either -- claiming "set up" while no component
    // is bound is exactly the kind of half-truth that makes a later wave chase a ghost.
    void RoadSignIconManager::SetupComponent()
    {
        static bool sbLogged = false;
        if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: RoadSignIconManager::SetupComponent @0x8250AE90 "
                   "(the 64 object-controller binds) is unreconstructed\n";
        }
    }

    // @ 0x824FB1D8 -- append the sign components to the screen's expected-apt list.
    // [F3 NAMED GATE] Same parked slice: the body walks the 64 object-controller pointers
    // (asserting each is non-null), hashes each element's name and calls
    // BrnGui::StateLoadingHelper::AppendExpectedAptComponent through mpGuiCache. Neither
    // the controller pool nor mpGuiCache is bound in this manager on this build.
    void RoadSignIconManager::AppendExpectedComponents()
    {
        static bool sbLogged = false;
        if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: RoadSignIconManager::AppendExpectedComponents "
                   "@0x824FB1D8 (the 64-sign expected-component pass) is unreconstructed\n";
        }
    }

    // @ 0x824F56C0 -- the sign component's own name buffer (X360: `192 * a2 + a1 + 4`,
    // i.e. element base + 4 == CgsGui::GuiComponent::macName). Reached by name here.
    const char* RoadSignIconManager::GetIconNameAtIndex(u32 luIndex) const
    {
        CGS_ASSERT(luIndex < KU_NUM_SIGN_ICONS, "Invalid index");   // :586 (non-gating)

        return maIcons[luIndex].GetName();
    }

    // @ 0x82502B70 -- every sign's world position, transformed into device space.
    // The console re-packs the cached 2D lane {x, y} into a world {x, 0, z, 0} triple
    // (v12[0] = lane.x, v12[1] = 0, v12[2] = lane.y, v12[3] = 0) before calling
    // WorldToDevice with lbClamp = false, and stores the 16-byte result at a 16-byte
    // stride. The count is the literal 64 the body writes unconditionally.
    void RoadSignIconManager::GetRoadSignIconPositions(Vector2* lpav2Positions,
                                                       s32* lpiNumIcons) const
    {
        CGS_ASSERT(lpiNumIcons != 0, "lpiNumIcons");   // :564 (non-gating)

        for (u32 luIcon = 0; luIcon < KU_NUM_SIGN_ICONS; ++luIcon)
        {
            const Vector4& lv4Lane = maIcons[luIcon].mv4WorldPosition;

            Vector3 lv3World;
            lv3World.x = lv4Lane.x;
            lv3World.y = 0.0f;
            lv3World.z = lv4Lane.y;
            lv3World.w = 0.0f;

            lpav2Positions[luIcon] = MapTransform::WorldToDevice(lv3World, false);
        }

        *lpiNumIcons = static_cast<s32>(KU_NUM_SIGN_ICONS);
    }

    // ---------------------------------------------------------------------------------
    // ⭐ FIX1 (2026-08-29, main-menu closure wave). Declared by the base-TU grow, bodied
    // nowhere -- a guaranteed LNK2019 against the mounted BrnCrashNavMap.cpp, whose
    // SetFilterFromPanel path calls it through mpIconManager->mRoadSignIconManager.
    // ---------------------------------------------------------------------------------

    // @0x824F5920 -- a CHANGE-GUARDED single store to +0x310C. The console reads the
    // word, compares, and returns early on equality (`beqlr cr6`) before storing; the
    // guard is reproduced rather than collapsed to an unconditional store because the
    // early-out is what the emitted code actually is.
    void RoadSignIconManager::SetRoadIconFilter(s32 leScoreType)
    {
        if (meRoadIconFilter != leScoreType)
        {
            meRoadIconFilter = leScoreType;   // X360 +0x310C
        }
    }

}
