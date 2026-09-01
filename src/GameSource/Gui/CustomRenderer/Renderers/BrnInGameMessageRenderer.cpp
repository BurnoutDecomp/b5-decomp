// ============================================================================
// b5-decomp/src/GameSource/Gui/CustomRenderer/Renderers/BrnInGameMessageRenderer.cpp
//
// ⭐⭐ [tut-ticker] BrnGui::InGameMessageRenderer -- the bottom-of-screen ticker,
// reconstructed WHOLE (2026-08-24) from the X360 ARTIST set listed in the header.
// This is the consumer half of the training-ticker chain: GUI event 537
// (GuiEventTickerCustomMessage) lands in RecvEvent, queues an InGameMessage, and
// RenderComponent's state machine fades the black band in, scrolls the text with
// the ticker font, and fades out when the queue drains.
//
// SUBSTRATE (all pre-existing, the gateui waves'): CgsGraphics::TextObject +
// TextRenderer (CgsFontRenderer.cpp), the ImRenderBuffer<V> command API
// (SetTransform/SetTexture/RenderEnd), CgsUnicode's CopyN/_Print/Print<> family,
// CgsResource::Font::GetStringWidth.
//
// RODATA -- READ FROM THE IMAGE (x360 id1 reader, 2026-08-24; range-12 placement
// anchored on the recovered training string table, range-0 on the timed-tip tables):
//   flt_82054F40  K_MESSAGE_SCROLL_SPEED[8] = { -200 x5, -300 (custom), -400 (autosave), -200 }
//   flt_82F25818  the shared ticker FONT HEIGHT   (ships 24.0; wide-glyph language -> 28.0)
//   flt_82F2581C  the shared ticker TEXT Y        (ships 631.0; wide-glyph language -> 629.0)
//   flt_82F25BB0  the shared message gap          (ships 100.0; AddNewMessage recomputes it)
//   dword_82F25BB4 K_BACKGROUND_COLOURS_BLACK     (0xFF000000 -- black, alpha animated per frame)
//   dword_82F25BC0/BC4 the text colours           (0xFFFFFFFF white / 0xFF00B4FF orange, the
//                                                  wide-glyph language's variant)
// ⚠️ CORRECTED IDENTITY: the 2026-08-16 SetLanguageManager slice named flt_82F25818/1C
// "the banner X/Y position". DrawMessages stores flt_82F25818 into mTextObject.mfFontHeight
// (TO +0x08) every draw and ResetYPos stores flt_82F2581C into mfTextPosY -- they are the
// FONT HEIGHT and the TEXT Y. The wide-glyph override (28/629) is unchanged.
// ============================================================================

#include "GameSource/Gui/CustomRenderer/Renderers/BrnInGameMessageRenderer.h"

#include "GameSource/Gui/BrnGuiCache.h"                       // GuiCache (mode/road-rule/controller reads)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // LanguageManager (FindString / GetDefaultFont / GetCurrentLanguage)
#include "GameShared/GameClasses/Fonts/CgsFont.h"             // CgsResource::Font (GetStringWidth family)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h" // Im2dRenderBuffer (== Im2d)
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> command API
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"        // CgsGuiModuleIO::ImRendererSet (AptRenderHandler.h needs it complete)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h" // CgsGui::AptIm2dRenderBuffer (the view set's slot 0)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // GuiEventLoadNotification (RecvEvent case 14)
#include "GameShared/GameClasses/Containers/CgsHash.h"        // CgsHash::CalculateHash
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // CgsDev::Log (the console's own gxMessageFilterFlags prints)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"               // GuiAudioTriggerEvent (the CodeTicker audio pings)

#include <cstring>   // strncmp/_strnicmp/strlen

namespace BrnGui
{
namespace
{
    // ---- the shared mutable ticker layout globals (one copy per build, as on console) ----
    // flt_82F25818 / flt_82F2581C / flt_82F25BB0 / dword_82F25BB4 -- see the file banner.
    f32 gfTickerFontHeight = 24.0f;      // flt_82F25818 (SetLanguageManager: 28.0 for language 16)
    f32 gfTickerTextPosY   = 631.0f;     // flt_82F2581C (SetLanguageManager: 629.0 for language 16)
    f32 gfTickerMessageGap = 100.0f;     // flt_82F25BB0 (AddNewMessage recomputes; floor 100.0)

    // dword_82F25BC0 / dword_82F25BC4 -- the two text colours (packed RGBA, PC byte order per
    // the lEmitVertex convention: r in the low byte). White, and the wide-glyph orange.
    const u32 KU_TEXT_COLOUR         = 0xFFFFFFFFu;   // dword_82F25BC0
    const u32 KU_TEXT_COLOUR_WIDE    = 0xFF00B4FFu;   // dword_82F25BC4 (r=FF g=B4 b=00 = orange)

    // flt_82054F40 -- per-ticker-mode scroll speed (px/s; negative = leftward).
    const f32 KAF_MESSAGE_SCROLL_SPEED[8] =
    { -200.0f, -200.0f, -200.0f, -200.0f, -200.0f, -300.0f, -400.0f, -200.0f };

    // The wide-glyph language id + its layout override (SetLanguageManager @0x82443FB0).
    const s32 KI_WIDE_GLYPH_LANGUAGE      = 16;
    const f32 KF_WIDE_GLYPH_FONT_HEIGHT   = 28.0f;    // flt_82038B1C
    const f32 KF_WIDE_GLYPH_TEXT_POS_Y    = 629.0f;   // flt_8205540C

    // RenderComponent's state constants (the X360 immediates).
    const f32 KF_MESSAGE_DELAY_TIME   = 0.80000001f;  // idle re-buffer cadence
    const f32 KF_MESSAGE_FADE_TIME    = 0.30000001f;  // fade in/out duration
    const f32 KF_MESSAGE_START_POS_X  = 1280.0f;
    const f32 KF_MESSAGE_HEIGHT_PAD   = 44.0f;        // the text box height the draw opens
    const u8  KU_BACKGROUND_MAX_ALPHA = 0x80;         // 128
    const u8  KU_MESSAGE_MAX_ALPHA    = 0xFF;

