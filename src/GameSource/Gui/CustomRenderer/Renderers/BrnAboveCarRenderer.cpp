#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "BrnAboveCarRenderer.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayGuiModuleSerialiser.h"
#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace BrnGui
{
namespace
{
// ARTIST82FB2E60 remains zero; image+direct/indexed/VMX alias scan found no writer.
const f32 KF_SCORE_BANKING_SHOWTIME_TARGET_X = 0.0f;
const char* const KAP_POSITION_STRINGS[8] = {
    "POSITION_FIRST_LOWERCASE", "POSITION_SECOND_LOWERCASE", "POSITION_THIRD_LOWERCASE",
    "POSITION_FOURTH_LOWERCASE", "POSITION_FIFTH_LOWERCASE", "POSITION_SIXTH_LOWERCASE",
    "POSITION_SEVENTH_LOWERCASE", "POSITION_EIGHTH_LOWERCASE"}; //82F27800
const char* const KAP_POSITION_FALLBACK[8] = {"$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8"}; //82F257F8
const CgsGraphics::RGBA KAU_ONLINE_COLOURS[12] = {
    0xFF3070FFu, 0xFF00FFFFu, 0xFF0000FFu, 0xFFFF6C00u,
    0xFFFF75FDu, 0xFF24E960u, 0xFF187FF4u, 0xFFFF3A97u,
    0xFFE2F91Bu, 0xFFFFFFFFu, 0xFF404040u, 0xFF000000u}; //82F25BE4; entry0 fixed by CRT82C51C20

const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* FindOnlinePlayer(
    const GuiCache* lpCache, EActiveRaceCarIndex leCar)
{
    for (s32 liIndex = 0; liIndex < 8; ++liIndex)
    {
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayer = lpCache->GetOnlinePlayerInfo(liIndex);
        if (lpPlayer->meActiveRaceCarIndex == leCar)
            return lpPlayer;
    }
    return 0;
}

f32 ClampAboveCarUnit(f32 lfValue)
{
    return lfValue < 0.0f ? 0.0f : (lfValue > 1.0f ? 1.0f : lfValue);
}

f32 AboveCarFontHeight(f32 lfFov, f32 lfDistanceFraction)
{
    const f32 lfHalfAngle = lfFov * 0.5f;
    const f32 lfSin = static_cast<f32>(std::sin(static_cast<f64>(lfHalfAngle)));
    const f32 lfCos = static_cast<f32>(std::cos(static_cast<f64>(lfHalfAngle)));
    const Vector4 a(9.5f, 9.5f, 9.5f, 9.5f);
    const f32 f = ClampAboveCarUnit((lfSin / lfCos) * lfDistanceFraction);
    const Vector4 b(f, f, f, f), c(0.5f, 0.5f, 0.5f, 0.5f);
    return Vector4(__vmaddfp(a.mV, b.mV, c.mV)).GetX().GetFloat();
}

void SetAboveCarText(CgsGraphics::TextObject& lrText, const CgsResource::CgsUtf8* lpString,
                    f32 lfHeight, f32 lfBottom, CgsGraphics::RGBA luColour, f32 lfWidth)
{
    lrText.mpUtf8String = lpString;
    if (lrText.mbAutosize) lrText.CalculateAutosizing();
    lrText.mfFontHeight = lfHeight;
    lrText.mTextColour = luColour;
    lrText.mv2TopLeft.mX = 0.0f; lrText.mv2TopLeft.mY = lfBottom - lfHeight;
    lrText.mv2BottomRight.mX = 0.0f; lrText.mv2BottomRight.mY = lfBottom;
    lrText.meAlignment = CgsGraphics::TextObject::E_ALIGNMENT_CENTER;
    lrText.mfStringWidth = lfWidth;
    if (lrText.mbAutosize) lrText.CalculateAutosizing();
}
}

// ARTIST82454B50.
void AboveCarRenderer::UpdateCachedInfoForRival(EActiveRaceCarIndex leIndex)
{
    PlayerGamerTagAboveCarInfo& lrInfo = maCachedPlayerGameTagInfos[leIndex];
    lrInfo.mbUsed = true;
    char lacName[KI_RIVALNAME_LENGTH] = {0};
    CgsGraphics::RGBA luColour = 0xFF3070FFu;
    const u32 luPosition = mpGuiCache->GetEventPositionOfRaceCar(leIndex);
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayer = FindOnlinePlayer(mpGuiCache, leIndex);
    if (lpPlayer)
    {
        const char* lpcName = lpPlayer->mPlayerName.GetPlayerName();
        CGS_ASSERT(std::strlen(lpcName) < KI_RIVALNAME_LENGTH, "Name is too long for the64 byte buffer");
        CgsCore::SPrintf(lacName, KI_RIVALNAME_LENGTH, "%s", lpcName);
        lacName[KI_RIVALNAME_LENGTH-1] = 0;
        switch (mpGuiCache->GetGameMode())
        {
        case 10: case 12: case 14: case 15: case 17:
            luColour = KAU_ONLINE_COLOURS[mpGuiCache->GetOnlinePlayerColourFromARCI(leIndex)];
            break;
        case 11: case 13:
            if (mpGuiCache->GetCurrentOnlinePlayerTeam(leIndex) == 1) luColour = 0xFF0040FFu;
            else if (mpGuiCache->GetCurrentOnlinePlayerTeam(leIndex) == 2) luColour = 0xFFFF4000u;
            break;
        default: break;
        }
    }
    CGS_ASSERT(std::strlen(lacName) < KI_RIVALNAME_LENGTH, "Name is too long for the64 byte buffer");
    CgsCore::SPrintf(lrInfo.macRivalName, KI_RIVALNAME_LENGTH, "%s", lacName);
    lrInfo.mpRivalName = reinterpret_cast<const CgsResource::CgsUtf8*>(lrInfo.macRivalName);
    lrInfo.mfNameStringWidth = mTextObject.mpFont->GetStringWidth(lrInfo.mpRivalName);
    lrInfo.mColour = luColour;
    lrInfo.muRivalPosition = 0;
    if (mpGuiCache->GetGameMode() == 10)
    {
        CGS_ASSERT(luPosition < 9, "luRivalPosition < 9");
        if (luPosition && luPosition <= mpGuiCache->GetOpponentsInEvent()+1)
        {
            lrInfo.muRivalPosition = luPosition;
            const CgsResource::CgsUtf8* lpText = mpLanguageManager->FindString(KAP_POSITION_STRINGS[luPosition-1]);
            if (!lpText) lpText = reinterpret_cast<const CgsResource::CgsUtf8*>(KAP_POSITION_FALLBACK[luPosition-1]);
            const char* lpcText = reinterpret_cast<const char*>(lpText);
            CGS_ASSERT(std::strlen(lpcText) < KI_POSITION_LENGTH, "Position is too long for the16 byte buffer");
            lrInfo.mfPositionStringWidth = mTextObject.mpFont->GetStringWidth(lpText);
            CgsCore::SPrintf(lrInfo.macPositionText, KI_POSITION_LENGTH, "%s", lpcText);
            lrInfo.mpPositionText = reinterpret_cast<const CgsResource::CgsUtf8*>(lrInfo.macPositionText);
        }
        else
        {
            CGS_ASSERT(false, "Invalid rival position for active race car");
            lrInfo.mpPositionText = reinterpret_cast<const CgsResource::CgsUtf8*>("???");
        }
    }
}

// ARTIST82455068 inlines general affine inverse, then a camera-facing basis.
// The billboard's Y axis points down, matching the text renderer's glyph coordinates.
f32 AboveCarRenderer::SetTransformMatrixForCar(CgsGui::ImRendererSet* lpRendererSet,
                                               Vector3 lvPosition)
{
    const Matrix44& lrView = lpRendererSet->mCamera.mView;
    const rw::math::vpu::Matrix44Affine lView(Vector3(lrView.xAxis.mV), Vector3(lrView.yAxis.mV),
        Vector3(lrView.zAxis.mV), Vector3(lrView.wAxis.mV));
    const rw::math::vpu::Matrix44Affine lInverse = rw::math::vpu::Inverse(lView);
    const Vector3 lvDirection(lvPosition.mV.x - lInverse.wAxis.mV.x,
        lvPosition.mV.y - lInverse.wAxis.mV.y, lvPosition.mV.z - lInverse.wAxis.mV.z);
    const f32 lfDistance = std::sqrt(rw::math::vpu::Dot(lvDirection, lvDirection));
    const Vector3 lvForward(lvDirection.mV.x / lfDistance, lvDirection.mV.y / lfDistance,
                            lvDirection.mV.z / lfDistance);
    const Vector3 lvCross = rw::math::vpu::Cross(lvForward, lInverse.yAxis);
    const f32 lfCrossLength = std::sqrt(rw::math::vpu::Dot(lvCross, lvCross));
    const Vector3 lvRight(lvCross.mV.x / lfCrossLength, lvCross.mV.y / lfCrossLength,
                          lvCross.mV.z / lfCrossLength);
    const Vector3 lvUp = rw::math::vpu::Cross(lvRight, lvForward);
    const Matrix44 lModel(Vector4(lvRight.mV.x, lvRight.mV.y, lvRight.mV.z, 0.0f),
        Vector4(-lvUp.mV.x, -lvUp.mV.y, -lvUp.mV.z, 0.0f),
        Vector4(lvForward.mV.x, lvForward.mV.y, lvForward.mV.z, 0.0f),
        Vector4(lvPosition.mV.x, lvPosition.mV.y, lvPosition.mV.z, 1.0f));
    lpRendererSet->mpIm3dRenderBufferRacePosition->SetTransform(
        lModel, lpRendererSet->mCamera.mViewProjection);
    return lfDistance;
}

// ARTIST82454388. Member order comes from DecFIGS plus the ARTIST replay extension.
void AboveCarRenderer::Construct()
{
    CustomRenderComponentInterface::Construct();
    mePrepareStage = E_PREPARESTAGE_START;
    meReleaseStage = E_RELEASESTAGE_START;
    mpHeapAllocator = 0;
    mpBlendState = 0;
    maBankingScores.Clear(); // stw0,+4A0; Array<BankingScore,6> count
    mRecentCrashSet.UnSetAll();
    mTextObject.Construct(0, 0);
    mpScoreFont.Clear();
    mpTextRenderer = 0;
    mpLanguageManager = 0;
    mpGuiCache = 0;
    mpGuiModuleSerialiser = 0;
    for (s32 liIndex = 0; liIndex < 8; ++liIndex)
    {
        PlayerGamerTagAboveCarInfo& lrInfo = maCachedPlayerGameTagInfos[liIndex];
        lrInfo.mbUsed = false;
        lrInfo.macRivalName[0] = 0;
        lrInfo.mpRivalName = 0;
        lrInfo.macPositionText[0] = 0;
        lrInfo.mpPositionText = 0;
        maAboveCarObjectLayouts[liIndex].Clear();
    }
    mbTimeExtensionPending = false;
    miAboveCarRendererPM = -1;
    miAboveCarRendererPM = CgsDev::PerfMonCpu::AddMonitor("AboveCarRenderer", CgsDev::E_PMP_3, false, 2.0f, false);
    CGS_ASSERT(miAboveCarRendererPM >= 0, "miAboveCarRendererPM >= 0");
}

// ARTIST824469A8 stores r5: the first allocator, after the event-queue argument.
bool AboveCarRenderer::Prepare(CgsGui::GuiEventQueueSmall*,
                               rw::IResourceAllocator* lpHeapAllocator,
                               rw::IResourceAllocator*)
{
    if (mePrepareStage == E_PREPARESTAGE_START)
        mpHeapAllocator = lpHeapAllocator;
    else if (mePrepareStage != E_PREPARESTAGE_DONE)
    {
        CGS_ASSERT(false, " unknown prepare stage in AboveCarRenderer ");
        return false;
    }
    mePrepareStage = E_PREPARESTAGE_DONE;
    return true;
}

// ARTIST82446A58: allocator vtable+14 frees all lanes of the blend resource.
bool AboveCarRenderer::Release()
{
    if (meReleaseStage == E_RELEASESTAGE_START)
    {
        if (mpBlendState)
            mpHeapAllocator->DoFree(mBlendStateResource);
    }
    else if (meReleaseStage != E_RELEASESTAGE_DONE)
    {
        CGS_ASSERT(false, " unknown release stage in AboveCarRenderer ");
        return false;
    }
    meReleaseStage = E_RELEASESTAGE_DONE;
    return true;
}

CgsID AboveCarRenderer::GetID() const
{
    return 0x4DCEFF7CA8C29C00ULL; // ARTIST82446BE8 includes both halves.
}

void AboveCarRenderer::Update()
{
    using BrnReplays::BaseSerialiser;
    if (!mpGuiModuleSerialiser) return; // ARTIST82446B40..44
    const BaseSerialiser::EMode leMode = mpGuiModuleSerialiser->GetMode();
    const bool lbRecording = leMode == BaseSerialiser::E_MODE_RECORDING_PREPARING ||
        leMode == BaseSerialiser::E_MODE_RECORDING ||
        leMode == BaseSerialiser::E_MODE_RECORDING_STALLED;
    const bool lbPlaying = leMode == BaseSerialiser::E_MODE_PLAYING_PREPARING ||
        leMode == BaseSerialiser::E_MODE_PLAYING ||
        leMode == BaseSerialiser::E_MODE_PLAYING_STALLED;
    if (lbRecording || lbPlaying)
    {
        BrnReplays::GuiModuleStaticLayout* lpLayout = mpGuiModuleSerialiser->GetStaticLayout();
        CGS_STATIC_ASSERT((sizeof(maAboveCarObjectLayouts) == sizeof(lpLayout->maCarRecordsA)),
                      "Replay copies eight complete32-byte records");
        if (lbRecording)
            std::memcpy(lpLayout->maCarRecordsA, maAboveCarObjectLayouts, sizeof(maAboveCarObjectLayouts));
        else
            std::memcpy(maAboveCarObjectLayouts, lpLayout->maCarRecordsA, sizeof(maAboveCarObjectLayouts));
    }
}

// ARTIST824544D8. Events are payloads, without a prepended GuiEvent header.
void AboveCarRenderer::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
{
    CGS_ASSERT(lpEvent, " null event passed ");
    switch (liEventType)
    {
    case 14:
    {
        const CgsGui::GuiEventLoadNotification* lpNotification = static_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent);
        CgsResource::SafeResourceHandle<CgsResource::Font> lFont;
        lFont.mpResourceMemory = lpNotification->mResourceHandle.mpResourceMemory;
        lFont.mpSourceEntry = lpNotification->mResourceHandle.mpSourceEntry;
        CGS_ASSERT(lFont.mpResourceMemory && *static_cast<void**>(lFont.mpResourceMemory),
                   "Invalid resource data sent AboveCarRenderer::RecvEvent");
        if (lpNotification->meRequestType == 16)
        {
            CGS_ASSERT(!lFont.IsNull(), "lpFont != CgsResource::NULLResourceHandle");
            const char* lpcFamily = lFont->macTypefaceFamilyName;
            if (mTextObject.mpFont.IsNull())
            {
                const char* lpcDefault = mpLanguageManager->GetDefaultFont();
                if (_strnicmp(lpcFamily, lpcDefault, std::strlen(lpcDefault)) == 0)
                    mTextObject.mpFont = lFont;
            }
            if (_strnicmp(lpcFamily, "B5EAConDisS", 11) == 0)
                mTextObject.mpFont = lFont;
            if (mpScoreFont.IsNull())
            {
                const char* lpcDefault = mpLanguageManager->GetDefaultFont();
                if (_strnicmp(lpcFamily, lpcDefault, std::strlen(lpcDefault)) == 0)
                    mpScoreFont = lFont;
            }
            if (_strnicmp(lpcFamily, "B5DOTMAT", 8) == 0)
                mpScoreFont = lFont;
        }
        break;
    }
    case 64:
        mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
        CGS_ASSERT(mpGuiCache, "Invalid GUI cache pointer");
        break;
    case 209:
    {
        const Array<u16, 25>& lrRemoved = reinterpret_cast<const GuiRemovedTrafficEvent*>(lpEvent)->mRemovedTrafficArray;
        for (u32 luIndex = 0; luIndex < lrRemoved.GetLength(); ++luIndex)
        {
            const u16 luVehicle = lrRemoved.GetItem(luIndex);
            CGS_ASSERT(luVehicle < 600, "luIndex < NUMBITS");
            mRecentCrashSet.UnSetBit(luVehicle);
        }
        break;
    }
    case 377:
        if (reinterpret_cast<const GuiPlayerCrashingStateChangeEvent*>(lpEvent)->meCurrentState ==
            GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_LEAVE_TAKEDOWN &&
            mbTimeExtensionPending)
        {
            if (!maBankingScores.IsFull())
            {
                BankingScore lScore;
                lScore.mv2ScreenSpacePosition = Vector2(0.0f, 400.0f);
                lScore.mv3OriginalWorldSpacePosition.SetZero();
                lScore.miBaseScore = miTimeExtension;
                lScore.miComboBonus = 0;
                lScore.mbIsRoadRageTimeExtension = true;
                maBankingScores.Append(lScore);
            }
            mbTimeExtensionPending = false;
        }
        break;
    case 394:
    {
        const GuiHitVehicleEvent* lpHit = reinterpret_cast<const GuiHitVehicleEvent*>(lpEvent);
        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        if (!maBankingScores.IsFull())
        {
            const u32 luCount = mpGuiCache->GetScoringTrafficCount();
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                const BrnTraffic::BrnTrafficIO::VehicleScoreData* lpCar = mpGuiCache->GetScoringTrafficData(luIndex);
                if (lpCar->muVehicleIndex == lpHit->muVehicleIndex)
                {
                    BankingScore lScore;
                    lScore.mv2ScreenSpacePosition.SetZero();
                    lScore.mv3OriginalWorldSpacePosition = lpCar->mPosition;
                    lScore.miBaseScore = static_cast<s16>(lpHit->miVehicleBaseScore);
                    lScore.miComboBonus = static_cast<s16>(lpHit->miVehicleChainBonus);
                    lScore.mbIsRoadRageTimeExtension = false;
                    maBankingScores.Append(lScore);
                }
            }
        }
        CGS_ASSERT(lpHit->muVehicleIndex < 600, "Index < Number of bits");
        mRecentCrashSet.SetBit(lpHit->muVehicleIndex);
        break;
    }
    case 427:
        mbTimeExtensionPending = true;
        miTimeExtension = static_cast<s16>(*reinterpret_cast<const u32*>(lpEvent));
        break;
    }
}

