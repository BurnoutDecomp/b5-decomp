#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameSource/Replays/BrnGuiModuleAboveCarObjectLayout.h"
#include "rw/rwcore_structs.h"

namespace CgsLanguage { class LanguageManager; }
namespace BrnReplays { class GuiModuleSerialiser; }
namespace renderengine { struct BlendMaterialState; }

namespace BrnGui
{
class GuiCache;
const s32 KI_MAX_BANKING_SCORES = 6;
const s32 KI_MAX_RECENTLY_HIT_CARS = 64;
const s32 KI_RIVALNAME_LENGTH = 64;
const s32 KI_POSITION_LENGTH = 16;

// DecFIGS BrnAboveCarRenderer.h:43; ARTIST append/erase stride48.
struct BankingScore
{
    Vector2 mv2ScreenSpacePosition;
    Vector3 mv3OriginalWorldSpacePosition;
    s16 miBaseScore;
    s16 miComboBonus;
    bool mbIsRoadRageTimeExtension;
};

struct PlayerGamerTagAboveCarInfo
{
    bool mbUsed;
    char macRivalName[KI_RIVALNAME_LENGTH];
    const CgsResource::CgsUtf8* mpRivalName;
    char macPositionText[KI_POSITION_LENGTH];
    const CgsResource::CgsUtf8* mpPositionText;
    CgsGraphics::RGBA mColour;
    f32 mfNameStringWidth;
    f32 mfPositionStringWidth;
    u32 muRivalPosition;
};

// ARTIST vtable820CF8D8; declaration shape from DecFIGS.
class AboveCarRenderer : public CgsGui::CustomRenderComponentInterface
{
public:
    enum EPrepareStage { E_PREPARESTAGE_START, E_PREPARESTAGE_DONE };
    enum EReleaseStage { E_RELEASESTAGE_START, E_RELEASESTAGE_DONE };

    void Construct() override;
    bool Prepare(CgsGui::GuiEventQueueSmall*, rw::IResourceAllocator*,
                 rw::IResourceAllocator*) override;
    bool Release() override;
    void RecvEvent(const CgsModule::Event*, s32 liEventType) override;
    void Update() override;
    CgsID GetID() const override;
    s32 GetNumTextures() const override { return 1; }
    void SetTextRenderer(CgsGraphics::TextRenderer* lpRenderer) { mpTextRenderer = lpRenderer; }
    void SetLanguageManager(CgsLanguage::LanguageManager* lpManager) { mpLanguageManager = lpManager; }
    void SetReplaySerialiser(BrnReplays::GuiModuleSerialiser* lpSerialiser) { mpGuiModuleSerialiser = lpSerialiser; }

private:
    void RenderComponent(CgsGui::ImRendererSet*) override;
    void RenderTrafficCarScores(CgsGui::ImRendererSet*);
    void RenderBankingScores(CgsGui::ImRendererSet*);
    void RenderReplayAboveCar(CgsGui::ImRendererSet*);
    void UpdateCachedInfoForRival(EActiveRaceCarIndex leIndex);
    f32 SetTransformMatrixForCar(CgsGui::ImRendererSet*, Vector3 lvPosition);

    EPrepareStage mePrepareStage;
    EReleaseStage meReleaseStage;
    rw::IResourceAllocator* mpHeapAllocator;
    PlayerGamerTagAboveCarInfo maCachedPlayerGameTagInfos[8];
    CgsResource::SafeResourceHandle<CgsResource::Font> mpScoreFont;
    Array<BankingScore, KI_MAX_BANKING_SCORES> maBankingScores;
    CgsContainers::BitArray<601u> mRecentCrashSet;
    rw::Resource mBlendStateResource;
    renderengine::BlendMaterialState* mpBlendState;
    CgsGraphics::TextObject mTextObject;
    CgsGraphics::TextRenderer* mpTextRenderer;
    CgsLanguage::LanguageManager* mpLanguageManager;
    GuiCache* mpGuiCache;
    BrnReplays::GuiModuleSerialiser* mpGuiModuleSerialiser;
    // ARTIST extends DecFIGS with eight32-byte replay records at+5B0.
    BrnReplays::GuiModuleAboveCarObjectLayout maAboveCarObjectLayouts[8];
    s16 miTimeExtension;
    bool mbTimeExtensionPending;
    s32 miAboveCarRendererPM;
};
}