    // The ticker's component id (GetID @0x82446F58 returns the 32-bit constant; the console
    // compares component CgsIDs by this compressed value).
    const CgsID KID_INGAME_MESSAGE = static_cast<CgsID>(static_cast<u32>(-1174614848));
}

// ---------------------------------------------------------------------------
// InGameMessage::Construct -- the per-record zero-seed the owner Construct inlines
// (text[0], next, the seven flags, the width; the hash is left -- the console's own
// store set @0x82455308).
// ---------------------------------------------------------------------------
void InGameMessageRenderer::InGameMessage::Construct()
{
    macMessageText[0]    = 0;
    mpNextMessage        = 0;
    mbInUse              = false;
    mbIsPriorityMessage  = false;
    mbIsRoadRuleMessage  = false;
    mbIsCustomMessage    = false;
    mbIsTrainingMessage  = false;
    mbIsChallengeMessage = false;
    mbRepeats            = false;
    mfStringWidth        = 0.0f;
}

// ---------------------------------------------------------------------------
// InGameMessage::SetupMessage @0x8244B7A0 -- format the text (0..3 positional params
// through the CgsUnicode print family), hash it, latch the flags. Store map:
// +517 priority, +518 roadRule, +519 custom, +520 training, +521 challenge, +522 repeats.
// ---------------------------------------------------------------------------
void InGameMessageRenderer::InGameMessage::SetupMessage(
    bool lbPriority, bool lbRoadRule, bool lbCustom, bool lbTraining, bool lbChallenge,
    bool lbRepeats, const CgsUnicode::CgsUtf8* lpMessageText, s32 liNumParams,
    const CgsUnicode::CgsUtf8* lpParam1, const CgsUnicode::CgsUtf8* lpParam2,
    const CgsUnicode::CgsUtf8* lpParam3)
{
    CGS_ASSERT(lpMessageText != 0, "lpMessageText");   // :1978
    if (lpMessageText == 0)
    {
        return;
    }

    switch (liNumParams)
    {
        case 0:
            CgsUnicode::CopyN(macMessageText, lpMessageText, 512);
            break;

        case 1:
        {
            CGS_ASSERT(lpParam1 != 0, "lpParam1");     // :1995
            // The console converts param 1 through a UnicodeBuffer then _Prints it; the
            // one-arg Print<> wrapper is that exact pair.
            const CgsUnicode::CgsUtf8* lapArgs[1] = { lpParam1 };
            CgsUnicode::_Print(macMessageText, lpMessageText, 512, lapArgs, 1);
            break;
        }

        case 2:
        {
            CGS_ASSERT(lpParam1 != 0, "lpParam1");     // :2007
            CGS_ASSERT(lpParam2 != 0, "lpParam2");     // :2008
            const CgsUnicode::CgsUtf8* lapArgs[2] = { lpParam1, lpParam2 };
            CgsUnicode::_Print(macMessageText, lpMessageText, 512, lapArgs, 2);
            break;
        }

        case 3:
        {
            CGS_ASSERT(lpParam1 != 0, "lpParam1");     // :2021
            CGS_ASSERT(lpParam2 != 0, "lpParam2");     // :2022
            CGS_ASSERT(lpParam3 != 0, "lpParam3");     // :2023
            const CgsUnicode::CgsUtf8* lapArgs[3] = { lpParam1, lpParam2, lpParam3 };
            CgsUnicode::_Print(macMessageText, lpMessageText, 512, lapArgs, 3);
            break;
        }

        default:
            CGS_ASSERT(false,
                       "Unhandled number of params in InGameMessageRenderer::InGameMessage::SetupMessage\n"); // :2036
            break;
    }

    muStringHash = CgsContainers::CgsHash::CalculateHash(
        reinterpret_cast<char*>(macMessageText),
        static_cast<int>(std::strlen(reinterpret_cast<const char*>(macMessageText))));

    mbIsRoadRuleMessage  = lbRoadRule;    // +518
    mbIsCustomMessage    = lbCustom;      // +519
    mbIsTrainingMessage  = lbTraining;    // +520
    mpNextMessage        = 0;             // +512
    mbInUse              = true;          // +516
    mbIsPriorityMessage  = lbPriority;    // +517
    mbIsChallengeMessage = lbChallenge;   // +521
    mbRepeats            = lbRepeats;     // +522
}

// ---------------------------------------------------------------------------
// Construct @0x82455308.
// ---------------------------------------------------------------------------
void InGameMessageRenderer::Construct()
{
    CgsGui::CustomRenderComponentInterface::Construct();

    mfTimeRemainingInState = KF_MESSAGE_DELAY_TIME;   // +5008 = 0.8
    mePrepareStage  = E_PREPARESTAGE_START;           // +4800
    meReleaseStage  = E_RELEASESTAGE_START;           // +4812
    meUpdateStage   = E_UPDATESTAGE_NOTDISPLAYED;     // +4804
    meTickerMode    = E_TICKERMODE_NONE;              // +4808
    mpHeapAllocator = 0;                              // +4816
    mpBlendState    = 0;                              // +4868

    mTextObject.Construct(0, 0);                      // +4872

    mpTextRenderer     = 0;                           // +4996
    mpLanguageManager  = 0;                           // +5000
    mpGuiCache         = 0;                           // +5004
    mu8TextAlpha           = 0;                       // +5012
    mu8BackgroundAlpha     = 0;                       // +5013
    mfTextStartPosX        = 0.0f;                    // +4840
    mu8BackgroundAlphaPeak = 0;                       // +5014
    mfTextPosY             = gfTickerTextPosY;        // +4844 <- flt_82F2581C
    mpCurrentMessage       = 0;                       // +4796

    for (s32 li = 0; li < K_MAX_INGAME_MESSAGES; ++li)
    {
        maMessages[li].Construct();
    }
    mAutoSaveMessage.Construct();

    mRoadRulesToShow = CgsContainers::BitArray<64u>();  // +4824 (zeroed)
    mu8NextRoadRulesMessage    = 0;                   // +5015
    mu8NextIntervalMessage     = 0;                   // +5016
    mu8NumRoadRuleScoresShown  = 0;                   // +5017
    mbEnabled                  = false;               // +5018
    mbGamePaused               = false;               // +5019
    mbGamePausedForDisconnect  = false;               // +5020
    mbShowingBreakingNews      = false;               // +5021
    mbShownStartEngineTip      = false;               // +5022
    mbShowAutoSaveMessage      = false;               // +5023
}

// ---------------------------------------------------------------------------
// Prepare @0x82455558 -- one-stage prepare: latch the collaborators + reset the
// whole runtime state, then report DONE. (meTickerMode seeds OFFLINE.)
// ---------------------------------------------------------------------------
bool InGameMessageRenderer::Prepare(CgsGui::GuiEventQueueSmall* lpEventQueue,
                                    rw::IResourceAllocator* lpHeapAllocator,
                                    rw::IResourceAllocator* /*lpTextureAllocator*/)
{
    switch (mePrepareStage)
    {
        case E_PREPARESTAGE_START:
        {
            mpHeapAllocator        = lpHeapAllocator;         // +4816 <- a3
            mfTimeRemainingInState = KF_MESSAGE_DELAY_TIME;
            mePrepareStage         = E_PREPARESTAGE_START;
            meUpdateStage          = E_UPDATESTAGE_NOTDISPLAYED;
            meTickerMode           = E_TICKERMODE_OFFLINE;    // +4808 = 2

            CGS_ASSERT(lpEventQueue != 0, "lpOutputEventQueue");   // :248
            mpOutputEventQueue = lpEventQueue;                // +4832

            mu8TextAlpha           = 0;
            mu8BackgroundAlpha     = 0;
            mu8BackgroundAlphaPeak = 0;
            mfTextStartPosX        = KF_MESSAGE_START_POS_X;  // 1280.0
            miLastRoadRuleScoreReceived = -1;                 // +4836
            mfTextPosY             = gfTickerTextPosY;        // flt_82F2581C
            mRoadRulesToShow       = CgsContainers::BitArray<64u>();
            mu8NextRoadRulesMessage   = 0;
            mu8NextIntervalMessage    = 0;
            mu8NumRoadRuleScoresShown = 0;
            mbEnabled                 = false;
            mbGamePaused              = false;
            mbGamePausedForDisconnect = false;
            mbShowingBreakingNews     = false;
            mbShownStartEngineTip     = false;
            mbShowAutoSaveMessage     = false;

            mePrepareStage = E_PREPARESTAGE_DONE;
            return true;
        }

        case E_PREPARESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_DONE;
            return true;

        default:
            CGS_ASSERT(false, " unknown prepare stage in InGameMessageRenderer ");   // :285
            return false;
    }
}

// ---------------------------------------------------------------------------
// Release @0x82455688 -- clear the road-rule mask and free the blend-state
// resource through the heap allocator when it was acquired.
// ---------------------------------------------------------------------------
bool InGameMessageRenderer::Release()
{
    switch (meReleaseStage)
    {
        case E_RELEASESTAGE_START:
            meReleaseStage   = E_RELEASESTAGE_START;
            mRoadRulesToShow = CgsContainers::BitArray<64u>();
            // Console: `if (mpBlendState) (*(*mpHeapAllocator + 20))(mpHeapAllocator,
            // &mBlendStateResource)` -- the allocator free of the acquired blend state.
            // [FLAG PC fold] the pair is never acquired on this build (see the member
            // banner), so the conditional free is a structural no-op here.
            meReleaseStage = E_RELEASESTAGE_DONE;
            return true;

        case E_RELEASESTAGE_DONE:
            meReleaseStage = E_RELEASESTAGE_DONE;
            return true;

        default:
            CGS_ASSERT(false, " unknown release stage in InGameMessageRenderer ");   // :332
            return false;
    }
}

// ---------------------------------------------------------------------------
// Destruct @0x82455758 -- chain the base destruct, clear the road-rule mask.
// (The Hex-Rays callee name "BaseCollisionGenerator::Destruct" is an ICF fold of
// the empty base CustomRenderComponentInterface::Destruct.)
// ---------------------------------------------------------------------------
void InGameMessageRenderer::Destruct()
{
    CgsGui::CustomRenderComponentInterface::Destruct();
    mRoadRulesToShow = CgsContainers::BitArray<64u>();
}

// ---------------------------------------------------------------------------
// Update @0x82446F30 -- refresh the controller-disconnect latch off the cache.
// ---------------------------------------------------------------------------
void InGameMessageRenderer::Update()
{
    if (mpGuiCache != 0)
    {
        mbGamePausedForDisconnect = (mpGuiCache->GetActiveControllerIndex() == -1);  // cache+19256
    }
}

// ---------------------------------------------------------------------------
// GetID @0x82446F58.
// ---------------------------------------------------------------------------
CgsID InGameMessageRenderer::GetID() const
{
    return KID_INGAME_MESSAGE;
}

// ---------------------------------------------------------------------------
// SetLanguageManager @0x82443FB0 (re-homed from the 2026-08-16 minimal slice, with
// the two globals' identity CORRECTED -- see the file banner).
// ---------------------------------------------------------------------------
void* InGameMessageRenderer::SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager)
{
    CGS_ASSERT(lpLanguageManager != 0, "lpLanguageManager");

    mpLanguageManager = lpLanguageManager;

    if (lpLanguageManager->GetCurrentLanguage() == KI_WIDE_GLYPH_LANGUAGE)
    {
        gfTickerFontHeight = KF_WIDE_GLYPH_FONT_HEIGHT;   // flt_82F25818 <- 28.0
        gfTickerTextPosY   = KF_WIDE_GLYPH_TEXT_POS_Y;    // flt_82F2581C <- 629.0
    }

    return this;
}