// ARTIST8245B860: world-space score labels and grouped overhead-sign bonuses.
void AboveCarRenderer::RenderTrafficCarScores(CgsGui::ImRendererSet* lpRendererSet)
{
    const s32 liMode = mpGuiCache->GetGameMode();
    if (liMode != 2 && liMode != 16)
    {
        mRecentCrashSet.UnSetAll();
        return;
    }

    CgsGraphics::Im3dRenderBuffer* lpBuffer = lpRendererSet->mpIm3dRenderBufferRacePosition;
    const CgsResource::SafeResourceHandle<CgsResource::Font> lPreviousFont = mTextObject.mpFont;
    mTextObject.mpFont = mpScoreFont;
    mTextObject.mfFontHeight = 0.5f; // 82F257B8
    mTextObject.mbDropShadow = true;
    mTextObject.mTextColour = mpLanguageManager->GetCurrentLanguage() == 16 ? 0xFF00B4FFu : 0xFFFFFFFFu;
    // CRT82C51BE4 initializes82FB35C0 to1280,720.
    mTextObject.mv2TopLeft.mX = -1280.0f; mTextObject.mv2TopLeft.mY = -0.5f;
    mTextObject.mv2BottomRight.mX = 1280.0f; mTextObject.mv2BottomRight.mY = 720.0f;
    mTextObject.meAlignment = CgsGraphics::TextObject::E_ALIGNMENT_CENTER;
    CgsGraphics::Camera& lrCamera = lpRendererSet->mCamera;
    const f32 lfNearClip = lrCamera.maProjectionScalars[7];
    lrCamera.maProjectionScalars[7] *= 1.1f;
    lrCamera.UpdatePerspectiveProjectionMatrix();

    char lacScore[32];
    for (u32 luIndex = 0; luIndex < mpGuiCache->GetScoringTrafficCount(); ++luIndex)
    {
        const BrnTraffic::BrnTrafficIO::VehicleScoreData* lpCar = mpGuiCache->GetScoringTrafficData(luIndex);
        CGS_ASSERT(lpCar->muVehicleIndex < 600, "invalid index < NUMBITS");
        if (mRecentCrashSet.IsBitSet(lpCar->muVehicleIndex))
            continue;
        SetTransformMatrixForCar(lpRendererSet, lpCar->mPosition);
        mpLanguageManager->FormatText(lacScore, 31, static_cast<s32>(lpCar->miScore),
            CgsLanguage::LanguageManager::E_FORMAT_MONEY);
        mTextObject.mpUtf8String = reinterpret_cast<const CgsResource::CgsUtf8*>(lacScore);
        if (mTextObject.mbAutosize) mTextObject.CalculateAutosizing();
        mTextObject.mfStringWidth = mTextObject.mpFont->GetStringWidth(mTextObject.mpUtf8String);
        if (mTextObject.mbAutosize) mTextObject.CalculateAutosizing();
        mpTextRenderer->RenderString(lpBuffer, mTextObject);
        if (lpCar->miMultiplier > 0)
        {
            CgsCore::SnPrintf(lacScore, 31, "+%d", static_cast<s32>(lpCar->miMultiplier)); //82057CD4
            const CgsGraphics::TextVector2 lvTopLeft = mTextObject.mv2TopLeft;
            mTextObject.mv2TopLeft.mY -= 1.0f;
            mTextObject.mfFontHeight = 1.0f;
            mTextObject.mfStringWidth = mTextObject.mpFont->GetStringWidth(mTextObject.mpUtf8String);
            if (mTextObject.mbAutosize) mTextObject.CalculateAutosizing();
            mpTextRenderer->RenderString(lpBuffer, mTextObject);
            mTextObject.mv2TopLeft = lvTopLeft;
            mTextObject.mfFontHeight = 0.5f;
        }
    }

    Vector3 laCentres[32];
    Vector3 laSums[32];
    f32 lafCounts[32];
    u32 luGroups = 0;
    const GuiOverheadSignInfoEvent::VisibleOverheadSignArray& lrSigns = *mpGuiCache->GetVisibleOverheadSignArray();
    for (u32 luIndex = 0; luIndex < lrSigns.GetLength(); ++luIndex)
    {
        const Vector3 lvPosition = lrSigns.GetItem(luIndex).mWorldSpacePosition;
        u32 luGroup = 0;
        for (; luGroup < luGroups; ++luGroup)
        {
            const f32 lfX = lvPosition.mV.x - laCentres[luGroup].mV.x;
            const f32 lfZ = lvPosition.mV.z - laCentres[luGroup].mV.z;
            // CRT82C51C10 splats225; vrlimi clears Y before the dot product.
            if (lfX * lfX + lfZ * lfZ < 225.0f)
                break;
        }
        if (luGroup < luGroups)
        {
            lafCounts[luGroup] += 1.0f;
            laSums[luGroup].mV.x += lvPosition.mV.x;
            laSums[luGroup].mV.y += lvPosition.mV.y;
            laSums[luGroup].mV.z += lvPosition.mV.z;
            laSums[luGroup].mV.w += lvPosition.mV.w;
            const f32 lfReciprocal = 1.0f / lafCounts[luGroup];
            laCentres[luGroup].mV = __vmulfp(laSums[luGroup].mV, __vspltw(Vector4(lfReciprocal, lfReciprocal, lfReciprocal, lfReciprocal).mV, 0));
        }
        else if (luGroups < 32)
        {
            laCentres[luGroups] = laSums[luGroups] = lvPosition;
            lafCounts[luGroups++] = 1.0f;
        }
    }
    mpLanguageManager->FormatText(lacScore, 31, 10000,
        CgsLanguage::LanguageManager::E_FORMAT_MONEY); // 82F257EC
    mTextObject.mpUtf8String = reinterpret_cast<const CgsResource::CgsUtf8*>(lacScore);
    if (mTextObject.mbAutosize) mTextObject.CalculateAutosizing();
    mTextObject.mfFontHeight = 1.0f;
    mTextObject.mfStringWidth = mTextObject.mpFont->GetStringWidth(mTextObject.mpUtf8String);
    if (mTextObject.mbAutosize) mTextObject.CalculateAutosizing();
    for (u32 luGroup = 0; luGroup < luGroups; ++luGroup)
    {
        SetTransformMatrixForCar(lpRendererSet, laCentres[luGroup]);
        mpTextRenderer->RenderString(lpBuffer, mTextObject);
    }
    mTextObject.mpFont = lPreviousFont;
    mTextObject.mbDropShadow = false;
    lrCamera.maProjectionScalars[7] = lfNearClip;
    lrCamera.UpdatePerspectiveProjectionMatrix();
}

