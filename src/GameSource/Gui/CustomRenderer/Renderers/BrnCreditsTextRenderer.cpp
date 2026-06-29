#include "BrnCreditsTextRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                   // CgsIDCompress
#include "GameShared/GameClasses/Core/CgsStringUtils.h"          // CgsCore::SPrintf
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"  // CgsLanguage::LanguageManager::FindString

// BrnGui::CreditsTextRenderer bodies -- faithful X360 ARTIST ports.
//
// The simple accessors (Construct/GetID/Prepare/SetTextRenderer/SetRenderEnabled) are
// recovered store-for-store. RecalculateParagraphs and RenderComponent are reconstructed
// from the PPC asm (the Hex-Rays output was register-mangled -- "local variable allocation
// has failed"); Update is dominated by an inlined VPU matrix build (the screen transform),
// reconstructed for its scalar scroll/fade tail and left with the transform build flagged.
//
// FLAGGED -- UNRECOVERED .data/.rodata CONSTANTS:
//   This TU defines a band of file-scope float layout constants (the DWARF names them
//   KF_TEXTBOX_* / KF_*_TEXT_SIZE / KF_SCROLL_* / KF_FADE_* / KF_PARAGRAPH_SPACING* /
//   KF_CREDITS_DROPSHADOW_* / KF_DROPSHADOW_ALPHA / KI_CREDITS_BRIGHTNESS). The dossier
//   exposes only the rodata ADDRESSES the asm loads (flt_82F258A4, flt_82F258A8, ...),
//   not their byte values, so the actual numbers are genuinely unrecoverable here. Per
//   project policy they are NOT fabricated: each is an honest 0-initialised placeholder
//   tagged `// FLAGGED: <addr> value unrecovered`. The control flow / field wiring around
//   them is faithful; only the literal magnitudes are pending a rodata recovery pass.
//   Likewise the static credits name table (off_82F258E8 "Director people") and the four
//   immediate-buffer batch helpers (sub_824587B0 / sub_82458EC0 / sub_82458898 /
//   sub_821F0E50) are flagged where they appear.

namespace BrnGui
{
namespace
{
    // ------------------------------------------------------------------------------------
    // FLAGGED placeholders for this TU's unrecovered file-scope float constants. Names are
    // the DWARF names (BrnCreditsTextRenderer.cpp:23-40); VALUES are placeholders.
    // ------------------------------------------------------------------------------------
    const f32 KF_TEXTBOX_CENTRE_X     = 0.0f; // FLAGGED: flt_82F25? value unrecovered (DWARF KF_TEXTBOX_CENTRE_X)
    const f32 KF_TEXTBOX_CENTRE_Y     = 0.0f; // FLAGGED: value unrecovered (KF_TEXTBOX_CENTRE_Y)
    const f32 KF_TEXTBOX_WIDTH        = 0.0f; // FLAGGED: value unrecovered (KF_TEXTBOX_WIDTH)
    const f32 KF_TEXTBOX_HEIGHT       = 0.0f; // FLAGGED: value unrecovered (KF_TEXTBOX_HEIGHT)
    const f32 KF_TEXTBOX_ANGLE        = 0.0f; // FLAGGED: value unrecovered (KF_TEXTBOX_ANGLE)
    const f32 KF_LARGE_TEXT_SIZE      = 0.0f; // FLAGGED: flt_82F258B4 value unrecovered (title font height)
    const f32 KF_SMALL_TEXT_SIZE      = 0.0f; // FLAGGED: flt_82F258B0 value unrecovered (detail font height)
    const f32 KF_PARAGRAPH_SPACING0   = 0.0f; // FLAGGED: unk_82F258B8 value unrecovered (detail-paragraph spacing)
    const f32 KF_PARAGRAPH_SPACING1   = 0.0f; // FLAGGED: unk_82F258BC value unrecovered (title-paragraph spacing)
    const f32 KF_SCROLL_START         = 0.0f; // FLAGGED: flt_82F258C0 value unrecovered (initial scroll)
    const f32 KF_SCROLL_SPEED         = 0.0f; // FLAGGED: flt_82F258C4 value unrecovered (scroll/sec)
    const f32 KF_FADE_BORDER          = 0.0f; // FLAGGED: flt_82F258C8 value unrecovered (bottom fade band)
    const f32 KF_FADE_IN_START        = 0.0f; // FLAGGED: flt_82F258CC value unrecovered (initial fade)
    const f32 KF_FADE_IN_SPEED        = 0.0f; // FLAGGED: flt_82F258D0 value unrecovered (fade/sec)
    const f32 KF_CREDITS_DROPSHADOW_X = 0.0f; // FLAGGED: flt_82F258D4 value unrecovered (shadow X offset)
    const f32 KF_CREDITS_DROPSHADOW_Y = 0.0f; // FLAGGED: flt_82F258D8 value unrecovered (shadow Y offset)
    const f32 KF_DROPSHADOW_ALPHA     = 0.0f; // FLAGGED: flt_82F258E0 value unrecovered (shadow alpha scale)

