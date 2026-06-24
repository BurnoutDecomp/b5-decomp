// BrnCarouselSliderBar.cpp
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
//   Construct  @ 0x8241BB50 -- base GuiComponent::Construct, then mbVisible = true,
//                              mbAptUpdatestate = false.
//   Update     @ 0x8241BB90 -- compute the bar start/end as integer percentages of the
//                              item count and publish them as the "BarStart"/"BarEnd" apt
//                              view states; if the visibility was just changed, also publish
//                              "vis_state" + "apt_updatestate" and clear the dirty flag.
//   SetVisible @ 0x8241BD18 -- store mbVisible and raise mbAptUpdatestate.
//
// Update math (X360 floats):
//   start% = itemCount                    / (firstItem + 4) * 100  (flt_820049E0 = 100.0,
//   end%   = (itemCount + numVisibleItems) / (firstItem + 4) * 100   flt_82001C98 = 1.0,
//                                                                    flt_82F25648 = KF_NUM_VISIBLE_ITEMS)
// Each value is converted to int (fctiwz) and formatted with "%d" into an 8-byte buffer that
// is force-terminated at [7]. The percentages are published via the GuiComponent apt-view-
// state plumbing (the X360 reaches AddOutputAptViewState with immediate == 0 = false).

#include "GameSource/Gui/Flow/Screen/Components/BrnCarouselSliderBar.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                  // CgsCore::SPrintf

namespace BrnGui
{

// BrnCarouselSliderBar.cpp:23/:24 -- the apt-view-state ids the bar extents are published to
// (off_82F253D4 / off_82F253D8).
static const char* const mspacBarStartID = "BarStart";
static const char* const mspacBarEndID   = "BarEnd";

// BrnCarouselSliderBar.cpp:27 -- number of items visible at once, in float form, used in
// the end-extent calculation (X360 flt_82F25648). Matches KI_NUM_VISIBLE_ITEMS (= 5).
static const f32 KF_NUM_VISIBLE_ITEMS = 5.0f;

// Generic engine .rdata floats the X360 build shares across TUs.
static const f32 KF_PERCENT_SCALE = 100.0f;   // flt_820049E0
static const f32 KF_ONE           = 1.0f;     // flt_82001C98 (numerator of 1/(firstItem+4))

// @ 0x8241BB50
void CarouselSliderBar::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                  const char* lpacParentName)
{
    GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    mbVisible        = true;    // stb 1, 0x8C
    mbAptUpdatestate = false;   // stb 0, 0x8D
}

// @ 0x8241BB90
void CarouselSliderBar::Update(s32 liFirstItem, s32 liItemCount)
{
    // X360: var_50 = sext(firstItem + 4) is the divisor (the bar's leading edge in item space);
    //       var_40 = sext(itemCount) is the numerator. f29 = 1.0 / (firstItem + 4); f31 = (float)itemCount.
    const f32 lfEdge      = static_cast<f32>(liFirstItem + 4);
    const f32 lfInvEdge   = KF_ONE / lfEdge;                          // f29 = fdivs(1.0, firstItem + 4)
    const f32 lfItemCount = static_cast<f32>(liItemCount);            // f31 = (float)itemCount

    char lacBarStart[8];
    // f12 = itemCount * 100; BarStart = f12 * f29 = itemCount * 100 / (firstItem + 4)
    s32  liStartPercent = static_cast<s32>((lfItemCount * KF_PERCENT_SCALE) * lfInvEdge);
    CgsCore::SPrintf(lacBarStart, 8, "%d", liStartPercent);
    lacBarStart[7] = 0;

    char lacBarEnd[8];
    // f0 = 5.0 + itemCount; BarEnd = (f0 * f29) * 100 = (itemCount + 5) * 100 / (firstItem + 4)
    s32  liEndPercent = static_cast<s32>(((KF_NUM_VISIBLE_ITEMS + lfItemCount) * lfInvEdge) * KF_PERCENT_SCALE);
    CgsCore::SPrintf(lacBarEnd, 8, "%d", liEndPercent);
    lacBarEnd[7] = 0;

    AddOutputAptViewState(mspacBarStartID, lacBarStart, false);
    AddOutputAptViewState(mspacBarEndID, lacBarEnd, false);

    if (mbAptUpdatestate)
    {
        const char* lpacVisState = mbVisible ? "visible" : "invisible";
        AddOutputAptViewState("vis_state", lpacVisState, false);
        AddOutputAptViewState("apt_updatestate", "1", false);
        mbAptUpdatestate = false;   // stb 0, 0x8D
    }
}

// @ 0x8241BD18
void CarouselSliderBar::SetVisible(bool lbVisible)
{
    mbVisible        = lbVisible;   // stb r4, 0x8C
    mbAptUpdatestate = true;        // stb 1, 0x8D
}

} // namespace BrnGui
