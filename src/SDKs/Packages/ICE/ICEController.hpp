#ifndef SDKS_PACKAGES_ICE_ICECONTROLLER_HPP
#define SDKS_PACKAGES_ICE_ICECONTROLLER_HPP

#include "types.hpp"
#include <cstddef>                                          // offsetof
#include "SDKs/Packages/ICE/ICEAuthor.hpp"                    // ICE::ICETake (the embedded edit take)
#include "SDKs/Packages/ICE/ICEMemory.hpp"                  // ICE::ICEPointers (Construct arg), ICEMemory
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.hpp"    // ICE::ICEWidgetMenu (the menu widgets)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetColorPicker.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTimeBar.hpp" // ICE::ICEWidgetTimeBar (the two time bars)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetInfoList.hpp"// ICE::ICEWidgetInfoList (the read-out list)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTargetBox.hpp"// ICE::ICEWidgetTargetBox (the look-at target box)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetLogo.hpp"    // ICE::ICEWidgetLogo (the animated ICE logo)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetWideScrMarker.hpp" // ICE::ICEWidgetWideScrMarker (the letterbox marker)

// ARTIST's controller derives from ICEAuthor (Construct calls the base at
// 8253DA4C). Native prefix/contained-widget offsets are pinned below. The old
// target-box gap and trailing status/tail declarations remain unrecovered;
// no whole Controller or Manager extent is certified by these prefix gates.

namespace ICE
{
static const s32 KI_ICE_NUM_CHANGE_ELEMENTS = 37;
static const s32 KI_ICE_NUM_MENU_WIDGETS = 7;

struct ICEController : public ICEAuthor
{
    s32 mRenderContext;                         // native +F6C
    // Retain service names used by ICEManager while recovering their real types.
    ICETimer* mpMenuB;                           // native +F70, frame timer
    ActionQueue* mpMenuA;                        // native +F74, input queue
    ICEMemory* mpMenuC;                          // native +F78, edit allocator
    const IResourceManager* mpSubTakeSource;     // native +F7C

    ICEWidgetMenu* mpMenuMain;                   // native +F80
    ICEWidgetMenu* mpMenuIntervalMain;
    ICEWidgetMenu* mpMenuCopy;
    ICEWidgetMenu* mpMenuCommit;
    ICEWidgetMenu* mpMenuAssembly;
    ICEWidgetMenu* mpMenuCancel;
    ICEWidgetMenu* mpMenuIcon;

    ICEWidgetTimeBar mTimeBar;                   // native +FA0
    ICEWidgetTimeBar mTimeBar2;                  // native +3F40
    ICEWidgetInfoList mInfoList;                 // native +6EE0
    ICEWidgetColorPicker mColorPicker;           // native +7144
    ICEWidgetTargetBox mTargetBox;               // native +7170

    // Retained pre-existing debt: this span is not recovered as a native type.
    u8 maTargetBoxPad[0x20];                     // native +71C0..71DF
    ICEWidgetWideScrMarker mWideScrMarker;       // native +71E0
    ICEWidgetLogo mLogo;                         // native +71E4

    // Incomplete tail: fields below are in native order, but their absolute
    // offsets are still early. The four geometry floats at71F0..71FC, the
    // status-string capacity and7390..73B7 need further native evidence.
    // No filler is introduced to force this tail or the overall extent to fit.
    s32 miStatusCount;                           // native +7204
    s32 miStatusMode;                            // native +7208
    char miStatusText;                           // native +720C first character only
    s32 miEditFlag724C;
    u8 miShowUi;
    u8 miFlagOrient;
    u8 miFlagAux;
    u8 miFlagReset;
    f32 mfCursorParam;
    f32 mfCursorRatio;
    f32 mfBarFade;
    s16 mi16BubbleYaw;
    s16 mi16BubblePitch;
    f32 mfBubbleRadius;
    f32 maChangeBuffer[KI_ICE_NUM_CHANGE_ELEMENTS]; // native +7268, joystick values
    f32 maRampAccum[KI_ICE_NUM_CHANGE_ELEMENTS];    // native +72FC, acceleration values
    s32 miMenuTimer;                            // native +73B8
    u8 miBarFlipLatch;                          // native +73BC (stb/lbz)
    u8 miBarFlipSub;                            // native +73BD
    f32 mfMarkIn;                              // native +73C0
    f32 mfMarkOut;                             // native +73C4