// ---------------------------------------------------------------------------
// ResetYPos @0x82446F70 -- re-seat the text Y for the mode (mode 5, custom
// messages, keeps whatever event 535 set).
// ---------------------------------------------------------------------------
void InGameMessageRenderer::ResetYPos()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :1898

    switch (meTickerMode)
    {
        case E_TICKERMODE_STARTENGINE:
        case E_TICKERMODE_OFFLINE:
        case E_TICKERMODE_SIGNEDIN:
        case E_TICKERMODE_ROADRULES:
        case E_TICKERMODE_AUTOSAVE_MESSAGE:
        case E_TICKERMODE_RECONNECT_CONTROLLER_MESSAGE:
            mfTextPosY = gfTickerTextPosY;   // flt_82F2581C
            break;

        case E_TICKERMODE_CUSTOMMESSAGES:
            break;

        default:
            CGS_ASSERT(false, "Invalid ticker mode  in InGameMessageRenderer::ResetYPos\n");  // :1925
            break;
    }
}

// ---------------------------------------------------------------------------
// ClearAllMessages @0x8244B4E0 -- unlink every queued message, KEEPING challenge
// messages unless lbClearChallenge and training messages unless lbClearTraining.
// ---------------------------------------------------------------------------
void InGameMessageRenderer::ClearAllMessages(bool lbClearTraining, bool lbClearChallenge)
{
    InGameMessage* lpMessage = mpCurrentMessage;
    InGameMessage* lpKept    = 0;   // the last message that survived (list splice anchor)

    while (lpMessage != 0)
    {
        if (!lpMessage->mbIsChallengeMessage || lbClearChallenge)      // +521
        {
            if (!lpMessage->mbIsTrainingMessage || lbClearTraining)    // +520
            {
                lpMessage->mbInUse = false;
                if (mpCurrentMessage == lpMessage)
                {
                    mpCurrentMessage = lpMessage->mpNextMessage;
                }
                if (lpKept != 0)
                {
                    lpKept->mpNextMessage = lpMessage->mpNextMessage;
                }
            }
            else
            {
                lpKept = lpMessage;
            }
        }
        else
        {
            lpKept = lpMessage;
        }

        lpMessage = lpMessage->mpNextMessage;
    }
}

// ---------------------------------------------------------------------------
// UpdateTickerMode @0x8244B570 -- pick the mode for this frame, and on a change
// reset/fade the presentation.
// ---------------------------------------------------------------------------
void InGameMessageRenderer::UpdateTickerMode()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :1800

    ETickerMode leNewMode;
    if (mbGamePausedForDisconnect)               // +5020
    {
        leNewMode = E_TICKERMODE_RECONNECT_CONTROLLER_MESSAGE;   // 7
    }
    else if (mbShowAutoSaveMessage)              // +5023
    {
        leNewMode = E_TICKERMODE_AUTOSAVE_MESSAGE;               // 6
    }
    else
    {
        // Any queued CUSTOM message forces the custom-messages mode (the ticker's route).
        s32 liCustomCount = 0;
        for (InGameMessage* lp = mpCurrentMessage; lp != 0; lp = lp->mpNextMessage)
        {
            if (lp->mbIsCustomMessage)           // +519
            {
                ++liCustomCount;
            }
        }

        if (liCustomCount > 0)
        {
            leNewMode = E_TICKERMODE_CUSTOMMESSAGES;             // 5
        }
        else
        {
            // Road-rules eligibility: an active road rule (cache+44092) AND either scores
            // pending in the 64-bit mask OR a road-rule message already queued -> mode 4;
            // otherwise offline/signed-in off the cache's signed-in byte (+19280;
            // consumer-named AreRoadRuleFriendScoresAvailable on this build).
            bool lbRoadRules = false;
            if (mpGuiCache->GetActiveRoadRule() != 0)
            {
                u64 lu64Mask = 0;
                mRoadRulesToShow.GetBitRange(0, 64, &lu64Mask);
                if (lu64Mask != 0)
                {
                    lbRoadRules = true;
                }
                else
                {
                    for (InGameMessage* lp = mpCurrentMessage; lp != 0; lp = lp->mpNextMessage)
                    {
                        if (lp->mbIsRoadRuleMessage)     // +518
                        {
                            lbRoadRules = true;
                            break;
                        }
                    }
                }
            }
            leNewMode = lbRoadRules ? E_TICKERMODE_ROADRULES
                                    : (mpGuiCache->AreRoadRuleFriendScoresAvailable()
                                           ? E_TICKERMODE_SIGNEDIN     // 3
                                           : E_TICKERMODE_OFFLINE);    // 2
        }
    }

    if (meTickerMode != leNewMode)
    {
        if (meTickerMode == E_TICKERMODE_STARTENGINE)
        {
            mbShownStartEngineTip = true;        // +5022
        }
        meTickerMode = leNewMode;

        if (leNewMode == E_TICKERMODE_AUTOSAVE_MESSAGE ||
            leNewMode == E_TICKERMODE_CUSTOMMESSAGES ||
            leNewMode == E_TICKERMODE_RECONNECT_CONTROLLER_MESSAGE)
        {
            InGameMessage* lpHead = mpCurrentMessage;
            meUpdateStage          = E_UPDATESTAGE_NOTDISPLAYED;
            mfTimeRemainingInState = 0.0f;
            // Drop a leading PRIORITY autosave message so the new mode starts clean.
            if (lpHead != 0 && lpHead->mbIsPriorityMessage && lpHead == &mAutoSaveMessage)
            {
                lpHead->mbInUse  = false;
                mpCurrentMessage = mpCurrentMessage->mpNextMessage;
            }
        }
        else
        {
            if (meUpdateStage == E_UPDATESTAGE_FADINGIN ||
                meUpdateStage == E_UPDATESTAGE_DISPLAYING_MESSAGES ||
                meUpdateStage == E_UPDATESTAGE_DISPLAYING_ROADRULES)
            {
                mfTimeRemainingInState = KF_MESSAGE_FADE_TIME;   // 0.3
                meUpdateStage          = E_UPDATESTAGE_FADINGOUT;
            }
            ClearAllMessages(/*lbClearTraining*/ true, /*lbClearChallenge*/ false);
        }
    }
}

