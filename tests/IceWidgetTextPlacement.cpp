// Where the ICE editor's info list and logo put their text, checked on the production
// ICEWidgetInfoList.cpp and ICEWidgetLogo.cpp linked against a recording ICERender.
//
// ARTIST passes the editor's ICERender in r3 to every ICERender call, so the text
// position starts at r4. InfoList::Render (0x82534640) draws row i at
// X = (int)this+0x25C, Y = (int)this+0x260 - 18*i, size 16, the row text as the format
// (0x825346B8); Logo::Render (0x825346E0) draws "ICE" at X = (int)mfX, Y = (int)mfY,
// size 64 (0x82534934). Reading the render pointer as X shifts every coordinate by one.
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "SDKs/Packages/ICE/ICEMath.hpp"
#include "SDKs/Packages/ICE/ICERender.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetInfoList.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetLogo.hpp"
#include "rw/core/stdc/stdc.h"

// Incidental callees of the two TUs: the row formatter and the logo's pulse maths. The
// pulse only feeds the logo's inert varargs, so a flat phase is enough here.
namespace rw { namespace core { namespace stdc {
    s32 Vsnprintf(char* lpcDst, s32 liSize, const char* lpcFormat, va_list lvaArgs)
    {
        return std::vsnprintf(lpcDst, liSize, lpcFormat, lvaArgs);
    }
} } }
namespace ICE
{
    void Angle::SetFromVecFloat(VecFloat) {}
    f32  ICEMath::Sin(Angle) { return 0.0f; }
}

struct TextCall
{
    s32         miX;
    s32         miY;
    f32         mfSize;
    std::string mFormat;
};
static std::vector<TextCall> gCalls;

// The recording ICERender. These definitions compile against either declaration shape
// (a static or a member function is defined with the same syntax) and never touch `this`.
namespace ICE
{
    void ICERender::ScrShadowPrintf(s32 liX, s32 liY, f32 lfSize, const char* lpcFormat, ...)
    {
        gCalls.push_back({ liX, liY, lfSize, lpcFormat ? lpcFormat : "" });
    }
}

// Call Render with the editor's render context when it takes one, else with the integer
// the pre-fix declaration took (a sentinel no coordinate should ever equal).
static const s32 KI_SENTINEL = 7777;
template <class W>
static auto CallRender(W& lrWidget, ICE::ICERender* lpRender, int) -> decltype(lrWidget.Render(lpRender), void())
{
    lrWidget.Render(lpRender);
}
template <class W>
static void CallRender(W& lrWidget, ICE::ICERender*, long)
{
    lrWidget.Render(KI_SENTINEL);
}

static unsigned guChecks;
static unsigned guFailures;
static void Check(bool lbGood, const char* lpcWhy)
{
    ++guChecks;
    if (!lbGood)
    {
        ++guFailures;
        std::printf("FAIL %s\n", lpcWhy);
    }
}

static void SetF32(void* lpBase, unsigned luOffset, f32 lfValue)
{
    std::memcpy(static_cast<unsigned char*>(lpBase) + luOffset, &lfValue, sizeof(lfValue));
}

int main()
{
    ICE::ICERender lRender;

    // --- info list: two rows from (447, 393), stepping up 18 per row ---
    static ICE::ICEWidgetInfoList lList;
    std::memset(&lList, 0, sizeof(lList));
    std::strcpy(lList.macItemText[0], "ROW A");
    std::strcpy(lList.macItemText[1], "ROW B");
    lList.miItemCount = 2;
    SetF32(&lList, 0x25C, 447.0f);   // the X ConstructMenus seeds (32 + 415)
    SetF32(&lList, 0x260, 393.0f);   // the first line's Y (418 - 25)
    gCalls.clear();
    CallRender(lList, &lRender, 0);
    Check(gCalls.size() == 2, "info list draws one line per row");
    if (gCalls.size() == 2)
    {
        Check(gCalls[0].miX == 447 && gCalls[0].miY == 393, "info list row 0 at (447, 393)");
        Check(gCalls[1].miX == 447 && gCalls[1].miY == 375, "info list row 1 at (447, 375)");
        Check(gCalls[0].mFormat == "ROW A" && gCalls[1].mFormat == "ROW B", "info list rows are the formats");
        Check(gCalls[0].mfSize == 16.0f, "info list text size 16");
    }

    // --- logo: "ICE" at (mfX, mfY), size 64 ---
    ICE::ICEWidgetLogo lLogo;
    std::memset(&lLogo, 0, sizeof(lLogo));
    lLogo.mfX = 100.0f;
    lLogo.mfY = 200.0f;
    gCalls.clear();
    CallRender(lLogo, &lRender, 0);
    Check(gCalls.size() == 1, "logo draws one line");
    if (gCalls.size() == 1)
    {
        Check(gCalls[0].miX == 100 && gCalls[0].miY == 200, "logo text at (mfX, mfY)");
        Check(gCalls[0].mFormat == "ICE" && gCalls[0].mfSize == 64.0f, "logo draws \"ICE\" at size 64");
    }

    std::printf("%s ice widget text placement: %u checks, %u failures\n",
                guFailures ? "FAIL" : "PASS", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
