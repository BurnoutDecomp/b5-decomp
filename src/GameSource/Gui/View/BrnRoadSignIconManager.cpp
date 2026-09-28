#include "GameSource/Gui/View/BrnRoadSignIconManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the BRN_MAPICON_DIAG witness
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"               // CgsIDCompress
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"          // CgsGui::GuiEventWrapper (the 562 record)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"   // StateInterface::GetOutputEventQueue
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.h" // CgsGui::ObjectController
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue::AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                      // GuiCache::IsHighDefinition / GetActiveRoadRuleScoringMode
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // GuiEventRoadSignIconStatus / GuiEventRoadRuleBatchDataResponse
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"            // MapTransform::WorldToDevice

#include <cstdlib>   // getenv (BRN_MAPICON_DIAG)

// BrnGui::RoadSignIcon / BrnGui::RoadSignIconManager (the asserts name the original home,
// GameSource/Gui/SatNav/BrnRoadSignIconManager.cpp). Every body follows the console asm; the
// asserts are the console's own strings and, as on the console, none of them gates.

namespace BrnGui
{
namespace
{
    // [FLAG PC witness] BRN_MAPICON_DIAG: the road-sign pool prepare / publish counts, first-N.
    bool IsMapIconDiagOn()
    {
        static const bool sbDiag = (getenv("BRN_MAPICON_DIAG") != 0);
        return sbDiag;
    }
    const s32 KI_MAPICON_DIAG_MAX_LINES = 8;

    // The output GUI-event channel (the AddEvent channel OutputGuiEvent posts on).
    const s32 KI_CHANNEL_GUI_EVENT_OUT = 40;
}

    // ================================ RoadSignIcon ================================

    const char* RoadSignIcon::KAPC_SIGN_COLOURS[RoadSignIcon::E_SIGN_COLOUR_COUNT] =
    {
        "Green",    // E_SIGN_COLOUR_GREEN
        "Red",      // E_SIGN_COLOUR_RED
        "Silver",   // E_SIGN_COLOUR_SILVER
        "Gold",     // E_SIGN_COLOUR_GOLD
    };
    const char* RoadSignIcon::mpRoadPrefix      = "RD_";
    const char* RoadSignIcon::mpShowSign        = "ShowPost";
    const char* RoadSignIcon::mpTrue            = "true";
    const char* RoadSignIcon::mpFalse           = "false";
    const char* RoadSignIcon::mpacRoadFrameName = "SignName";
    const char* RoadSignIcon::mpacRoadColour    = "colour";