    f32& JoyVelocity(s32 liAxis) { return maChangeBuffer[liAxis]; }
    f32 JoyVelocity(s32 liAxis) const { return maChangeBuffer[liAxis]; }

    void Construct(ICEPointers* lpICEPointers);
    bool IsMarked() const;
    bool IsShakeEditing() const;
    void SetZoomRange();
    void SetCurrentIntervalValue(s32 liElement, s32 liValue);
    void SetJoyVelocities(s32 liAxis, f32 lfMagnitude);
    void MoveCursor();
    void BarFlipping(bool lbForward);
    void RefreshMenu(s32 liMenuKind);
    void RefreshInfoList();
    void RefreshIntervalMenu();
    void ConstructMenus();
    void DestructMenus();
    void Update();
    void JoyHandler();
    void Render();
    void RenderBars();
    void RenderMenus();
    void RenderTargetBox();
    void ChangeKeyElementValue(s32 liChangeIndex);
    void ChangeKeyElementValues();
    void ToBubble();
    void FromBubble();
    ICETake* GetEditTake() { return &mEditTake; }
    bool AreMenusActive() const { return miState > 0; }
};

// Actual containment anchors. These do not certify the unresolved tail.
CGS_STATIC_ASSERT((sizeof(ICEAuthor) == 0xF6C), "native author extent");
CGS_STATIC_ASSERT((offsetof(ICEController, mEditTake) == 0x28), "native controller edit take");
CGS_STATIC_ASSERT((offsetof(ICEController, mScratchTake) == 0x760), "native controller scratch take");
CGS_STATIC_ASSERT((offsetof(ICEController, miState) == 0xE98), "native controller author state");
CGS_STATIC_ASSERT((offsetof(ICEController, mRenderContext) == 0xF6C), "native controller render context");
CGS_STATIC_ASSERT((offsetof(ICEController, mpMenuB) == 0xF70), "native controller timer");
CGS_STATIC_ASSERT((offsetof(ICEController, mpMenuA) == 0xF74), "native controller action queue");
CGS_STATIC_ASSERT((offsetof(ICEController, mpMenuC) == 0xF78), "native controller memory");
CGS_STATIC_ASSERT((offsetof(ICEController, mpSubTakeSource) == 0xF7C), "native controller resources");
CGS_STATIC_ASSERT((offsetof(ICEController, mTimeBar) == 0xFA0), "native controller primary timebar");
CGS_STATIC_ASSERT((offsetof(ICEController, mTimeBar2) == 0x3F40), "native controller secondary timebar");
CGS_STATIC_ASSERT((offsetof(ICEController, mInfoList) == 0x6EE0), "native controller info list");
CGS_STATIC_ASSERT((offsetof(ICEController, mColorPicker) == 0x7144), "native controller color picker");
CGS_STATIC_ASSERT((offsetof(ICEController, mTargetBox) == 0x7170), "native controller target box");
CGS_STATIC_ASSERT((offsetof(ICEController, mWideScrMarker) - offsetof(ICEController, mTargetBox) == 0x70),
                  "native target-to-wide-screen widget delta");
CGS_STATIC_ASSERT((offsetof(ICEController, mLogo) - offsetof(ICEController, mTargetBox) == 0x74),
                  "native target-to-logo widget delta");
CGS_STATIC_ASSERT((offsetof(ICEController, maRampAccum) == offsetof(ICEController, maChangeBuffer) +
                  sizeof(((ICEController*)0)->maChangeBuffer)), "distinct adjacent native velocity banks");
CGS_STATIC_ASSERT((sizeof(((ICEController*)0)->miShowUi) == 1), "native UI flag width");
CGS_STATIC_ASSERT((sizeof(((ICEController*)0)->miBarFlipLatch) == 1), "native bar flip latch width");
}

#endif