    // Right edge of the text box (flt_82F258A4) and the box bottom margin (flt_82F258A8)
    // the asm folds in repeatedly. FLAGGED: values unrecovered.
    const f32 KF_TEXTBOX_RIGHT        = 0.0f; // FLAGGED: flt_82F258A4 value unrecovered
    const f32 KF_TEXTBOX_BOTTOM       = 0.0f; // FLAGGED: flt_82F258A8 value unrecovered

    // 1.0f the asm materialises from flt_82001CC0 for the autosize/colour-clamp paths.
    const f32 KF_ONE = 1.0f;

    // The fade-band extent constants passed to RenderStringFadingY's shadow pass: a fade-in
    // ramp length (flt_82057DD8) and a +100 unit Y margin (flt_820049E0 == 100.0). The margin
    // is recovered from the literal; the ramp length is unrecovered.
    const f32 KF_SHADOW_FADE_RAMP = 0.0f;   // FLAGGED: flt_82057DD8 value unrecovered
    const f32 KF_FADE_Y_MARGIN    = 100.0f; // flt_820049E0 (recovered literal)

    // Per-frame timestep the X360 multiplies the scroll/fade speeds by (0.016666668 ==
    // 1/60s, flt_820139F8). Recovered from the literal in the asm.
    const f32 KF_FRAME_DT = 0.016666668f;

    // The static credits name table the Construct loop counts (off_82F258E8, "Director
    // people"). FLAGGED: table contents unrecovered; its entry count seeds miNumStrings.
    // Modelled as an empty (NULL-terminated) table so the count is a faithful 0 until the
    // rodata is recovered.
    const char* const KAPC_CREDITS_NAME_TABLE[] = { 0 }; // FLAGGED: off_82F258E8 contents unrecovered