    // The component construct, then the sign defaults in the console's store order.
    void RoadSignIcon::Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                                 const char* lpacParentName, bool lbCommunicateWithApt)
    {
        CGS_ASSERT(lacName != 0, "lacName");                     // assert line 73
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");   // assert line 74

        CgsGui::GuiComponent::Construct(lacName, lpStateInterface, lpacParentName);

        mbCommunicateWithApt = lbCommunicateWithApt;
        mRoadId              = 0;
        mv4WorldPosition.x   = 0.0f;
        mv4WorldPosition.y   = 0.0f;
        mv4WorldPosition.z   = 0.0f;
        mv4WorldPosition.w   = 0.0f;
        meSignColour         = E_SIGN_COLOUR_GREEN;
        meIconType           = E_ICON_TYPE_COUNT;
        mbSignVisible        = false;
        mbOfflineTimeRuled   = false;
        mbOfflineCrashRuled  = false;
        mbOnlineTimeRuled    = false;
        mbOnlineCrashRuled   = false;
    }

    void RoadSignIcon::SetIcon(ERoadIcon leIcon, const Vector2* lpv2WorldPos, EIconType leIconType,
                               CgsID lRoadId)
    {
        CGS_ASSERT(leIcon >= 0 && leIcon < E_ROADICON_COUNT,
                   "leIcon >= 0 && leIcon < E_ROADICON_COUNT");   // assert line 182

        mv4WorldPosition.x = lpv2WorldPos->x;
        mv4WorldPosition.y = lpv2WorldPos->y;
        mv4WorldPosition.z = lpv2WorldPos->z;
        mv4WorldPosition.w = lpv2WorldPos->w;
        meIconType         = leIconType;
        mRoadId            = lRoadId;
    }

    // Drive "_visible" only on a change, and only when this sign talks to apt.
    void RoadSignIcon::SetVisible(bool lbVisible)
    {
        if (lbVisible != mbSignVisible)
        {
            if (mbCommunicateWithApt)
            {
                AddOutputAptViewState("_visible", lbVisible ? "true" : "false", true);
            }
            mbSignVisible = lbVisible;
        }
    }

    // "RD_<road id>" is the sign's frame label for that road.
    void RoadSignIcon::DisplayRoad(ERoadIcon leIcon, bool lbShowPost)
    {
        CGS_ASSERT(static_cast<u32>(leIcon) < static_cast<u32>(E_ROADICON_COUNT),
                   "Unable to find a valid icon name");   // assert line 203

        char lacRoadName[32];
        CgsCore::SPrintf(lacRoadName, sizeof(lacRoadName), "%s%s", mpRoadPrefix,
                         gapcRoadIconNames[leIcon]);
        DisplayRoad(lacRoadName, lbShowPost);
    }

    void RoadSignIcon::DisplayRoad(const char* lpcRoadName, bool lbShowPost)
    {
        CGS_ASSERT(lpcRoadName != 0, "NULL != lpcRoadName");         // assert line 224
        CGS_ASSERT('\0' != lpcRoadName[0], "'\\0' != lpcRoadName[0]");  // assert line 225

        if (mbCommunicateWithApt)
        {
            AddOutputAptViewState(mpacRoadFrameName, lpcRoadName, false);
        }
        if (mbCommunicateWithApt)
        {
            AddOutputAptViewState(mpShowSign, lbShowPost ? mpTrue : mpFalse, false);
        }
        SetVisible(true);
    }

    // A linear search of the 64 icon names; the console open-codes the strcmp.
    ERoadIcon RoadSignIcon::FindRoadFromName(const char* lpIconName)
    {
        CGS_ASSERT(lpIconName != 0, "lpIconName");   // assert line 334

        s32 liIcon = 0;
        for (; liIcon < E_ROADICON_COUNT; ++liIcon)
        {
            const char* lpcName = gapcRoadIconNames[liIcon];
            const char* lpcFind = lpIconName;
            s32 liDelta = 0;
            for (;;)
            {
                liDelta = static_cast<s32>(*lpcFind) - static_cast<s32>(*lpcName);
                if (*lpcFind == 0 || liDelta != 0)
                    break;
                ++lpcFind;
                ++lpcName;
            }
            if (liDelta == 0)
                break;
        }

        CGS_ASSERT(liIcon < E_ROADICON_COUNT,
                   "Unable to find a valid icon name ( looking for ");   // assert line 346

        return static_cast<ERoadIcon>(liIcon);
    }

    // The two lanes go to apt as "%3.3f" strings.
    void RoadSignIcon::SetScreenPosition(Vector2 lv2ScreenPos)
    {
        if (mbCommunicateWithApt)
        {
            char lacValue[32];
            CgsCore::SPrintf(lacValue, sizeof(lacValue), "%3.3f", static_cast<double>(lv2ScreenPos.x));
            AddOutputAptViewState("_x", lacValue, true);
            CgsCore::SPrintf(lacValue, sizeof(lacValue), "%3.3f", static_cast<double>(lv2ScreenPos.y));
            AddOutputAptViewState("_y", lacValue, true);
        }
    }

    // The colour is stored even when it trips the range assert.
    void RoadSignIcon::SetColour(ESignColour leColour)
    {
        CGS_ASSERT(static_cast<u32>(leColour) < static_cast<u32>(E_SIGN_COLOUR_COUNT),
                   "Invalid colour (");   // assert line 313

        meSignColour = leColour;
        if (mbCommunicateWithApt)
        {
            AddOutputAptViewState(mpacRoadColour, KAPC_SIGN_COLOURS[leColour], false);
        }
    }

    void RoadSignIcon::SetScale(Vector2 lv2Scale)
    {
        if (mbCommunicateWithApt)
        {
            char lacValue[32];
            CgsCore::SPrintf(lacValue, sizeof(lacValue), "%3.3f", static_cast<double>(lv2Scale.x));
            AddOutputAptViewState("_xscale", lacValue, true);
            CgsCore::SPrintf(lacValue, sizeof(lacValue), "%3.3f", static_cast<double>(lv2Scale.y));
            AddOutputAptViewState("_yscale", lacValue, true);
        }
    }

    // ============================= RoadSignIconManager =============================

    // KF_ZOOM_RANGE is not stored in the image: Update multiplies by its folded reciprocal
    // (1.6666666f, bits 0x3FD55555 == 1.0f / 0.6f), and KF_ZOOM_ADJUST's stored bits
    // (0x3ECCCCCC) are exactly 1.0f - 0.6f. FLAG: which of the two 30/55 pairs is the
    // VIEW_LERP pair and which the SCALE_FACTOR pair is inferred from Update's locals (the lerp
    // result is lfScaleLerp, the clamped value lfScaleFactor); the values are the image's.
    const f32 RoadSignIconManager::KF_ZOOM_RANGE       = 0.6f;
    const f32 RoadSignIconManager::KF_ZOOM_ADJUST      = 0.39999998f;
    const f32 RoadSignIconManager::KF_MIN_SCALE_FACTOR = 30.0f;
    const f32 RoadSignIconManager::KF_MAX_SCALE_FACTOR = 55.0f;
    const f32 RoadSignIconManager::KF_MIN_VIEW_LERP    = 30.0f;
    const f32 RoadSignIconManager::KF_MAX_VIEW_LERP    = 55.0f;
    f32       RoadSignIconManager::KF_SD_MIN_SCALE_FACTOR = 30.0f;
    f32       RoadSignIconManager::KF_SD_MAX_SCALE_FACTOR = 85.0f;
    // The console's static initialiser copies these two from the SD scale pair at startup.
    f32       RoadSignIconManager::KF_SD_MIN_VIEW_LERP    = RoadSignIconManager::KF_SD_MIN_SCALE_FACTOR;
    f32       RoadSignIconManager::KF_SD_MAX_VIEW_LERP    = RoadSignIconManager::KF_SD_MAX_SCALE_FACTOR;

    // mbComponentVisible is left alone, as the console leaves it.
    void RoadSignIconManager::Construct()
    {
        for (u32 luIcon = 0; luIcon < KU_NUM_SIGN_ICONS; ++luIcon)
        {
            mapObjectController[luIcon] = 0;
        }
        mpGuiCache         = 0;
        mbIconsTransformed = false;
        mpStateInterface   = 0;
        meRoadRuleType     = 0;
        mfZoomFactor       = 1.0f;
    }

    // One sign per icon name, each wrapped in a freshly allocated apt ObjectController.
    void RoadSignIconManager::Prepare(CgsGui::StateInterface* lpStateInterface, GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");   // assert line 382
        CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");               // assert line 383

        for (u32 luIcon = 0; luIcon < KU_NUM_SIGN_ICONS; ++luIcon)
        {
            mIcons[luIcon].Construct(gapcRoadIconNames[luIcon], lpStateInterface, 0, false);

            CgsGui::ObjectController* lpController = new CgsGui::ObjectController();
            CGS_ASSERT(lpController != 0,
                       "Unable to allocate space for the ObjectController in the Apt cache");   // assert line 391
            mapObjectController[luIcon] = lpController;
            lpController->SetControlledObject(&mIcons[luIcon]);
            lpController->Register();
        }

        mpGuiCache         = lpGuiCache;
        mpStateInterface   = lpStateInterface;
        mbIconsTransformed = false;
        mbComponentVisible = false;

        // [FLAG PC witness] BRN_MAPICON_DIAG, first-N.
        static s32 siPrepareLines = 0;
        if (IsMapIconDiagOn() && siPrepareLines < KI_MAPICON_DIAG_MAX_LINES && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siPrepareLines;
            *CgsDev::Log::gpDebugPrint << "[mapicon] road signs prepared: " << static_cast<s32>(KU_NUM_SIGN_ICONS)
                                       << " components, controllers registered\n";
        }
    }

    // The controllers are unregistered and dropped; the console never deletes them here.
    void RoadSignIconManager::ReleaseResources()
    {
        for (u32 luIcon = 0; luIcon < KU_NUM_SIGN_ICONS; ++luIcon)
        {
            if (mapObjectController[luIcon] != 0)
            {
                mapObjectController[luIcon]->UnRegister();
                mapObjectController[luIcon] = 0;
            }
        }
    }

    // The zoom-driven sign scale, then per sign: screen position, scale, and the road-rule
    // colour for the current scoring mode; finally the bank goes to the renderer.
    void RoadSignIconManager::Update()
    {
        // Every clamp below is the console's fsel pair: the lower bound wins when the value is
        // at or below it, the upper bound when the value is above it or unordered.
        f32 lZoomProp = (mfZoomFactor - KF_ZOOM_ADJUST) * (1.0f / KF_ZOOM_RANGE);
        if (lZoomProp <= 0.0f)
            lZoomProp = 0.0f;
        if (!(lZoomProp <= 1.0f))
            lZoomProp = 1.0f;

        f32 lfMinViewLerp;
        f32 lfMaxViewLerp;
        f32 lfMinScaleFactor;
        f32 lfMaxScaleFactor;
        if (mpGuiCache->IsHighDefinition())
        {
            lfMinViewLerp    = KF_MIN_VIEW_LERP;
            lfMaxViewLerp    = KF_MAX_VIEW_LERP;
            lfMinScaleFactor = KF_MIN_SCALE_FACTOR;
            lfMaxScaleFactor = KF_MAX_SCALE_FACTOR;
        }
        else
        {
            lfMinViewLerp    = KF_SD_MIN_VIEW_LERP;
            lfMaxViewLerp    = KF_SD_MAX_VIEW_LERP;
            lfMinScaleFactor = KF_SD_MIN_SCALE_FACTOR;
            lfMaxScaleFactor = KF_SD_MAX_SCALE_FACTOR;
        }

        const f32 lfScaleLerp = lfMinViewLerp + (lfMaxViewLerp - lfMinViewLerp) * lZoomProp;

        f32 lfScaleFactor = lfScaleLerp;
        if (lfScaleFactor <= lfMinScaleFactor)
            lfScaleFactor = lfMinScaleFactor;
        if (!(lfScaleFactor <= lfMaxScaleFactor))
            lfScaleFactor = lfMaxScaleFactor;

        Vector2 lv2ScaleFactor;
        lv2ScaleFactor.x = lfScaleFactor;
        lv2ScaleFactor.y = lfScaleFactor;
        lv2ScaleFactor.z = 0.0f;
        lv2ScaleFactor.w = 0.0f;

        for (u32 luIndex = 0; luIndex < KU_NUM_SIGN_ICONS; ++luIndex)
        {
            RoadSignIcon* lpIcon = &mIcons[luIndex];

            // The 2D world lane unflattened to {x, 0, z}.
            Vector3 lv3World;
            lv3World.x = lpIcon->mv4WorldPosition.x;
            lv3World.y = 0.0f;
            lv3World.z = lpIcon->mv4WorldPosition.y;
            lv3World.w = 0.0f;
            const Vector2 lv2ScreenPos = MapTransform::WorldToDevice(lv3World, false);
            lpIcon->SetScreenPosition(lv2ScreenPos);
            lpIcon->SetScale(lv2ScaleFactor);

            bool lbTimeRuled;
            bool lbCrashRuled;
            if (mpGuiCache->GetActiveRoadRuleScoringMode() == 0)   // E_ROAD_PANEL_MODE_OFFLINE
            {
                lbTimeRuled  = lpIcon->mbOfflineTimeRuled;
                lbCrashRuled = lpIcon->mbOfflineCrashRuled;
            }
            else
            {
                CGS_ASSERT(mpGuiCache->GetActiveRoadRuleScoringMode() == 1,
                           "mpGuiCache->GetActiveRoadRuleScoringMode() == "
                           "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE");   // assert line 477
                lbTimeRuled  = lpIcon->mbOnlineTimeRuled;
                lbCrashRuled = lpIcon->mbOnlineCrashRuled;
            }

            // Both rules held: gold. One held: silver when it is the rule being shown, red
            // otherwise. Neither: red. (Score type 0 is time, 1 crash.)
            if (lbTimeRuled && lbCrashRuled)
            {
                lpIcon->meSignColour = RoadSignIcon::E_SIGN_COLOUR_GOLD;
            }
            else if (lbTimeRuled)
            {
                lpIcon->meSignColour = (meRoadRuleType == 0) ? RoadSignIcon::E_SIGN_COLOUR_SILVER
                                                             : RoadSignIcon::E_SIGN_COLOUR_RED;
            }
            else if (lbCrashRuled)
            {
                lpIcon->meSignColour = (meRoadRuleType == 1) ? RoadSignIcon::E_SIGN_COLOUR_SILVER
                                                             : RoadSignIcon::E_SIGN_COLOUR_RED;
            }
            else
            {
                lpIcon->meSignColour = RoadSignIcon::E_SIGN_COLOUR_RED;
            }
            lpIcon->SetColour(lpIcon->meSignColour);
        }

        if (mpStateInterface != 0)
        {
            // Console record {8, 562, 12, &mIcons[0], lfScaleFactor * 0.01f}, 20 bytes on
            // channel 40; the host wrapper carries its own size and payload offset.
            GuiEventRoadSignIconStatus lRoadSignStatus;
            lRoadSignStatus.mpRoadSignIcons = &mIcons[0];
            lRoadSignStatus.mfScaleFactor   = lfScaleFactor * 0.01f;

            CgsGui::GuiEventWrapper<GuiEventRoadSignIconStatus, KI_CHANNEL_GUI_EVENT_OUT> lRecord(lRoadSignStatus);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord),
                lRecord.GetChannel(), static_cast<s32>(sizeof(lRecord)));

            // [FLAG PC witness] BRN_MAPICON_DIAG, first-N.
            static s32 siUpdateLines = 0;
            if (IsMapIconDiagOn() && siUpdateLines < KI_MAPICON_DIAG_MAX_LINES && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siUpdateLines;
                *CgsDev::Log::gpDebugPrint << "[mapicon] road signs published: " << static_cast<s32>(KU_NUM_SIGN_ICONS)
                                           << " scale=" << lRoadSignStatus.mfScaleFactor << "\n";
            }
        }
    }

    // Per road in the batch: find its sign by the decimal road id and copy the four ruled
    // flags. FindRoadFromName reads no member, so the console calls it on the pool base; a
    // road with no sign is asserted there and its index (64) is used unchecked, as on the
    // console.
    void RoadSignIconManager::SetRoadRuleBatchData(const GuiEventRoadRuleBatchDataResponse* lpRoadRules)
    {
        CGS_ASSERT(lpRoadRules != 0, "lpRoadRules");   // assert line 677

        for (s32 liRoadRuleIndex = 0; liRoadRuleIndex < lpRoadRules->miRoadCount; ++liRoadRuleIndex)
        {
            CGS_ASSERT(lpRoadRules->maRoadIds[liRoadRuleIndex] != 0,
                       "lpRoadRules->maRoadIds[ liRoadRuleIndex ] != kCGSID_NULL");   // assert line 683

            char lacRoadNameString[32];
            CgsCore::SPrintf(lacRoadNameString, 31, "%llu",
                             static_cast<unsigned long long>(lpRoadRules->maRoadIds[liRoadRuleIndex]));
            lacRoadNameString[31] = '\0';

            const ERoadIcon leRoadIcon = mIcons[0].FindRoadFromName(lacRoadNameString);

            // FLAG PC safety: a road outside the 64-name table comes back as the count; the
            // console writes one icon past the array into the controller pointers that follow.
            if (static_cast<u32>(leRoadIcon) >= KU_NUM_SIGN_ICONS)
            {
                continue;
            }

            RoadSignIcon& lrIcon = mIcons[leRoadIcon];
            lrIcon.mbOfflineTimeRuled  = lpRoadRules->mabPlayerBeatenOfflineTime[liRoadRuleIndex];
            lrIcon.mbOfflineCrashRuled = lpRoadRules->mabPlayerBeatenOfflineCrash[liRoadRuleIndex];
            lrIcon.mbOnlineTimeRuled   = lpRoadRules->mabPlayerBeatenOnlineTime[liRoadRuleIndex];
            lrIcon.mbOnlineCrashRuled  = lpRoadRules->mabPlayerBeatenOnlineCrash[liRoadRuleIndex];
        }
    }

    // Skipped entirely when the value already matches the master flag; each sign's own flag
    // (and, for apt-bound signs, its "_visible" state) only on a change.
    void RoadSignIconManager::SetSignsVisible(bool lbVisible)
    {
        if (lbVisible == mbComponentVisible)
            return;

        for (u32 luIcon = 0; luIcon < KU_NUM_SIGN_ICONS; ++luIcon)
        {
            RoadSignIcon& lrIcon = mIcons[luIcon];
            if (lbVisible != lrIcon.mbSignVisible)
            {
                if (lrIcon.mbCommunicateWithApt)
                {
                    lrIcon.AddOutputAptViewState("_visible", lbVisible ? "true" : "false", true);
                }
                lrIcon.mbSignVisible = lbVisible;
            }
        }

        mbComponentVisible = lbVisible;
    }

    // Bind every sign to its apt object. The first pass reads each sign's authored stage
    // position (a 2048 x 2048 stage) off its controller and maps it into the world rect; later
    // passes keep the position already stored. Every sign then gets its road id, its "RD_"
    // artwork with the post shown, and is made visible.
    void RoadSignIconManager::SetupComponent()
    {
        Vector4 lv4StageRect;
        lv4StageRect.x = 0.0f;
        lv4StageRect.y = 0.0f;
        lv4StageRect.z = 2048.0f;
        lv4StageRect.w = 2048.0f;

        for (s32 liIndex = 0; liIndex < GetNumIcons(); ++liIndex)
        {
            RoadSignIcon&             lrIcon       = mIcons[liIndex];
            CgsGui::ObjectController* lpController = mapObjectController[liIndex];
            const CgsID               lRoadId      = CgsIDCompress(lrIcon.GetName());

            Vector2 lv2WorldPos;
            if (mbIconsTransformed)
            {
                lv2WorldPos.x = lrIcon.mv4WorldPosition.x;
                lv2WorldPos.y = lrIcon.mv4WorldPosition.y;
                lv2WorldPos.z = lrIcon.mv4WorldPosition.z;
                lv2WorldPos.w = lrIcon.mv4WorldPosition.w;
            }
            else
            {
                lv2WorldPos = MapTransform::Transform(lpController->GetPos(), lv4StageRect,
                                                      MapTransform::GetWorldRect());
            }

            const ERoadIcon leIcon = static_cast<ERoadIcon>(liIndex);
            lrIcon.SetIcon(leIcon, &lv2WorldPos, RoadSignIcon::E_ICON_TYPE_A, lRoadId);
            lrIcon.DisplayRoad(leIcon, true);
            lpController->SetObjectVariableBoolean("_visible", true);
        }

        mbComponentVisible = true;
        mbIconsTransformed = true;

        // [FLAG PC witness] BRN_MAPICON_DIAG, first-N: the first sign's world lanes.
        static s32 siSetupLines = 0;
        if (IsMapIconDiagOn() && siSetupLines < KI_MAPICON_DIAG_MAX_LINES && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siSetupLines;
            *CgsDev::Log::gpDebugPrint << "[mapicon] road signs set up: " << GetNumIcons()
                                       << " sign0 world=" << mIcons[0].mv4WorldPosition.x
                                       << "," << mIcons[0].mv4WorldPosition.y << "\n";
        }
    }

    // Register the 64 sign names as expected screen components and queue each controller to
    // attach to its apt object on ONLOAD. The controlled list is cleared first.
    void RoadSignIconManager::AppendExpectedComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // assert line 538

        mpGuiCache->ClearExpectedControlledAptComponentList();

        for (s32 liIndex = 0; liIndex < GetNumIcons(); ++liIndex)
        {
            CGS_ASSERT(mapObjectController[liIndex] != 0,
                       "mapObjectController[liIndex]");   // assert line 545

            const char* lpcName = mIcons[liIndex].GetName();
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, lpcName);
            mpGuiCache->AppendExpectedControlledAptComponent(lpcName, mapObjectController[liIndex]);
        }
    }

    // The sign component's own name buffer.
    const char* RoadSignIconManager::GetIconNameAtIndex(u32 luIndex) const
    {
        CGS_ASSERT(luIndex < KU_NUM_SIGN_ICONS, "Invalid index");   // assert line 586

        return mIcons[luIndex].GetName();
    }

    // Every sign's world lane unflattened to {x, 0, z} and taken to device space; the count
    // is the literal 64 the console stores.
    void RoadSignIconManager::GetRoadSignIconPositions(Vector2* lpav2Positions,
                                                       s32* lpiNumIcons) const
    {
        CGS_ASSERT(lpiNumIcons != 0, "lpiNumIcons");   // assert line 564

        for (u32 luIcon = 0; luIcon < KU_NUM_SIGN_ICONS; ++luIcon)
        {
            const Vector4& lv4Lane = mIcons[luIcon].mv4WorldPosition;

            Vector3 lv3World;
            lv3World.x = lv4Lane.x;
            lv3World.y = 0.0f;
            lv3World.z = lv4Lane.y;
            lv3World.w = 0.0f;

            lpav2Positions[luIcon] = MapTransform::WorldToDevice(lv3World, false);
        }

        *lpiNumIcons = static_cast<s32>(KU_NUM_SIGN_ICONS);
    }

    // A change-guarded single store.
    void RoadSignIconManager::SetRoadIconFilter(s32 leRoadRuleType)
    {
        if (meRoadRuleType != leRoadRuleType)
        {
            meRoadRuleType = leRoadRuleType;
        }
    }

}
