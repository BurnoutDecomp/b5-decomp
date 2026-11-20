// The parameter lists ARTIST attests for six dev-tool functions: five in the ICE camera
// editor and the light-setup tool blend of the environment manager. Each list was read off
// the X360 callee (which argument registers it reads before writing) and its call sites
// (which ones they load). On this ABI an f32 argument also occupies the next integer
// argument slot, so a register-counting decompiler invents an int for every float; the
// asserts below pin the lists the console actually passes. Compile-only: a failing
// static_assert is the RED verdict.
#include <cstdio>
#include <type_traits>

#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/EnvironmentManager/BrnEnvironmentManager.h"
#undef protected
#undef private
#include "SDKs/Packages/ICE/ICEAuthor.hpp"
#include "SDKs/Packages/ICE/ICERender.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTargetBox.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTimeBar.hpp"

using BrnWorld::EnvironmentSettings::EnvironmentManager;

// 0x8252DE28 reads r4 and f1 (r5 is first written at 0x8252DF28); every call site in
// ChangeKeyElementValue and Update loads r4 and f1 only.
static_assert(std::is_same<decltype(&ICE::ICEAuthor::SetKeyElementValueChange),
                           void (ICE::ICEAuthor::*)(s32, f32)>::value,
              "ICEAuthor::SetKeyElementValueChange takes (element, delta)");

// Every call site loads r3/r4 only (EditorOn 0x8253F41C, EditAssembly 0x8253F4A4, JoyHandler
// 0x8253EA48, ICEWrapper::EditorOff 0x82540418); 0x8253CD78 first writes f1 at 0x8253CDA8.
static_assert(std::is_same<decltype(&ICE::ICEAuthor::SetState),
                           void (ICE::ICEAuthor::*)(s32)>::value,
              "ICEAuthor::SetState takes the new state only");

// 0x82531078 reads f1-f3 and stores r7 as a byte (0x82531088); r4-r6 are written first.
// ConstructMenus loads f1-f3 and r7 (0 at 0x825320C8, 1 at 0x825320EC), never r4-r6.
static_assert(std::is_same<decltype(&ICE::ICEWidgetTimeBar::Construct),
                           void (ICE::ICEWidgetTimeBar::*)(f32, f32, f32, bool)>::value,
              "ICEWidgetTimeBar::Construct takes (x, y, start, compact)");

// Both RenderBars call sites load r6 = 30 (0x825396D0, 0x82539BC4) after f1/f2.
static_assert(std::is_same<decltype(&ICE::ICEWidgetTimeBar::SetTimeInfo),
                           void (ICE::ICEWidgetTimeBar::*)(f32, f32, s32)>::value,
              "ICEWidgetTimeBar::SetTimeInfo takes (time, total, frame rate)");

// RenderTargetBox loads r4 = controller + 0xF6C (0x8253A154) before the call (0x8253A194).
static_assert(std::is_same<decltype(&ICE::ICEWidgetTargetBox::Render),
                           void (ICE::ICEWidgetTargetBox::*)(ICE::ICERender*)>::value,
              "ICEWidgetTargetBox::Render takes the editor's ICERender");

// SetupBlend keeps its output buffer (r6) in r20 (0x827D5024) and passes it as r5 to the tool
// blend (0x827D527C), exactly as it does to SetupSeasonsBlend (0x827D51E8).
static_assert(std::is_same<decltype(&EnvironmentManager::SetupUpdateFromToolBlend),
                           bool (EnvironmentManager::*)(EnvironmentManager::BlendFrame&,
                                                        BrnWorldIO::UpdateOutputBuffer*)>::value,
              "EnvironmentManager::SetupUpdateFromToolBlend takes (blend frame, output buffer)");

int main()
{
    std::printf("PASS dev-tool signatures: 6 parameter lists match ARTIST\n");
    return 0;
}
