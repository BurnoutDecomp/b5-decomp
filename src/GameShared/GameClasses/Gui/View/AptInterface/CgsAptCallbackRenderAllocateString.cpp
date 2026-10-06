#include "SDKs/EATech/include/Apt/AptTextLayoutDiagnosticsPC.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"  // AptCallbackRender::AllocateString + the AcquireStringSlot bridge
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptString.h"          // the REAL CgsGui::CgsAptString (text object) + Prepare

// =============================================================================
// CgsGui::AptCallbackRender::AllocateString - PS3 0x5C7260.
//
// This is the ONE render callback that needs the REAL CgsGui::CgsAptString text object (to call
// CgsAptString::Prepare @0x5C6B20). Its TU therefore includes CgsAptString.h and CANNOT include
// CgsAptRenderHandler.h -- that header models the pooled CgsAptString opaquely (a 128-byte slot),
// which is a conflicting definition of the SAME guest type. The opaque-world half (free the
// previous string, hand out a free pool slot + char buffer, surface the font collection / effect /
// size scale) is done by AptCallbackRender::AcquireStringSlot in CgsAptCallbackRender.cpp;
// here we cast the slot back to the real CgsAptString and lay the string out.
//
// Faithful to the dossier:
//   1. (eFlags & 0x406) == 0  -> DeallocateString(pCurrString, 0)          [inside the bridge]
//   2. slot = AptRenderHandler::GetUnusedAptString(&buf, szString)          [inside the bridge]
//   3. CgsAptString::Prepare(slot, fonts, params, buf, effect, sizeScale)   [here]
//   4. return slot
// =============================================================================

namespace CgsGui
{
    AptAssetString AptCallbackRender::AllocateString(AptAllocateStringParameters* lpParameters)
    {
        // The opaque-world half: free the previous string (per flags), carve a free pool slot +
        // its char buffer, and surface the text-layout inputs Prepare needs.
        CgsUnicode::CgsUtf8* lpStringBuffer = nullptr;
        const void*          lpFonts        = nullptr;
        s32                  liEffect       = 0;
        f32                  lfSizeScale    = 1.0f;
        s32                  liZID          = 0;
        void* lpSlot = AcquireStringSlot(
            lpParameters, &lpStringBuffer, &lpFonts, &liEffect, &lfSizeScale, &liZID);

        // Guard the shape-only bring-up: when no FontCollection is wired (title-screen shapes
        // don't need text layout), the slot has nothing to lay out -- return the unresolved
        // handle rather than asserting in CgsAptString::Prepare (which requires lpFonts).
        if (lpSlot == nullptr || lpFonts == nullptr)
            return reinterpret_cast<AptAssetString>(static_cast<intptr_t>(0));

        // Lay the parametrised string out into the slot's char buffer with the font collection,
        // the requested effect, and the size scale (the guest's CgsAptString::Prepare call).
        CgsAptString* lpAptString = reinterpret_cast<CgsAptString*>(lpSlot);
        lpAptString->Prepare(
            reinterpret_cast<const FontCollection*>(lpFonts),
            lpParameters,
            lpStringBuffer,
            static_cast<CgsAptString::ETextEffects>(liEffect),
            lfSizeScale);

        // Default-off observation of the exact callback input/output, in their original local units.
        if (AptTextLayoutDiagnosticsPC::Register(liZID,
                reinterpret_cast<const char*>(lpAptString->mTextObject.mpUtf8String)))
        {
            const CgsGraphics::TextObject& text = lpAptString->mTextObject;
            char line[768];
            std::snprintf(line, sizeof(line),
                "[apt-text-layout] alloc id=%d caption='%s' box=%.6f,%.6f,%.6f,%.6f align=%d boxAlign=%d flags=%u fontIn=%.6f textBox=%.6f,%.6f,%.6f,%.6f textAlign=%d font=%.6f widthEm=%.6f measured=%.6f,%.6f auto=%d wrap=%d multi=%d",
                liZID, AptTextLayoutDiagnosticsPC::Caption(AptTextLayoutDiagnosticsPC::Kind(liZID)),
                lpParameters->x0,lpParameters->y0,lpParameters->x1,lpParameters->y1,
                static_cast<int>(lpParameters->eAlignment),static_cast<int>(lpParameters->eBoxAlignment),
                lpParameters->eFlags,lpParameters->fFontHeight,
                text.mv2TopLeft.mX,text.mv2TopLeft.mY,text.mv2BottomRight.mX,text.mv2BottomRight.mY,
                static_cast<int>(text.meAlignment),text.mfFontHeight,text.mfStringWidth,
                lpParameters->fTextWidth,lpParameters->fTextHeight,
                static_cast<int>(text.mbAutosize),static_cast<int>(text.mbWordWrap),static_cast<int>(text.mbMultiLine));
            AptTextLayoutDiagnosticsPC::Emit(line);
        }

        // Return the x64 render-data handle (slotIndex + 1) the engine stores in mZID.
        return reinterpret_cast<AptAssetString>(static_cast<intptr_t>(liZID));
    }
}