    // The X360 default (invalid) resource-font handle qword (qword_82FB3688) Construct copies
    // into both font handles. It is the null/invalid handle (matches TextObject::Construct's
    // own mpFont reset); modelled as a default-cleared SafeResourceHandle.
    void lClearToDefaultFontHandle(CgsResource::SafeResourceHandle<CgsResource::Font>& lrHandle)
    {
        lrHandle.mpResourceMemory = 0;
        lrHandle.mpSourceEntry    = 0;
    }
}

// 0x8244EA38: copy the default font handle into both handles, clear the credits type and
// the paragraph count, count the static credits name-table entries into the count, then
// zero the paragraph array.
void CreditsTextRenderer::Construct()
{
    // stw qword_82FB3688 -> +0x60/+0x64 (mpTitleFont) and +0x58/+0x5C (mpNormalFont).
    lClearToDefaultFontHandle(mpTitleFont);
    lClearToDefaultFontHandle(mpNormalFont);

    meCreditsType = E_CREDITS_TYPE_END; // stw 0, 0x20C4
    miNumStrings  = 0;                  // stw 0, 0x68

    // do { ++miNumStrings; } while (off_82F258E8[miNumStrings]); -- count the static name
    // table entries (the loop runs only if the table is non-empty).
    if (KAPC_CREDITS_NAME_TABLE[0] != 0)
    {
        do
        {
            ++miNumStrings;
        }
        while (KAPC_CREDITS_NAME_TABLE[miNumStrings] != 0);
    }

    // memset(this + 0x6C, 0, 8000) -- clear the KI_MAX_PARAGRAPHS paragraph records.
    for (s32 li = 0; li < KI_MAX_PARAGRAPHS; ++li)
    {
        maParagraphs[li].mfHeight   = 0.0f;
        maParagraphs[li].mfPosition = 0.0f;
        maParagraphs[li].mpText     = 0;
        maParagraphs[li].mbTitle    = false;
    }
}

// 0x82447190: store the heap allocator (guest stores arg3 at +0x08) and report success.
bool CreditsTextRenderer::Prepare(rw::IResourceAllocator* lpHeapAllocator)
{
    mpHeapAllocator = lpHeapAllocator; // stw r5, 8(r11)
    return true;                       // li r3, 1
}

// 0x824471A0: the renderer id.
CgsID CreditsTextRenderer::GetID() const
{
    return CgsIDCompress("CREDITS");
}

// 0x824471B0: store the shared text renderer (guest stores arg at +0x50).
void CreditsTextRenderer::SetTextRenderer(CgsGraphics::TextRenderer* lpTextRenderer)
{
    mpTextRenderer = lpTextRenderer; // stw r4, 0x50(r3)
}

// 0x82457BB8: store the enabled flag; when enabling, rebuild the paragraphs and reset the
// scroll/fade to their start values.
void CreditsTextRenderer::SetRenderEnabled(bool lbRenderEnabled)
{
    mbRenderEnabled = lbRenderEnabled; // stb r4, 4(r31)

    if (lbRenderEnabled)
    {
        RecalculateParagraphs();
        mfScroll = KF_SCROLL_START;  // stfs flt_82F258C0, 0x20A4
        mfFade   = KF_FADE_IN_START; // stfs flt_82F258CC, 0x20A8
    }
}

// 0x8244EAB0: (re)build the paragraph list from the localisation database. For each index
// look up its title + detail strings (CREDITS_*_%d, or REPLAY_CREDITS_*_%d for replay
// credits), set the text on the matching TextObject, measure its line count and height,
// and accumulate the scroll positions. Stops at the first missing pair (end credits) or
// after the fixed 3-entry replay set.
void CreditsTextRenderer::RecalculateParagraphs()
{
    // The two TextObjects are (re)constructed with default state, then styled. The X360 copies
    // mpTitleFont into mTitleTextObject (+0x1FAC) and mpNormalFont into mNormalTextObject
    // (+0x2028) over each object's leading mpFont handle (the stw r,0(obj)/stw r,4(obj) pairs),
    // sets the font height, the text-box rect, and the multiline / wordwrap / italic flags.
    mTitleTextObject.Construct(0, 0);
    mTitleTextObject.mpFont         = mpTitleFont;                       // stw {+0x60,+0x64}, 0(r18)/4(r18) -> mpFont
    mTitleTextObject.mbAutosize     = false;                            // stb 0, 0x1FF1
    mTitleTextObject.mfFontHeight   = KF_SMALL_TEXT_SIZE;               // stfs flt_82F258B0, 0x1FB4
    mTitleTextObject.mv2TopLeft.mX  = 0.0f;                             // stfs 0, 0x1FC4
    mTitleTextObject.mv2TopLeft.mY  = 0.0f;                             // stfs 0, 0x1FC8
    mTitleTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT;              // stfs flt_82F258A4, 0x1FCC
    mTitleTextObject.mv2BottomRight.mY = KF_TEXTBOX_BOTTOM;             // stfs flt_82F258A8, 0x1FD0
    mTitleTextObject.mbMultiLine    = 1;                                // stw 1, 0x1FD8
    mTitleTextObject.mbWordWrap     = 1;                                // stw 1, 0x1FDC
    mTitleTextObject.mbItalic       = 1;                                // stw 1, 0x1FE0

    mNormalTextObject.Construct(0, 0);
    mNormalTextObject.mpFont        = mpNormalFont;                     // stw {+0x58,+0x5C}, 0(r21)/4(r21) -> mpFont
    mNormalTextObject.mbAutosize    = false;                            // stb 0, 0x206D
    mNormalTextObject.mfFontHeight  = KF_LARGE_TEXT_SIZE;               // stfs flt_82F258B4, 0x2030
    mNormalTextObject.mv2TopLeft.mX = 0.0f;
    mNormalTextObject.mv2TopLeft.mY = 0.0f;
    mNormalTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT;
    mNormalTextObject.mv2BottomRight.mY = KF_TEXTBOX_BOTTOM;
    mNormalTextObject.mbMultiLine   = 1;                                // stw 1, 0x2054
    mNormalTextObject.mbWordWrap    = 1;                                // stw 1, 0x2058
    mNormalTextObject.mbItalic      = 1;                                // stw 1, 0x205C

    s32 liCount = 0;
    f32 lfScrollPos = 0.0f;

    // Guard (X360): only build when both the language manager and text renderer are set, and
    // neither font handle is still the default/invalid handle. (The asm tests handle halves
    // against the default qword_82FB3688; on the host that is "the handle is non-default".)
    if (mpTextRenderer != 0 && mpLanguageManager != 0 &&
        !mpTitleFont.IsNull() && !mpNormalFont.IsNull())
    {
        const char* const lpcDetailFmt      = "CREDITS_DETAIL_%d";
        const char* const lpcTitleFmt       = "CREDITS_TITLE_%d";
        const char* const lpcReplayDetailFmt = "REPLAY_CREDITS_DETAIL_%d";
        const char* const lpcReplayTitleFmt  = "REPLAY_CREDITS_TITLE_%d";

        char lacTitleKey[128];
        char lacDetailKey[128];

        for (s32 liIndex = 0; ; )
        {
            // Pick the key set by credits type (asserts an unknown type).
            const char* lpcTitleFmtSel;
            const char* lpcDetailFmtSel;
            if (meCreditsType == E_CREDITS_TYPE_END)
            {
                lpcTitleFmtSel  = lpcTitleFmt;
                lpcDetailFmtSel = lpcDetailFmt;
            }
            else if (meCreditsType == E_CREDITS_TYPE_REPLAY)
            {
                lpcTitleFmtSel  = lpcReplayTitleFmt;
                lpcDetailFmtSel = lpcReplayDetailFmt;
            }
            else
            {
                CGS_ASSERT(false, "Invalid type of credits to render : ");
                lpcTitleFmtSel  = lpcTitleFmt;
                lpcDetailFmtSel = lpcDetailFmt;
            }

            CgsCore::SPrintf(lacTitleKey, 128, lpcTitleFmtSel, liIndex);
            CgsCore::SPrintf(lacDetailKey, 128, lpcDetailFmtSel, liIndex);

            const CgsResource::CgsUtf8* lpTitleString =
                mpLanguageManager->FindString(lacTitleKey);
            const CgsResource::CgsUtf8* lpDetailString =
                mpLanguageManager->FindString(lacDetailKey);

            if (lpDetailString == 0)
            {
                // No detail string: for replay credits keep scanning the first three indices
                // (titles only); otherwise stop the whole build.
                if (meCreditsType != E_CREDITS_TYPE_REPLAY || liIndex >= 3)
                    break;
                ++liIndex;
                continue;
            }

            // Title paragraph (if present) -- styled on mTitleTextObject.
            if (lpTitleString != 0)
            {
                maParagraphs[liCount].mfPosition = lfScrollPos;
                maParagraphs[liCount].mpText     = lpTitleString;
                maParagraphs[liCount].mbTitle    = true;

                mTitleTextObject.mpUtf8String = lpTitleString;
                if (mTitleTextObject.mbWordWrap == 1)
                    mTitleTextObject.CalculateAutosizing();

                const CgsResource::CgsUtf8* lpLine = 0;
                const u32 luLines =
                    mTitleTextObject.GetNumLinesAndStartLine(0, &lpLine) - 1;
                const f32 lfHeight = static_cast<f32>(luLines) * mTitleTextObject.mfFontHeight;
                maParagraphs[liCount].mfHeight = lfHeight;
                lfScrollPos += lfHeight + KF_PARAGRAPH_SPACING0;
                ++liCount;
            }

            // Detail paragraph -- styled on mNormalTextObject.
            maParagraphs[liCount].mfPosition = lfScrollPos;
            maParagraphs[liCount].mpText     = lpDetailString;
            maParagraphs[liCount].mbTitle    = false;

            mNormalTextObject.mpUtf8String = lpDetailString;
            if (mNormalTextObject.mbWordWrap == 1)
                mNormalTextObject.CalculateAutosizing();

            const CgsResource::CgsUtf8* lpLine2 = 0;
            const u32 luLines2 =
                mNormalTextObject.GetNumLinesAndStartLine(0, &lpLine2) - 1;
            const f32 lfHeight2 = static_cast<f32>(luLines2) * mNormalTextObject.mfFontHeight;
            maParagraphs[liCount].mfHeight = lfHeight2;
            lfScrollPos += lfHeight2 + KF_PARAGRAPH_SPACING1;
            ++liCount;

            ++liIndex;
        }
    }

    miNumStrings = liCount; // stw r23, 0x68(this)
}

// 0x8245DE40: rebuild the paragraphs, then -- once faded in -- draw every in-view paragraph
// in two passes through TextRenderer::RenderStringFadingY: a dropped/offset shadow pass and
// the main pass. Each pass walks maParagraphs, keeps the ones overlapping the visible band,
// positions the matching TextObject's box and colour, and renders with the top/bottom fade.
void CreditsTextRenderer::RenderComponent(ImRendererSet* lpRendererSet)
{
    RecalculateParagraphs();

    // Nothing to draw until the fade-in has started.
    if (mfFade <= 0.0f)
        return;

    CgsGraphics::Im2dRenderBuffer* lpBuffer = lpRendererSet->mpIm2dRenderBuffer;

    // FLAGGED: sub_824587B0(buffer+4) -- open the immediate-buffer batch for this submission
    // (an Im2dRenderBuffer batch-begin reached at buffer+4). External callee, body pending.
    // lBatchBegin(lpBuffer);

    lpBuffer->SetTransform(mScreenTransform);

    // --- Pass 1: the dropped shadow. ---
    for (s32 liPara = 0; liPara < miNumStrings; ++liPara)
    {
        const ParagraphInfo& lrPara = maParagraphs[liPara];
        if (lrPara.mfPosition <= (mfScroll + KF_TEXTBOX_BOTTOM) &&
            (lrPara.mfPosition + lrPara.mfHeight) >= mfScroll)
        {
            const f32 lfTopY = (lrPara.mfPosition - mfScroll) + KF_CREDITS_DROPSHADOW_Y;
            CgsGraphics::TextObject* lpObject;
            if (lrPara.mbTitle)
            {
                mTitleTextObject.mv2TopLeft.mX  = KF_CREDITS_DROPSHADOW_X;
                mTitleTextObject.mv2TopLeft.mY  = lfTopY;
                mTitleTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT + KF_CREDITS_DROPSHADOW_X;
                mTitleTextObject.mv2BottomRight.mY =
                    ((lrPara.mfPosition + lrPara.mfHeight) - mfScroll) + KF_CREDITS_DROPSHADOW_Y;
                mTitleTextObject.mpUtf8String   = lrPara.mpText;
                if (mTitleTextObject.mbWordWrap == 1)
                    mTitleTextObject.CalculateAutosizing();
                // The shadow alpha == clamp(KF_DROPSHADOW_ALPHA * mfFade) packed into the top byte.
                const s32 liAlpha = static_cast<s32>(KF_DROPSHADOW_ALPHA * mfFade + 0.5f);
                mTitleTextObject.mTextColour = static_cast<CgsGraphics::RGBA>(liAlpha << 24);
                lpObject = &mTitleTextObject;
            }
            else
            {
                mNormalTextObject.mv2TopLeft.mX  = KF_CREDITS_DROPSHADOW_X;
                mNormalTextObject.mv2TopLeft.mY  = lfTopY;
                mNormalTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT + KF_CREDITS_DROPSHADOW_X;
                mNormalTextObject.mv2BottomRight.mY =
                    ((lrPara.mfPosition + lrPara.mfHeight) - mfScroll) + KF_CREDITS_DROPSHADOW_Y;
                mNormalTextObject.mpUtf8String   = lrPara.mpText;
                if (mNormalTextObject.mbWordWrap == 1)
                    mNormalTextObject.CalculateAutosizing();
                const s32 liAlpha = static_cast<s32>(KF_DROPSHADOW_ALPHA * mfFade + 0.5f);
                mNormalTextObject.mTextColour = static_cast<CgsGraphics::RGBA>(liAlpha << 24);
                lpObject = &mNormalTextObject;
            }

            // Shadow pass fade band (asm f1..f4): top pivot 0.0, ramp KF_SHADOW_FADE_RAMP,
            // lower-fade start KF_TEXTBOX_BOTTOM + 100, lower-fade end KF_TEXTBOX_BOTTOM.
            mpTextRenderer->RenderStringFadingY(lpBuffer, *lpObject, 0.0f, KF_SHADOW_FADE_RAMP,
                                                KF_TEXTBOX_BOTTOM + KF_FADE_Y_MARGIN,
                                                KF_TEXTBOX_BOTTOM);
        }
    }

    // FLAGGED: byte_82FB3784 / sub_82458EC0(buffer+4, dword_83010F24) -- an optional batch
    // state switch (a debug/blend toggle) before the main pass. External callee + flag, body
    // pending.
    // if (gbCreditsBatchToggle) sub_82458EC0(lpBuffer, KU_CreditsBatchState);

    // --- Pass 2: the main coloured text. ---
    for (s32 liPara = 0; liPara < miNumStrings; ++liPara)
    {
        const ParagraphInfo& lrPara = maParagraphs[liPara];
        if (lrPara.mfPosition <= (mfScroll + KF_TEXTBOX_BOTTOM) &&
            (lrPara.mfPosition + lrPara.mfHeight) >= mfScroll)
        {
            CgsGraphics::TextObject* lpObject;
            if (lrPara.mbTitle)
            {
                mTitleTextObject.mv2TopLeft.mY  = lrPara.mfPosition - mfScroll;
                mTitleTextObject.mv2TopLeft.mX  = 0.0f;
                mTitleTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT;
                mTitleTextObject.mv2BottomRight.mY =
                    (lrPara.mfPosition + lrPara.mfHeight) - mfScroll;
                mTitleTextObject.mpUtf8String   = lrPara.mpText;
                if (mTitleTextObject.mbWordWrap == 1)
                    mTitleTextObject.CalculateAutosizing();
                // Main colour: a grey (KI_CREDITS_BRIGHTNESS replicated into RGB) with the
                // fade alpha (clamp(mfFade * 255) << 24) in the top byte. FLAGGED:
                // dword_82F258DC (the brightness byte) value unrecovered -> 0.
                const s32 liAlpha = static_cast<s32>(mfFade * 255.0f + 0.5f) & 0xFF;
                const u32 luBrightness = 0u; // FLAGGED: dword_82F258DC value unrecovered
                mTitleTextObject.mTextColour = static_cast<CgsGraphics::RGBA>(
                    (((((liAlpha << 8) | luBrightness) << 8) | luBrightness) << 8) | luBrightness);
                lpObject = &mTitleTextObject;
            }
            else
            {
                mNormalTextObject.mv2TopLeft.mY  = lrPara.mfPosition - mfScroll;
                mNormalTextObject.mv2TopLeft.mX  = 0.0f;
                mNormalTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT;
                mNormalTextObject.mv2BottomRight.mY =
                    (lrPara.mfPosition + lrPara.mfHeight) - mfScroll;
                mNormalTextObject.mpUtf8String   = lrPara.mpText;
                if (mNormalTextObject.mbWordWrap == 1)
                    mNormalTextObject.CalculateAutosizing();
                const s32 liAlpha = static_cast<s32>(mfFade * 255.0f + 0.5f) & 0xFF;
                const u32 luBrightness = 0u; // FLAGGED: dword_82F258DC value unrecovered
                mNormalTextObject.mTextColour = static_cast<CgsGraphics::RGBA>(
                    (((((liAlpha << 8) | luBrightness) << 8) | luBrightness) << 8) | luBrightness);
                lpObject = &mNormalTextObject;
            }

            // Main pass fade band (asm f1..f4): top pivot 0.0, fade border KF_FADE_BORDER,
            // lower-fade start KF_TEXTBOX_BOTTOM - KF_FADE_BORDER, lower-fade end KF_TEXTBOX_BOTTOM.
            mpTextRenderer->RenderStringFadingY(lpBuffer, *lpObject, 0.0f, KF_FADE_BORDER,
                                                KF_TEXTBOX_BOTTOM - KF_FADE_BORDER,
                                                KF_TEXTBOX_BOTTOM);
        }
    }

    // FLAGGED: sub_82458898(buffer+4) -- close / submit the immediate-buffer batch. External
    // callee, body pending.
    // lBatchEnd(lpBuffer);
}

// 0x824575F0: advance the scroll + fade while enabled, wrap the scroll once the last
// paragraph has left the top of the box, and clamp the fade to 1.0. The leading body builds
// the rotated + aspect-corrected screen transform from the text box (an inlined VPU matrix
// build); that part is flagged -- only the scalar scroll/fade tail is reconstructed here.
void CreditsTextRenderer::Update()
{
    // FLAGGED: the X360 first rebuilds mScreenTransform from the credits text-box centre /
    // size / angle (KF_TEXTBOX_CENTRE_X/Y, KF_TEXTBOX_WIDTH/HEIGHT, KF_TEXTBOX_ANGLE) via an
    // inlined rw::math::vpu matrix path (vperm/vmaddfp ...), then folds in the display aspect
    // ratio. That transform build is not faithfully reconstructable from the VPU asm and its
    // box constants are unrecovered; left as a flagged gap. The scalar scroll/fade update
    // below IS faithful.
    // BuildScreenTransform();   // -> mScreenTransform (FLAGGED: VPU transform build)

    if (mbRenderEnabled)
    {
        mfScroll += KF_SCROLL_SPEED * KF_FRAME_DT; // *(this+0x20A4) += flt_82F258C4 * 1/60
        mfFade   += KF_FADE_IN_SPEED * KF_FRAME_DT; // *(this+0x20A8) += flt_82F258D0 * 1/60
    }

    // Wrap the scroll once the credits column has fully scrolled off the top, then snap it back
    // to -KF_TEXTBOX_BOTTOM. FLAGGED (register-mangled fn): the X360 forms the wrap limit from
    // two `this`-relative loads into the paragraph region --
    //   f0  = *(f32*)(this + (miNumStrings + 6) * 16)
    //   f12 = *(f32*)(this + miNumStrings * 16 + 0x5C)
    //   limit = f0 + f12 + KF_TEXTBOX_BOTTOM
    // The array base is +0x6C (not a 16-aligned offset), so this byte arithmetic does not map
    // cleanly onto named maParagraphs[] members; the named approximation below preserves the
    // intent (last paragraph's position + height + the box margin) but is NOT byte-exact to the
    // guest's index pairing. Reconfirm against the rodata/offset pass.
    const f32 lfWrapLimit =
        maParagraphs[miNumStrings + 6].mfPosition +
        maParagraphs[miNumStrings].mfHeight +
        KF_TEXTBOX_BOTTOM;
    if (mfScroll > lfWrapLimit)
        mfScroll = -KF_TEXTBOX_BOTTOM;

    if (mfFade > KF_ONE)
        mfFade = KF_ONE;
}
}