// ---------------------------------------------------------------------------
// AddNewMessage @0x82455790.
// ---------------------------------------------------------------------------
bool InGameMessageRenderer::AddNewMessage(
    bool lbPriority, bool lbRoadRule, bool lbCustom, bool lbRepeats, bool lbTraining,
    bool lbAllowDuplicates, bool lbChallenge,
    const CgsUnicode::CgsUtf8* lpMessageText, s32 liNumParams,
    const CgsUnicode::CgsUtf8* lpParam1, const CgsUnicode::CgsUtf8* lpParam2,
    const CgsUnicode::CgsUtf8* lpParam3)
{
    if (lbPriority && lbCustom)
    {
        CGS_ASSERT(false,
                   "false == (( true == lbIsPriorityMessage) && ( true == lbIsCustomMessage ))"); // :1120
    }
    // The console evaluates the handle cast here; on a NULL handle the cast's own
    // assert expression dereferences mpResourceMemory and would AV before FireAssert
    // ever ran (proven on the 2026-08-24 boot: AV reading 0 at AddNewMessage+0x4E).
    // Test the handle's memory pointer directly -- the null-safe x64 equivalent.
    CGS_ASSERT(mTextObject.mpFont.mpResourceMemory != 0,
               "Font for the InGameMessageRenderer was not Loaded");   // :1121
    if (mTextObject.mpFont.mpResourceMemory == 0)
    {
        // [PC bring-up guard] A dev assert continues on PC; without the font every
        // string-width call below dereferences null. Drop the message instead.
        return false;
    }

    // Recompute the shared message gap so the next line starts just off-screen of the
    // current head: 1280 - (fontHeight-scaled head width + current X), floored at 100.
    if (mpCurrentMessage != 0)
    {
        const f32 lfHeadWidth =
            mTextObject.mpFont->GetStringWidth(mpCurrentMessage->macMessageText);
        f32 lfGap = 1280.0f - ((mTextObject.mfFontHeight * lfHeadWidth) + mfTextStartPosX);
        if (lfGap < 100.0f)
        {
            lfGap = 100.0f;
        }
        gfTickerMessageGap = lfGap;              // flt_82F25BB0
    }

    if (lbPriority)
    {
        // A priority message takes the dedicated slot and jumps the queue head. The console
        // hardcodes the flag set here: SetupMessage(rec, 1, 0, 0, 0, 0, 1, text) --
        // priority + repeats, everything else off.
        mAutoSaveMessage.SetupMessage(true, false, false, false, false, true,
                                      lpMessageText, liNumParams, lpParam1, lpParam2, lpParam3);
        mAutoSaveMessage.mpNextMessage = mpCurrentMessage;
        mpCurrentMessage = &mAutoSaveMessage;
        return true;
    }

    // Find a free pool slot.
    s32 liSlot = 0;
    while (maMessages[liSlot].mbInUse)
    {
        if (++liSlot >= K_MAX_INGAME_MESSAGES)
        {
            return false;   // pool full
        }
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "Adding ticker message : \""
            << (lpMessageText != 0 ? reinterpret_cast<const char*>(lpMessageText)
                                   : "<NULLSTRING>")
            << "\"\nMessage has " << liNumParams << " parameters\n";
    }

    InGameMessage& lrMessage = maMessages[liSlot];
    lrMessage.SetupMessage(lbPriority, lbRoadRule, lbCustom, lbTraining, lbChallenge, lbRepeats,
                           lpMessageText, liNumParams, lpParam1, lpParam2, lpParam3);
    lrMessage.mfStringWidth =
        mTextObject.mpFont->GetStringWidthIgnoringNewlines(lrMessage.macMessageText);

    // Duplicate suppression (skipped when lbAllowDuplicates): an identical hash already
    // queued rolls the fresh record back to empty.
    bool lbDuplicate = false;
    if (!lbAllowDuplicates)
    {
        for (InGameMessage* lp = mpCurrentMessage; lp != 0; lp = lp->mpNextMessage)
        {
            if (lp->muStringHash == lrMessage.muStringHash)
            {
                lbDuplicate = true;
                break;
            }
        }
    }

    if (lbDuplicate)
    {
        lrMessage.Construct();
        lrMessage.muStringHash = 0;
        return true;
    }

    // Append to the tail (or become the head).
    if (mpCurrentMessage != 0)
    {
        InGameMessage* lpTail = mpCurrentMessage;
        while (lpTail->mpNextMessage != 0)
        {
            lpTail = lpTail->mpNextMessage;
        }
        lpTail->mpNextMessage = &lrMessage;
    }
    else
    {
        mpCurrentMessage = &lrMessage;
    }
    return true;
}

