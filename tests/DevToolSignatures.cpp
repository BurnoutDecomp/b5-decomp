// The parameter lists ARTIST attests for six dev-tool functions: five in the ICE camera
// editor and the light-setup tool blend of the environment manager. Each list was read off
// the X360 callee (which argument registers it reads before writing) and its call sites
// (which ones they load). On this ABI an f32 argument also occupies the next integer
// argument slot, so a register-counting decompiler invents an int for every float; the
// asserts below pin the lists the console actually passes. Compile-only: a failing
// static_assert is the RED verdict.
#include <cstdarg>
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
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetInfoList.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetLogo.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.hpp"
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

// Every widget Render takes the editor's render context: the controller passes
// controller + 0xF6C in r4 (RenderBars 0x82539EAC, RenderMenus 0x82539F6C, Render via r30
// = this + 0xF6C at 0x8253B1DC), and each body forwards it in r3 to its ICERender calls.
static_assert(std::is_same<decltype(&ICE::ICEWidgetInfoList::Render),
                           void (ICE::ICEWidgetInfoList::*)(ICE::ICERender*)>::value,
              "ICEWidgetInfoList::Render takes the editor's ICERender");
static_assert(std::is_same<decltype(&ICE::ICEWidgetLogo::Render),
                           void (ICE::ICEWidgetLogo::*)(ICE::ICERender*)>::value,
              "ICEWidgetLogo::Render takes the editor's ICERender");
static_assert(std::is_same<decltype(&ICE::ICEWidgetMenu::Render),
                           void (ICE::ICEWidgetMenu::*)(ICE::ICERender*)>::value,
              "ICEWidgetMenu::Render takes the editor's ICERender");
static_assert(std::is_same<decltype(&ICE::ICEWidgetTimeBar::Render),
                           void (ICE::ICEWidgetTimeBar::*)(ICE::ICERender*)>::value,
              "ICEWidgetTimeBar::Render takes the editor's ICERender");

// The ICERender draws are member calls: every console call site loads the render context
// into r3 (e.g. InfoList 0x825346B4, Menu 0x82534B3C, RenderBars 0x82538EE4), and the
// callees never read it. Their own parameters start at r4.
static_assert(std::is_same<decltype(&ICE::ICERender::ScrShadowPrintf),
                           void (ICE::ICERender::*)(s32, s32, f32, const char*, ...)>::value,
              "ICERender::ScrShadowPrintf is a member (x, y, size, format, ...)");
static_assert(std::is_same<decltype(&ICE::ICERender::ScrPrintf),
                           void (ICE::ICERender::*)(s32, s32, f32, const char*, ...)>::value,
              "ICERender::ScrPrintf is a member (x, y, size, format, ...)");
static_assert(std::is_same<decltype(&ICE::ICERender::ScrPrintfArg),
                           void (ICE::ICERender::*)(s32, s32, f32, const ICE::Vector4*, const char*,
                                                    va_list&)>::value,
              "ICERender::ScrPrintfArg is a member (x, y, size, colour, format, args)");
// RenderPoly: r4 = the poly and r5-r8 four colour pointers, the same one at every call site
// (e.g. Menu 0x82534B04..0x82534B30); the callee reads r4 and r5 only.
static_assert(std::is_same<decltype(&ICE::ICERender::RenderPoly),
                           void (ICE::ICERender::*)(const ICE::Poly*, const ICE::Vector4*,
                                                    const ICE::Vector4*, const ICE::Vector4*,
                                                    const ICE::Vector4*)>::value,
              "ICERender::RenderPoly is a member (poly, four colours)");

int main()
{
    std::printf("PASS dev-tool signatures: 14 parameter lists match ARTIST\n");
    return 0;
}