// ARTIST8245B3A0: playback uses the previous frame's double-buffered gamertags.
void AboveCarRenderer::RenderReplayAboveCar(CgsGui::ImRendererSet* lpRendererSet)
{
    if (!mpGuiCache->GetRenderReplayPlayerNames())
        return;
    const BrnReplays::GuiModuleStaticLayout* lpLayout = mpGuiModuleSerialiser->GetStaticLayout();
    for (s32 liIndex = 0; liIndex < 8; ++liIndex)
    {
        const BrnReplays::GuiModuleAboveCarObjectLayout& lrRecord = maAboveCarObjectLayouts[liIndex];
        if (!lrRecord.mbVisible) continue;
        Vector3 lvPosition = lrRecord.mWorldPosition;
        lvPosition.mV.y += 1.0f;
        const f32 lfDistance = SetTransformMatrixForCar(lpRendererSet, lvPosition);
        const f32 lfFraction = ClampAboveCarUnit(lfDistance * 0.004f);
        if (lfFraction >= 1.0f) continue;
        const f32 lfHeight = AboveCarFontHeight(lpRendererSet->mCamera.maProjectionScalars[3], lfFraction);
        f32 lfBottom = -0.4f;
        CgsGraphics::Im3dRenderBuffer* lpBuffer = lpRendererSet->mpIm3dRenderBufferRacePosition;
        if (lrRecord.muRacePosition)
        {
            const CgsResource::CgsUtf8* lpText = mpLanguageManager->FindString(KAP_POSITION_STRINGS[lrRecord.muRacePosition-1]);
            CGS_ASSERT(lpText, "lpPositionString");
            SetAboveCarText(mTextObject, lpText, lfHeight, lfBottom, lrRecord.mColour,
                mTextObject.mpFont->GetStringWidth(lpText));
            mpTextRenderer->RenderString(lpBuffer, mTextObject);
            lfBottom -= lfHeight;
        }
        const u8 (*lpNames)[16] = lpLayout->mbFrameParity ? lpLayout->maCarFlagsBuffer0 : lpLayout->maCarFlagsBuffer1;
        const CgsResource::CgsUtf8* lpName = reinterpret_cast<const CgsResource::CgsUtf8*>(lpNames[liIndex]);
        CGS_ASSERT(lpName, "lpStaticLayout->maaGamertags[lpStaticLayout->GetGamertagBufferIndexLastFrame()][leActiveRaceCarIndex].GetPlayerName()");
        SetAboveCarText(mTextObject, lpName, lfHeight, lfBottom, lrRecord.mColour,
            mTextObject.mpFont->GetStringWidth(lpName));
        mpTextRenderer->RenderString(lpBuffer, mTextObject);
    }
}