// ---------------------------------------------------------------------------
// DrawMessages @0x82455B10 -- lay each queued line into mTextObject and RenderString
// it, walking the scroll position; a repeated head wraps to the tail, a finished
// non-repeating head is retired.
// ---------------------------------------------------------------------------
void InGameMessageRenderer::DrawMessages(CgsGraphics::Im2dRenderBuffer* lpBuffer)
{
    f32 lfPosX = mfTextStartPosX;
    InGameMessage* lpMessage = mpCurrentMessage;

    // Text colour: the wide-glyph language draws the ticker in the alternate colour.
    const u32 luBaseColour =
        (mpLanguageManager != 0 &&
         mpLanguageManager->GetCurrentLanguage() == KI_WIDE_GLYPH_LANGUAGE)
            ? KU_TEXT_COLOUR_WIDE : KU_TEXT_COLOUR;

    if (lpMessage == 0)
    {
        return;
    }

    // A PRIORITY head draws alone, centered state (the autosave/reconnect banner).
    if (lpMessage->mbIsPriorityMessage)
    {
        mTextObject.mpUtf8String = lpMessage->macMessageText;
        if (mTextObject.mbAutosize)
        {
            mTextObject.CalculateAutosizing();
        }
        // colour = base RGB with the animated text alpha in the top byte
        // (the console's rlwimi insert of mu8TextAlpha).
        mTextObject.mfFontHeight   = gfTickerFontHeight;                 // flt_82F25818
        mTextObject.mTextColour    = (luBaseColour & 0x00FFFFFFu) |
                                     (static_cast<u32>(mu8TextAlpha) << 24);
        mTextObject.mv2TopLeft.mX  = lfPosX;
        mTextObject.mv2TopLeft.mY  = mfTextPosY;
        mTextObject.mv2BottomRight.mX = 1280.0f;
        mTextObject.mv2BottomRight.mY = mfTextPosY + KF_MESSAGE_HEIGHT_PAD;
        mTextObject.meAlignment    = CgsGraphics::TextObject::E_ALIGNMENT_LEFT;

        const f32 lfWidth = mTextObject.mpFont->GetStringWidth(lpMessage->macMessageText);
        mTextObject.mfStringWidth = lfWidth;
        if (mTextObject.mbAutosize)
        {
            mTextObject.CalculateAutosizing();
        }
        mpTextRenderer->RenderStringBuffered(
            &reinterpret_cast<CgsGui::AptIm2dRenderBuffer*>(lpBuffer)->mCommandBuffer,
            mTextObject);

        if ((mTextObject.mfFontHeight * lfWidth) + lfPosX <= 0.0f)
        {
            mfTextStartPosX = 1280.0f;   // wrapped off-screen: restart from the right
        }
        return;
    }

    // The scrolling multi-line walk.
    bool lbWrapped = false;
    while (lfPosX < 1280.0f)
    {
        mTextObject.mpUtf8String = lpMessage->macMessageText;
        if (mTextObject.mbAutosize)
        {
            mTextObject.CalculateAutosizing();
        }
        mTextObject.mfFontHeight   = gfTickerFontHeight;
        mTextObject.mTextColour    = (luBaseColour & 0x00FFFFFFu) |
                                     (static_cast<u32>(mu8TextAlpha) << 24);
        mTextObject.mv2TopLeft.mX  = lfPosX;
        mTextObject.mv2TopLeft.mY  = mfTextPosY;
        mTextObject.mv2BottomRight.mX = 1280.0f;
        mTextObject.mv2BottomRight.mY = mfTextPosY + KF_MESSAGE_HEIGHT_PAD;
        mTextObject.meAlignment    = CgsGraphics::TextObject::E_ALIGNMENT_LEFT;
        mTextObject.mfStringWidth  = lpMessage->mfStringWidth;
        if (mTextObject.mbAutosize)
        {
            mTextObject.CalculateAutosizing();
        }

        const f32 lfScaledWidth = mTextObject.mfFontHeight * lpMessage->mfStringWidth;
        // [DIAG] NOT IN THE X360 BINARY -- the [tut-ticker] DRAW rung (first 8).
        {
            static s32 siDrawDiagLeft = 8;
            if (siDrawDiagLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                --siDrawDiagLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[tut-ticker] DrawMessages x=" << lfPosX << " y=" << mfTextPosY
                    << " alpha=" << static_cast<s32>(mu8TextAlpha)
                    << " w=" << lpMessage->mfStringWidth
                    << " text='" << reinterpret_cast<const char*>(lpMessage->macMessageText)
                    << "'\n";
            }
        }
        mpTextRenderer->RenderStringBuffered(
            &reinterpret_cast<CgsGui::AptIm2dRenderBuffer*>(lpBuffer)->mCommandBuffer,
            mTextObject);

        InGameMessage* lpHead   = mpCurrentMessage;
        const f32      lfEndX   = lfScaledWidth + lfPosX;
        lfPosX = lpHead->mbRepeats ? (lfEndX + 100.0f) : (gfTickerMessageGap + lfEndX);

        if (lpMessage == lpHead && lfEndX <= 0.0f)
        {
            // The head just scrolled fully off-screen.
            if (lpHead->mbRepeats)
            {
                // Rotate the repeating head to the tail.
                lpMessage = lpMessage->mpNextMessage;
                InGameMessage* lpTail = mpCurrentMessage;
                if (lpMessage != 0)
                {
                    if (lpHead->mpNextMessage != 0)
                    {
                        while (lpTail->mpNextMessage != 0)
                        {
                            lpTail = lpTail->mpNextMessage;
                        }
                    }
                    lpTail->mpNextMessage = lpHead;
                    mpCurrentMessage      = mpCurrentMessage->mpNextMessage;
                    lpHead->mpNextMessage = 0;
                    lpHead   = mpCurrentMessage;
                    lpMessage = lpHead;
                }
                mfTextStartPosX = lfPosX;
                if (lpMessage == 0)
                {
                    lpMessage = lpHead;
                    lbWrapped = true;
                    if (lpHead == 0)
                    {
                        break;
                    }
                }
                while (lpMessage != 0 && !lpMessage->mbRepeats && lbWrapped)
                {
                    lpMessage = lpMessage->mpNextMessage;
                }
            }
            else
            {
                // Retire the finished head.
                lpHead->mbInUse  = false;
                mpCurrentMessage = mpCurrentMessage->mpNextMessage;
                mfTextStartPosX  = lfPosX;
                lpMessage        = mpCurrentMessage;
            }
        }
        else
        {
            lpMessage = lpMessage->mpNextMessage;
            if (lpMessage == 0)
            {
                lpMessage = mpCurrentMessage;
                lbWrapped = true;
                if (lpHead == 0)
                {
                    break;
                }
            }
            while (lpMessage != 0 && !lpMessage->mbRepeats && lbWrapped)
            {
                lpMessage = lpMessage->mpNextMessage;
            }
        }

        if (lpMessage == 0)
        {
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// DrawBackground @0x8245CC20 -- the black band behind the text: a 4-vertex
// triangle strip over [0..1280] x [posY-2 .. posY+fontHeight+2], black at
// mu8BackgroundAlpha. Skipped entirely at alpha 0.
// ---------------------------------------------------------------------------
void InGameMessageRenderer::DrawBackground(CgsGraphics::Im2dRenderBuffer* lpBuffer)
{
    if (mu8BackgroundAlpha == 0)
    {
        return;
    }

    const f32 lfTop    = mfTextPosY - 2.0f;
    const f32 lfBottom = mfTextPosY + gfTickerFontHeight + 2.0f;

    // K_BACKGROUND_COLOURS_BLACK (dword_82F25BB4 == 0xFF000000) with the animated alpha in
    // the top byte -- black RGB, alpha = mu8BackgroundAlpha (the console's byte repack).
    const u32 luColour = static_cast<u32>(mu8BackgroundAlpha) << 24;

    CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
    const f32 lafX[4] = { 0.0f, 0.0f, 1280.0f, 1280.0f };
    const f32 lafY[4] = { lfTop, lfBottom, lfTop, lfBottom };
    const f32 lafU[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    const f32 lafV[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    for (s32 li = 0; li < 4; ++li)
    {
        laVerts[li].mv2Pos.x    = lafX[li];
        laVerts[li].mv2Pos.y    = lafY[li];
        laVerts[li].mv2Tex0UV.x = lafU[li];
        laVerts[li].mv2Tex0UV.y = lafV[li];
        *reinterpret_cast<u32*>(&laVerts[li].mv4Colour) = luColour;
    }

    // Console: sub_82458988(buffer+4, 6 /*triangle strip*/, verts, 4) -- the plain
    // render-static submit through the bound (untextured) state. PC fold: clear the
    // bound texture then submit through the COPYING Render (@0x24EDE0) -- RenderEnd
    // stores only a POINTER to the run, and these vertices are stack locals that are
    // gone by dispatch time (the 14:04 boot proved it: every band invisible).
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrCmd =
        reinterpret_cast<CgsGui::AptIm2dRenderBuffer*>(lpBuffer)->mCommandBuffer;
    lrCmd.SetTexture(0);
    lrCmd.Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);
}

// ---------------------------------------------------------------------------
// BufferMessagesForGameMode @0x82455E98 -- refill the queue for the current mode.
// [FLAG PARTIAL, named]: mode 4 (ROADRULES -- the breaking-news id table
// off_82F2584C + the RRULES clock format) is un-transcribed; it is unreachable on
// this build (no active road rule -- see UpdateTickerMode). Modes 6/7 are whole.
// Modes 1/2/3 (start-engine / offline / signed-in ambient strings) fall through
// with no messages, exactly as the console's switch does (no case for them here).
// ---------------------------------------------------------------------------
void InGameMessageRenderer::BufferMessagesForGameMode()
{
    switch (meTickerMode)
    {
        case E_TICKERMODE_NONE:
        case E_TICKERMODE_NUM:
            CGS_ASSERT(false,
                       "Unhandled ticker mode in InGameMessageRenderer::BufferMessagesForGameMode"); // :1726
            break;

        case E_TICKERMODE_ROADRULES:
            // [FLAG parked -- see the banner. Unreachable offline: mode 4 needs
            // GuiCache::GetActiveRoadRule() != 0.]
            break;

        case E_TICKERMODE_AUTOSAVE_MESSAGE:
        {
            // Console case 6 (asm @0x82456090): the RAW literal "Saving"
            // (KAC_AUTOSAVE_STRING[7]) with (priority, repeats, allowDuplicates).
            AddNewMessage(true, false, false, /*repeats*/ true, /*training*/ false,
                          /*allowDup*/ true, /*challenge*/ false,
                          reinterpret_cast<const CgsUnicode::CgsUtf8*>("Saving"),
                          0, 0, 0, 0);
            break;
        }

        case E_TICKERMODE_RECONNECT_CONTROLLER_MESSAGE:
        {
            // Console case 7 (asm @0x824560DC): FindString("CONTROLLER_DISCONNECTED"),
            // FALLING BACK to the key literal when the language misses it.
            const CgsUnicode::CgsUtf8* lpText =
                (mpLanguageManager != 0)
                    ? mpLanguageManager->FindString("CONTROLLER_DISCONNECTED")
                    : 0;
            if (lpText == 0)
            {
                lpText = reinterpret_cast<const CgsUnicode::CgsUtf8*>("CONTROLLER_DISCONNECTED");
            }
            AddNewMessage(true, false, false, /*repeats*/ true, /*training*/ false,
                          /*allowDup*/ false, /*challenge*/ false, lpText, 0, 0, 0, 0);
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// RequestNewRoadRulesScore @0x82468E20 -- [FLAG PARKED, named]: the road-rules
// score-request pump (posts the next queued road-rule query through the output
// event queue). Its whole surface -- the 64-bit pending mask walk + the score
// request event layout -- belongs to the road-rules feed, which is unreachable
// on this build (no active road rule). Returns "nothing outstanding".
// ---------------------------------------------------------------------------
bool InGameMessageRenderer::RequestNewRoadRulesScore()
{
    return false;
}

// ---------------------------------------------------------------------------
// RecvEvent @0x82468170 -- the event dispatch. WHOLE for the ticker family
// (14 font adopt / 64 cache / 145 enable / 505 pause / 534-537-539), PARKED (named)
// for the road-rules income arms (0..3 score responses, 345 new-score, 346
// breaking-news mask) -- each needs the road-rules event payload homes and is
// unreachable offline.
// ---------------------------------------------------------------------------
void InGameMessageRenderer::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
{
    switch (liEventType)
    {
        case 145:   // ticker master enable
            mbEnabled = true;   // +5018
            break;

        case 14:    // load notification: adopt the ticker font
        {
            // Payload: CgsGui::GuiEventLoadNotification { ResourceHandle @+0 (PC 16B),
            // meRequestType, muLoadRequestId } -- the console reads the u64 handle at +0 and
            // the type word at +8; the PC struct is the same record at x64 widths.
            CGS_ASSERT(lpEvent != 0, "Invalid resource data sent InGameMessageRenderer::RecvEvent"); // :400
            const CgsGui::GuiEventLoadNotification* lpcNotification =
                reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent);
            if (static_cast<s32>(lpcNotification->meRequestType) != 16)   // FONTDATA only
            {
                break;
            }

            CgsResource::SafeResourceHandle<CgsResource::Font> lFont;
            lFont.mpResourceMemory = lpcNotification->mResourceHandle.mpResourceMemory;
            lFont.mpSourceEntry    = lpcNotification->mResourceHandle.mpSourceEntry;
            if (lFont.mpResourceMemory == 0)
            {
                CGS_ASSERT(false, "lpFont != CgsResource::NULLResourceHandle");   // :406
                break;
            }

            CgsResource::Font* lpFont = static_cast<CgsResource::Font*>(lFont);
            const char* lpcFontName = lpFont->macTypefaceFamilyName;   // the console's Font+336 read

            // While the ticker still has NO font, adopt the language default...
            // (HasDefaultFont gate: PC-only -- PrepareDefaultFont is unreconstructed,
            // so an unconditional GetDefaultFont() fires its :443 assert on every
            // early font load. The arm is inert until that name loads; the B5DOTMAT
            // arm below is the one that adopts the ticker face.)
            if (mTextObject.mpFont.mpResourceMemory == 0 && mpLanguageManager != 0 &&
                mpLanguageManager->HasDefaultFont())
            {
                const char* lpcDefault = mpLanguageManager->GetDefaultFont();
                if (lpcDefault != 0 &&
                    _strnicmp(lpcFontName, lpcDefault, std::strlen(lpcDefault)) == 0)
                {
                    mTextObject.mpFont = lFont;
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
                        CgsDev::Log::gpDebugPrint != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "InGameMessageRenderer: using default font " << lpcFontName << "\n";
                    }
                }
            }
            // ...and ALWAYS prefer the dedicated ticker face when it arrives. ARTIST
            // compares the first eight characters against "B5DOTMAT" (Font+0x150).
            if (_strnicmp(lpcFontName, "B5DOTMAT", 8) == 0)
            {
                mTextObject.mpFont = lFont;
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    // [DIAG] NOT IN THE X360 BINARY -- the [tut-ticker] FONT rung.
                    *CgsDev::Log::gpDebugPrint
                        << "[tut-ticker] ticker font adopted '" << lpcFontName << "'\n";
                }
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
                    CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "InGameMessageRenderer: using font " << lpcFontName << "\n";
                }
            }
            break;
        }

        case 64:    // the GuiCache bind
        {
            GuiCache* const* lppCache = reinterpret_cast<GuiCache* const*>(lpEvent);
            if (*lppCache == 0)
            {
                CGS_ASSERT(false, "Invalid GUI cache pointer");   // :433
            }
            mpGuiCache = *lppCache;
            break;
        }

        case 505:   // game paused {u32 pad, bool paused @+4}
        {
            CGS_ASSERT(lpEvent != 0, "lpPausedEvent");   // :731
            mbGamePaused = (reinterpret_cast<const u8*>(lpEvent)[4] != 0);   // +5019
            if (mpGuiCache != 0)
            {
                mbGamePausedForDisconnect = (mpGuiCache->GetActiveControllerIndex() == -1);
            }
            break;
        }

        case 534:   // toggle the master enable; report visibility (event 538) back
        {
            mbEnabled = !mbEnabled;
            const u8 lu8Visible = (meUpdateStage != E_UPDATESTAGE_NOTDISPLAYED) ? 1u : 0u;
            if (mpOutputEventQueue != 0)
            {
                mpOutputEventQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lu8Visible), 538, 1);
            }
            break;
        }

        case 535:   // move the ticker Y
        {
            const f32 lfY = *reinterpret_cast<const f32*>(lpEvent);
            mfTextPosY = lfY;
            if (lfY < 0.0f || lfY >= 720.0f)
            {
                CGS_ASSERT(false, "Moving ticker off-screen - is this right?");   // :472
            }
            break;
        }

        case 536:   // clear messages {bool fadeIfEmpty @+0, bool clearChallenge @+1}
        {
            const u8* lpcFlags = reinterpret_cast<const u8*>(lpEvent);
            ClearAllMessages(false, lpcFlags[1] != 0);
            if (lpcFlags[0] != 0)
            {
                bool lbTrainingQueued = false;
                for (InGameMessage* lp = mpCurrentMessage; lp != 0; lp = lp->mpNextMessage)
                {
                    if (lp->mbIsTrainingMessage)
                    {
                        lbTrainingQueued = true;
                        break;
                    }
                }
                if (!lbTrainingQueued &&
                    (meUpdateStage == E_UPDATESTAGE_FADINGIN ||
                     meUpdateStage == E_UPDATESTAGE_DISPLAYING_MESSAGES ||
                     meUpdateStage == E_UPDATESTAGE_DISPLAYING_ROADRULES))
                {
                    mfTimeRemainingInState = KF_MESSAGE_FADE_TIME;
                    meUpdateStage          = E_UPDATESTAGE_FADINGOUT;
                }
            }
            break;
        }

        case 537:   // ⭐ THE TICKER CUSTOM MESSAGE (the training-tip route)
        {
            // The 2072-byte wire record (the bridge's TickerCustomMessageWire537):
            //   +0     s32  maiStringTypes[4]     (1 = raw text, 2 = language string id)
            //   +16    char maacStrings[4][512]
            //   +2064  s8   mi8NumStrings
            //   +2065..2068 the four flags {repeats, training, allowDuplicates, challenge}
            const u8* lpcRecord = reinterpret_cast<const u8*>(lpEvent);
            const s8  li8Count  = static_cast<s8>(lpcRecord[2064]);

            const CgsUnicode::CgsUtf8* lapResolved[4] = { 0, 0, 0, 0 };
            bool lbResolved = true;
            for (s8 li = 0; li < li8Count; ++li)
            {
                const s32 liStringType =
                    reinterpret_cast<const s32*>(lpcRecord)[li];
                const CgsUnicode::CgsUtf8* lpcString =
                    reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpcRecord + 16 + li * 512);
                if (liStringType == 1)
                {
                    lapResolved[li] = lpcString;
                }
                else if (liStringType == 2)
                {
                    lapResolved[li] = (mpLanguageManager != 0)
                        ? mpLanguageManager->FindString(reinterpret_cast<const char*>(lpcString))
                        : 0;
                    if (lapResolved[li] == 0)
                    {
                        lbResolved = false;
                        break;   // console: an unresolvable id abandons the resolve loop
                    }
                }
                else
                {
                    CGS_ASSERT(false,
                               "Unhandled string type  handling message E_GUI_TICKER_CUSTOM_MESSAGE\n"); // :528
                    lbResolved = false;
                    break;
                }
            }

            if (li8Count > 0 && !lbResolved)
            {
                break;   // console falls out of the arm without adding
            }

            const bool lbRepeats         = lpcRecord[2065] != 0;   // maFlags[0]
            const bool lbTraining        = lpcRecord[2066] != 0;   // maFlags[1]
            const bool lbAllowDuplicates = lpcRecord[2067] != 0;   // maFlags[2]
            const bool lbChallenge       = lpcRecord[2068] != 0;   // maFlags[3]

            if (lbTraining)
            {
                ClearAllMessages(false, false);   // a training line clears the ambient queue
            }

            const bool lbAdded = AddNewMessage(
                false, false, /*custom*/ true, lbRepeats, lbTraining, lbAllowDuplicates,
                lbChallenge, lapResolved[0], (li8Count > 0) ? (li8Count - 1) : 0,
                lapResolved[1], lapResolved[2], lapResolved[3]);

            // [DIAG] NOT IN THE X360 BINARY -- the [tut-ticker] consumer rung: the text is
            // QUEUED in the renderer. First-N latched.
            {
                static s32 siDiagLeft = 8;
                if (siDiagLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
                {
                    --siDiagLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[tut-ticker] InGameMessageRenderer queued custom message"
                        << " added=" << (lbAdded ? 1 : 0)
                        << " training=" << (lbTraining ? 1 : 0)
                        << " text='"
                        << (lapResolved[0] != 0
                                ? reinterpret_cast<const char*>(lapResolved[0]) : "<null>")
                        << "'\n";
                }
            }

            if (!lbAdded)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
                    CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "Unable to add a new GUI custom ticker message (too full), so dropping it.\n";
                }
                if (lbTraining)
                {
                    CGS_ASSERT(false, "Ticker is too full for a training message!\n");   // :556
                }
            }
            break;
        }

        case 539:   // visibility query -> event 538
        {
            const u8 lu8Visible = (meUpdateStage != E_UPDATESTAGE_NOTDISPLAYED) ? 1u : 0u;
            if (mpOutputEventQueue != 0)
            {
                mpOutputEventQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lu8Visible), 538, 1);
            }
            break;
        }

        // [FLAG PARKED, named -- the road-rules income arms. Console: 0..3 = the four
        // road-rule score RESPONSE flavours (name lookup + time/currency format +
        // AddNewMessage + the 64-bit shown-mask update), 345 = a new road-rule score
        // arriving (mode-4 refresh), 346 = the breaking-news mask (mbShowingBreakingNews
        // latch + fadeout kick). All unreachable offline (no active road rule feeds
        // them); their payload layouts land with the road-rules feed.]
        case 0: case 1: case 2: case 3:
        case 345: case 346:
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// RenderComponent @0x82469C68 -- the per-frame state machine + draw.
// ---------------------------------------------------------------------------
void InGameMessageRenderer::RenderComponent(CgsGui::ImRendererSet* lpRendererSet)
{
    if (mpGuiCache == 0)
    {
        return;
    }

    // `if (TO.font == the null handle) skip` -- no ticker font adopted yet.
    if (mTextObject.mpFont.mpResourceMemory == 0)
    {
        return;
    }

    // The run gate: (game mode NONE/-1 or 15 || fading out || autosave || disconnect)
    // AND enabled -- OR any queued custom message (which runs regardless of 145).
    const s32 liGameModeType = mpGuiCache->GetCurrentGameModeType();   // cache+40536
    const bool lbAmbientWindow =
        (liGameModeType == -1 || liGameModeType == 15) ||
        meUpdateStage == E_UPDATESTAGE_FADINGOUT ||
        mbShowAutoSaveMessage || mbGamePausedForDisconnect;

    if (!(mbEnabled && lbAmbientWindow))
    {
        s32 liCustomCount = 0;
        for (InGameMessage* lp = mpCurrentMessage; lp != 0; lp = lp->mpNextMessage)
        {
            if (lp->mbIsCustomMessage)
            {
                ++liCustomCount;
            }
        }
        if (liCustomCount <= 0)
        {
            return;
        }
    }

    UpdateTickerMode();

    // The frame's timestep comes off the cache head (GuiEventTimeInfo's delta -- the
    // console reads *(cache + 0) here and decrements the state clock by it).
    const f32 lfTimeStep = mpGuiCache->GetTimeStep();
    mfTimeRemainingInState -= lfTimeStep;

    // The 2D command buffer (set slot 0 -- the console v15 = *a2).
    CgsGui::AptIm2dRenderBuffer* lpAptBuffer =
        *reinterpret_cast<CgsGui::AptIm2dRenderBuffer* const*>(lpRendererSet);
    if (lpAptBuffer == 0)
    {
        return;
    }
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrCmd =
        lpAptBuffer->mCommandBuffer;
    CgsGraphics::Im2dRenderBuffer* lpBuffer =
        reinterpret_cast<CgsGraphics::Im2dRenderBuffer*>(lpAptBuffer);

    // Publish the batch transform. The CONSOLE builds the canonical screen->NDC block
    // ({1/640, -1/360, -1, +1} + the aspect fold) because its GPU consumed NDC. The PC
    // dispatch walk (CgsImRenderBufferTemplate.cpp RENDER_PRIMITIVES) consumes
    // transforms in the Apt SetVertexMatrix convention instead: local -> LOGICAL SCREEN
    // pixels, which the dispatch then scales to the back buffer itself. Publishing the
    // console NDC block here collapsed every ticker vertex to a sub-pixel at the
    // top-left (the 14:04 boot). The renderer's vertices are ALREADY logical-screen
    // pixels, so the PC-correct batch transform is the screen-space identity -- it
    // must still be PUBLISHED (not skipped) to override whatever transform the Apt
    // walk latched last.
    {
        CgsGraphics::Im2dTransform lTransform = {};
        lTransform.mOriginXYZ.x  = 0.0f;
        lTransform.mOriginXYZ.y  = 0.0f;
        lTransform.mRightUp.x    = 1.0f;   // right = (1, 0)
        lTransform.mRightUp.w    = 1.0f;   // up    = (0, 1)
        // Colour lanes ride the Apt CXForm units through this dispatch: the fold is
        // (vertex/255) * (scale/255), so the IDENTITY scale is 255, not 1 (a 1.0 here
        // crushed every ticker colour to 0x01 -- the raw=FFFFFFFF folded=01010101 boot).
        lTransform.mColourScale.x = 255.0f;
        lTransform.mColourScale.y = 255.0f;
        lTransform.mColourScale.z = 255.0f;
        lTransform.mColourScale.w = 255.0f;
        lrCmd.SetTransform(lTransform);
    }

    // [DIAG] NOT IN THE X360 BINARY -- the [tut-ticker] RENDER rung: proves the
    // component passed every early-out and reports each stage transition (low-noise:
    // prints only when the stage changes; first 24).
    {
        static s32 siLastStage = -999;
        static s32 siDiagLeft  = 24;
        if (static_cast<s32>(meUpdateStage) != siLastStage && siDiagLeft > 0 &&
            CgsDev::Log::gpDebugPrint != 0)
        {
            --siDiagLeft;
            s32 liQueuedDiag = 0;
            for (InGameMessage* lp = mpCurrentMessage; lp != 0; lp = lp->mpNextMessage)
            {
                ++liQueuedDiag;
            }
            *CgsDev::Log::gpDebugPrint
                << "[tut-ticker] RenderComponent stage " << siLastStage << " -> "
                << static_cast<s32>(meUpdateStage)
                << " mode=" << static_cast<s32>(meTickerMode)
                << " queued=" << liQueuedDiag << "\n";
            siLastStage = static_cast<s32>(meUpdateStage);
        }
    }

    switch (meUpdateStage)
    {
        case E_UPDATESTAGE_NOTDISPLAYED:   // 0 -- wait, then buffer + open
        {
            mu8TextAlpha           = 0;
            mu8BackgroundAlpha     = 0;
            mu8BackgroundAlphaPeak = 0;
            if (mfTimeRemainingInState <= 0.0f)
            {
                mu8NumRoadRuleScoresShown = 0;
                BufferMessagesForGameMode();

                s32 liQueued = 0;
                for (InGameMessage* lp = mpCurrentMessage; lp != 0; lp = lp->mpNextMessage)
                {
                    ++liQueued;
                }

                if (meTickerMode == E_TICKERMODE_ROADRULES || liQueued > 0)
                {
                    meUpdateStage          = E_UPDATESTAGE_FADINGIN;
                    mfTimeRemainingInState = KF_MESSAGE_FADE_TIME;
                    ResetYPos();

                    if (mpOutputEventQueue != 0)
                    {
                        const u8 lu8Visible = 1;
                        mpOutputEventQueue->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lu8Visible), 538, 1);

                        // The console's "CodeTicker" fade-in audio ping (event 457).
                        GuiAudioTriggerEvent lAudio;
                        lAudio.Construct(0, "", "CodeTicker");
                        mpOutputEventQueue->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lAudio), 457, 100);
                    }
                }
                else
                {
                    mfTimeRemainingInState = KF_MESSAGE_DELAY_TIME;   // idle: re-poll in 0.8s
                }
            }
            break;
        }

        case E_UPDATESTAGE_FADINGIN:   // 1 -- background alpha 0 -> 128 over 0.3s
        {
            mu8TextAlpha = 0;
            f32 lfT = 1.0f - (mfTimeRemainingInState * 3.3333333f);   // 0..1
            if (lfT < 0.0f) lfT = 0.0f;
            if (lfT > 1.0f) lfT = 1.0f;
            const u8 lu8Alpha = static_cast<u8>(lfT * 128.0f);
            mu8BackgroundAlpha     = lu8Alpha;
            mu8BackgroundAlphaPeak = lu8Alpha;
            DrawBackground(lpBuffer);

            if (mfTimeRemainingInState <= 0.0f)
            {
                mfTimeRemainingInState = 0.0f;
                mfTextStartPosX        = KF_MESSAGE_START_POS_X;
                // Console tail: mode 6 -> stage 4, else stage = (mode==4)+2. (The reconnect
                // mode's stage-5 arm is folded by Hex-Rays; modes map 6->4, 7->5, 4->3,
                // rest->2, matching the displaying stages one-to-one.)
                if (meTickerMode == E_TICKERMODE_AUTOSAVE_MESSAGE)
                {
                    meUpdateStage = E_UPDATESTAGE_DISPLAYING_AUTOSAVE_MESSAGE;
                }
                else if (meTickerMode == E_TICKERMODE_RECONNECT_CONTROLLER_MESSAGE)
                {
                    meUpdateStage = E_UPDATESTAGE_RECONNECT_CONTROLLER_MESSAGE;
                }
                else
                {
                    meUpdateStage = (meTickerMode == E_TICKERMODE_ROADRULES)
                                        ? E_UPDATESTAGE_DISPLAYING_ROADRULES
                                        : E_UPDATESTAGE_DISPLAYING_MESSAGES;
                }
            }
            break;
        }

        case E_UPDATESTAGE_DISPLAYING_MESSAGES:   // 2 -- scroll + draw
        {
            mu8TextAlpha           = KU_MESSAGE_MAX_ALPHA;
            mu8BackgroundAlpha     = KU_BACKGROUND_MAX_ALPHA;
            mu8BackgroundAlphaPeak = KU_BACKGROUND_MAX_ALPHA;
            mfTextStartPosX +=
                KAF_MESSAGE_SCROLL_SPEED[meTickerMode] * lfTimeStep;
            DrawBackground(lpBuffer);
            DrawMessages(lpBuffer);

            if (mpCurrentMessage == 0)
            {
                meUpdateStage          = E_UPDATESTAGE_FADINGOUT;
                mfTimeRemainingInState = KF_MESSAGE_FADE_TIME;
                if (mpOutputEventQueue != 0)
                {
                    GuiAudioTriggerEvent lAudio;
                    lAudio.Construct(1, "", "CodeTicker");
                    mpOutputEventQueue->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lAudio), 457, 100);
                }
            }
            break;
        }

        case E_UPDATESTAGE_DISPLAYING_ROADRULES:   // 3 -- the road-rules feed
        {
            // [FLAG PARTIAL, named]: the interval-message refill (off_82F2585C ids at
            // every 10th score) is parked with the road-rules feed. The scroll/draw
            // spine and the drain-to-fadeout tail are the console's.
            mu8TextAlpha           = KU_MESSAGE_MAX_ALPHA;
            mu8BackgroundAlpha     = KU_BACKGROUND_MAX_ALPHA;
            mu8BackgroundAlphaPeak = KU_BACKGROUND_MAX_ALPHA;
            mfTextStartPosX +=
                KAF_MESSAGE_SCROLL_SPEED[meTickerMode] * lfTimeStep;
            DrawBackground(lpBuffer);
            DrawMessages(lpBuffer);

            const bool lbOutstanding = RequestNewRoadRulesScore();
            if (mpCurrentMessage == 0 && !lbOutstanding)
            {
                mfTimeRemainingInState = KF_MESSAGE_FADE_TIME;
                meUpdateStage          = E_UPDATESTAGE_FADINGOUT;
                if (mbShowingBreakingNews)
                {
                    mbShowingBreakingNews = false;
                    mRoadRulesToShow      = CgsContainers::BitArray<64u>();
                }
                else
                {
                    miLastRoadRuleScoreReceived = -1;
                }
            }
            break;
        }

        case E_UPDATESTAGE_DISPLAYING_AUTOSAVE_MESSAGE:   // 4
        case E_UPDATESTAGE_RECONNECT_CONTROLLER_MESSAGE:  // 5
        {
            mu8TextAlpha           = KU_MESSAGE_MAX_ALPHA;
            mu8BackgroundAlpha     = KU_BACKGROUND_MAX_ALPHA;
            mu8BackgroundAlphaPeak = KU_BACKGROUND_MAX_ALPHA;
            mfTextStartPosX +=
                KAF_MESSAGE_SCROLL_SPEED[meTickerMode] * lfTimeStep;
            DrawBackground(lpBuffer);
            DrawMessages(lpBuffer);
            break;
        }

        case E_UPDATESTAGE_FADINGOUT:   // 6 -- background alpha peak -> 0 over 0.3s
        {
            mu8TextAlpha = 0;
            f32 lfT = mfTimeRemainingInState * 3.3333333f;   // 1..0
            if (lfT < 0.0f) lfT = 0.0f;
            if (lfT > 1.0f) lfT = 1.0f;
            mu8BackgroundAlpha =
                static_cast<u8>(lfT * static_cast<f32>(mu8BackgroundAlphaPeak));
            DrawBackground(lpBuffer);

            if (mfTimeRemainingInState <= 0.0f)
            {
                meUpdateStage          = E_UPDATESTAGE_NOTDISPLAYED;
                mfTimeRemainingInState = KF_MESSAGE_DELAY_TIME;
                if (mpOutputEventQueue != 0)
                {
                    const u8 lu8Visible = 0;
                    mpOutputEventQueue->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lu8Visible), 538, 1);
                }
            }
            break;
        }

        default:
            CGS_ASSERT(false, "Unhandled state in InGameMessageRenderer::Update\n");   // :1067
            break;
    }
}
}   // namespace BrnGui
