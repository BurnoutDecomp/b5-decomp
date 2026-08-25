#include "GameSource/Gui/View/BrnRoadSignIconManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [H3b] the SetRoadRuleBatchData park print

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

}