// ARTIST82462A38: one 3D batch, online names or replay names, then traffic and banking.
void AboveCarRenderer::RenderComponent(CgsGui::ImRendererSet* lpRendererSet)
{
    CGS_ASSERT(mpLanguageManager, "mpLanguageManager");
    if (!mpGuiCache || mTextObject.mpFont.IsNull() || mpScoreFont.IsNull())
        return;
    if (miAboveCarRendererPM >= 0) CgsDev::PerfMonCpu::StartMonitor(miAboveCarRendererPM);
    CgsGraphics::Im3dRenderBuffer* lpBuffer = lpRendererSet->mpIm3dRenderBufferRacePosition;
    lpBuffer->BeginRendering();
    lpBuffer->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpRasterizerState_CullNone);
    lpBuffer->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpDepthStencilState_ZBufferOn);
    if (!mpBlendState)
    {
        renderengine::BlendStateParameters lParams;
        for (u32 i = 0; i != 4; ++i) lParams.maBlendFactor[i] = 0x07060706u;
        lParams.muState15 = 4;
        lParams.muState4 = 7;
        lParams.muState5 = lParams.muState6 = lParams.muState7 = 15;
        lParams.muState8 = 135;
        lParams.muState9 = 0xFFFFFFFFu;
        lParams.mbHasCustomBlendFactors = 1;
        lParams.mbState16 = 1;
        const rw::ResourceDescriptor lDescriptor = renderengine::BlendState::GetResourceDescriptor(lParams);
        mBlendStateResource = mpHeapAllocator->DoAllocate(lDescriptor, 0);
        mpBlendState = renderengine::BlendState::Initialize(mBlendStateResource, lParams);
    }
    lpBuffer->SetState(reinterpret_cast<const renderengine::BlendState*>(mpBlendState));
    const BrnReplays::BaseSerialiser::EMode leReplayMode = mpGuiModuleSerialiser->GetMode();
    if (leReplayMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING_PREPARING ||
        leReplayMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING ||
        leReplayMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING_STALLED)
        RenderReplayAboveCar(lpRendererSet);
    else
    {
        if (mpGuiCache->IsOnline())
        {
            for (s32 liIndex = 0; liIndex < 8; ++liIndex)
            {
                const EActiveRaceCarIndex leCar = static_cast<EActiveRaceCarIndex>(liIndex);
                PlayerGamerTagAboveCarInfo& lrInfo = maCachedPlayerGameTagInfos[liIndex];
                if (!mpGuiCache->IsActiveRaceCarIndexUsed(leCar))
                {
                    lrInfo.mbUsed = false;
                    continue;
                }
                if (liIndex == mpGuiCache->GetPlayerActiveRaceCarIndex() ||
                    mpGuiCache->IsRaceCarCrashing(leCar) || mpGuiCache->IsActiveRaceCarConnecting(leCar) ||
                    mpGuiCache->IsActiveRaceCarDisconnected(leCar) || mpGuiCache->GetOnlinePlayerInCarSelect(leCar) ||
                    mpGuiCache->GetOnlinePlayerColourFromARCI(leCar) == 10)
                    continue;
                const Vector4& lrPosition = mpGuiCache->GetRaceCarPosition(leCar);
                Vector3 lvPosition(lrPosition.mV); lvPosition.mV.y += 1.0f;
                const f32 lfDistance = SetTransformMatrixForCar(lpRendererSet, lvPosition);
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayer = FindOnlinePlayer(mpGuiCache, leCar);
                if (!lpPlayer) lrInfo.mbUsed = false;
                else if (!lrInfo.mbUsed || lrInfo.muRivalPosition != mpGuiCache->GetEventPositionOfRaceCar(leCar) ||
                         _stricmp(lpPlayer->mPlayerName.GetPlayerName(), lrInfo.macRivalName))
                    UpdateCachedInfoForRival(leCar);
                if (!lrInfo.mbUsed) continue;
                BrnReplays::GuiModuleAboveCarObjectLayout& lrRecord = maAboveCarObjectLayouts[liIndex];
                lrRecord.mWorldPosition = Vector3(lrPosition.mV);
                const f32 lfFraction = ClampAboveCarUnit(lfDistance * 0.004f);
                if (lfFraction >= 1.0f) continue;
                if (lfFraction > 0.75f)
                {
                    const u32 luAlpha = static_cast<u8>((1.0f - lfFraction) * 1020.0f);
                    lrInfo.mColour = (lrInfo.mColour & 0x00FFFFFFu) | (luAlpha << 24);
                }
                lrRecord.mColour = lrInfo.mColour;
                const f32 lfHeight = AboveCarFontHeight(lpRendererSet->mCamera.maProjectionScalars[3], lfFraction);
                f32 lfBottom = -0.4f;
                const s32 liMode = mpGuiCache->GetGameMode();
                if ((liMode == 0 || liMode == 10) && lrInfo.muRivalPosition &&
                    lrInfo.muRivalPosition <= mpGuiCache->GetOpponentsInEvent() + 1)
                {
                    lrRecord.muRacePosition = lrInfo.muRivalPosition;
                    lrRecord.mbVisible = true;
                    CGS_ASSERT(lrInfo.mpPositionText, "maCachedPlayerGameTagInfos[leActiveRaceCarIndex].mpPositionText");
                    SetAboveCarText(mTextObject, lrInfo.mpPositionText, lfHeight, lfBottom,
                                    lrInfo.mColour, lrInfo.mfPositionStringWidth);
                    mpTextRenderer->RenderString(lpBuffer, mTextObject);
                    lfBottom -= lfHeight;
                }
                if (mpTextRenderer && mpGuiCache->IsOnline())
                {
                    lrRecord.mbVisible = true;
                    CGS_ASSERT(lrInfo.mpRivalName, "maCachedPlayerGameTagInfos[leActiveRaceCarIndex].mpRivalName");
                    SetAboveCarText(mTextObject, lrInfo.mpRivalName, lfHeight, lfBottom,
                                    lrInfo.mColour, lrInfo.mfNameStringWidth);
                    mpTextRenderer->RenderString(lpBuffer, mTextObject);
                }
            }
        }
        RenderTrafficCarScores(lpRendererSet);
        RenderBankingScores(lpRendererSet);
    }
    lpBuffer->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpBlendState_Standard);
    lpBuffer->EndRendering();
    if (miAboveCarRendererPM >= 0) CgsDev::PerfMonCpu::StopMonitor(miAboveCarRendererPM);
}
// ARTIST8245BF28; DecFIGS BrnAboveCarRenderer.{h,cpp} and BrnGuiUnity locals.
// Required canonical declarations are listed in BankingScores.review.md.
void AboveCarRenderer::RenderBankingScores(CgsGui::ImRendererSet* lpImRenderers)
{
    const s32 liMode = mpGuiCache->GetGameMode();
    if (liMode != 2 && liMode != 16 && liMode != 3)
    {
        maBankingScores.Clear();
        mbTimeExtensionPending = false;
        return;
    }

    const CgsResource::SafeResourceHandle<CgsResource::Font> lpSavedFont = mTextObject.mpFont;
    mTextObject.mpFont = mpScoreFont;
    mTextObject.mfFontHeight = 0.5f; //82F257B8 KF_SCORE_FONT_SIZE_3D
    mTextObject.mbDropShadow = true;
    mTextObject.mTextColour = mpLanguageManager->GetCurrentLanguage() == 16
        ? 0xFF00B4FFu : 0xFFFFFFFFu; //82F25BA8/82F25BA4
    mTextObject.mv2TopLeft.mX = -1280.0f; mTextObject.mv2TopLeft.mY = -0.5f;
    mTextObject.mv2BottomRight.mX = 1280.0f; mTextObject.mv2BottomRight.mY = 720.0f; //82FB35C0, CRT82C51BE4
    mTextObject.meAlignment = CgsGraphics::TextObject::E_ALIGNMENT_CENTER;

    CgsGraphics::Camera& lrCamera = lpImRenderers->mCamera;
    const f32 lfSavedNear = lrCamera.maProjectionScalars[7]; //ARTIST camera+15C
    lrCamera.SetNearClipPlane(lfSavedNear * 1.1f); //82F257E4
    CGS_ASSERT((maBankingScores.GetCount() != Array<BankingScore,6>::KI_UNCONSTRUCTED),
               "Array used before Construct/Clear was called");
    u32 luCount = maBankingScores.GetLength();
    if (luCount)
    {
        CgsGraphics::Im2dRenderBuffer* lpIm2d = lpImRenderers->mpIm2dRenderBuffer;
        lpIm2d->BeginRendering();
        lpIm2d->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpRasterizerState_CullNone); //83010F3C
        lpIm2d->SetState(reinterpret_cast<const renderengine::BlendState*>(mpBlendState));
        lpIm2d->SetTexture(CgsGraphics::ImRendererBase::GetStateLibrary().mpTexture_White); //83010F58, real shared white raster
        CgsGraphics::Im2dTransform lTransform;
        lTransform.mOriginXYZ.Set(-1.0f, 1.0f, 0.0f, 0.0f);
        lTransform.mRightUp.Set(2.0f/1280.0f, 0.0f, 0.0f, -2.0f/720.0f);
        lTransform.mColourShift.Set(0.0f, 0.0f, 0.0f, 0.0f);
        lTransform.mColourScale.Set(1.0f, 1.0f, 1.0f, 1.0f);
        lpIm2d->SetTransform(lTransform); //raw VMX oracle pins all16 lanes
        mTextObject.mfFontHeight = 20.0f; //82F257C0 KF_SCORE_FONT_SIZE_2D
        mTextObject.mv2BottomRight.mX = 1280.0f; mTextObject.mv2BottomRight.mY = 720.0f;
        mTextObject.meAlignment = CgsGraphics::TextObject::E_ALIGNMENT_CENTER;

        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            BankingScore& lrScore = maBankingScores.GetItem(luIndex);
            const f32 lfTargetX = lrScore.mbIsRoadRageTimeExtension
                ? -620.0f : KF_SCORE_BANKING_SHOWTIME_TARGET_X; //82F257D4/82FB2E60
            const f32 lfTargetY = lrScore.mbIsRoadRageTimeExtension
                ? 200.0f : 140.0f; //82F257DC/82F257D8
            const f32 lfSpeed = lrScore.mbIsRoadRageTimeExtension
                ? 0.05f : 0.05f; //82F257CC/82F257C8; separate original constants
            const Vector3 lvWorld = lrScore.mv3OriginalWorldSpacePosition;
            // ARTIST compares abs XYZ with FLT_EPSILON; w is replaced with x.
            if (std::fabs(lvWorld.mV.x) > 1.1920928955078125e-7f ||
                std::fabs(lvWorld.mV.y) > 1.1920928955078125e-7f ||
                std::fabs(lvWorld.mV.z) > 1.1920928955078125e-7f)
            {
                Vector2& lvScreen = lrScore.mv2ScreenSpacePosition;
                if (BrnDirector::Camera::Utils::ProjectWorldSpacePointToScreen(
                        lrCamera.GetViewProjectionMatrix(), lvWorld, lvScreen) &&
                    lvScreen.mV.x >= -0.9f && lvScreen.mV.x <= 0.9f &&
                    lvScreen.mV.y >= -1.0f && lvScreen.mV.y <= 1.0f)
                {
                    lvScreen.mV.x *= 1280.0f;
                    // Separate vector operations in ARTIST: add, mul, mul, sub.
                    lvScreen.mV.y = ((lvScreen.mV.y + 1.0f) * 0.5f) * 720.0f - 10.0f;
                }
                else
                {
                    lvScreen.mV.x = lfTargetX;
                    lvScreen.mV.y = lfTargetY;
                }
                lrScore.mv3OriginalWorldSpacePosition.SetZero(); //83018100
            }

            CgsResource::CgsUtf8 lauText[32];
            if (lrScore.miComboBonus > 0)
            {
                mTextObject.mbMultiLine = 1;
                char lacAmount[32];
                char lacCombo[32];
                mpLanguageManager->FormatCurrencyString(
                    lacAmount, lrScore.miBaseScore + lrScore.miComboBonus, 32);
                CgsCore::SnPrintf(lacCombo, 32, "%d", lrScore.miComboBonus / 1000); //82F257E8
                lacCombo[31] = 0;
                const CgsResource::CgsUtf8* lpFormat = mpLanguageManager->FindString("SHOWTIME_COMBO"); //82F257F0
                CgsUnicode::UnicodeBuffer lCombo;
                CgsUnicode::UnicodeBuffer lAmount;
                lCombo.Convert(reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacCombo));
                lAmount.Convert(reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacAmount));
                const CgsUnicode::CgsUtf8* lapArgs[2] = {lCombo.GetBuffer(), lAmount.GetBuffer()};
                CgsUnicode::_Print(lauText, lpFormat, 32, lapArgs, 2);
            }
            else if (lrScore.mbIsRoadRageTimeExtension)
            {
                char lacSeconds[32];
                CgsCore::SnPrintf(lacSeconds, 32, "%d", lrScore.miBaseScore);
                lacSeconds[31] = 0;
                const CgsResource::CgsUtf8* lpFormat = mpLanguageManager->FindString("TIME_EXTENSION_SECONDS"); //82F257F4
                CgsUnicode::UnicodeBuffer lSeconds;
                lSeconds.Convert(reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacSeconds));
                const CgsUnicode::CgsUtf8* lapArgs[1] = {lSeconds.GetBuffer()};
                CgsUnicode::_Print(lauText, lpFormat, 32, lapArgs, 1);
            }
            else
            {
                mpLanguageManager->FormatCurrencyString(
                    reinterpret_cast<char*>(lauText), lrScore.miBaseScore, 32);
            }

            const f32 lfDeltaX = lfTargetX - lrScore.mv2ScreenSpacePosition.mV.x;
            const f32 lfDeltaY = lfTargetY - lrScore.mv2ScreenSpacePosition.mV.y;
            mTextObject.mpUtf8String = lauText;
            // fmuls x*x then fmadds y*y+x*x; fsel clamps include unordered FP.
            const Vector4 y(lfDeltaY, lfDeltaY, lfDeltaY, lfDeltaY);
            const f32 xx = lfDeltaX * lfDeltaX;
            const Vector4 x2(xx, xx, xx, xx);
            const f32 lfDistanceSquared = Vector4(__vmaddfp(y.mV, y.mV, x2.mV)).GetX().GetFloat();
            f32 lfFade = lfDistanceSquared / (90.0f*90.0f); //82F257E0
            lfFade = -lfFade >= 0.0f ? 0.0f : lfFade;
            lfFade = 1.0f-lfFade >= 0.0f ? lfFade : 1.0f;
            const u8 luAlpha = static_cast<u8>(lfFade * 255.0f); //82010C20, fctidz
            if (mTextObject.mbAutosize) mTextObject.CalculateAutosizing();
            mTextObject.mv2TopLeft.mX = lrScore.mv2ScreenSpacePosition.mV.x; mTextObject.mv2TopLeft.mY = lrScore.mv2ScreenSpacePosition.mV.y;
            mTextObject.mfStringWidth = mTextObject.mpFont->GetStringWidth(mTextObject.mpUtf8String);
            if (mTextObject.mbAutosize) mTextObject.CalculateAutosizing();
            mTextObject.mTextColour = (mTextObject.mTextColour & 0x00FFFFFFu) |
                                     (static_cast<u32>(luAlpha) << 24);
            mpTextRenderer->RenderString(lpIm2d, mTextObject);
            mTextObject.mbMultiLine = 0;
            // bgt tests: unordered deltas follow the erase branch in ARTIST.
            if (!(std::fabs(lfDeltaX) > 5.0f) && !(std::fabs(lfDeltaY) > 5.0f)) //82F257C4
            {
                maBankingScores.EraseFast(luIndex);
                --luIndex;
                --luCount;
            }
            else
            {
                const f32 lfStepX = lfDeltaX * lfSpeed;
                const f32 lfStepY = lfDeltaY * lfSpeed;
                lrScore.mv2ScreenSpacePosition.mV.x += lfStepX;
                lrScore.mv2ScreenSpacePosition.mV.y += lfStepY;
            }
        }
        lpIm2d->EndRendering();
    }
    mTextObject.mpFont = lpSavedFont;
    mTextObject.mbDropShadow = false;
    lrCamera.SetNearClipPlane(lfSavedNear);
}

}
