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
// 8253DA4C). Native offsets and the object-representation extent are pinned below.
// The three unobserved tail windows preserve bytes bounded by independent native
// accesses; their original declarations remain unknown. They add no initialization
// or editor behavior and do not establish a status-string capacity.

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

    // ConstructMenus writes the rectangle at native +0x71F0..+0x71FC.
    // Its owning widget/name is not yet recovered; these are four real float lanes.
    f32 mfHudRectX;
    f32 mfHudRectY;
    f32 mfHudRectWidth;
    f32 mfHudRectHeight;

    // ARTIST 825321AC ends the HUD rectangle at 7200; 8253DA84 stores the next
    // named scalar at 7204. Only the full game zero-init touches these bytes.
    u8 maUnobservedHudStatusStorage[0x04];       // native +7200..7203
    s32 miStatusCount;                           // native +7204
    s32 miStatusMode;                            // native +7208
    char miStatusText;                           // native +720C first format character
    // 8253DA8C clears only the first character; 8253DA90 stores the separate
    // scalar at 724C. This preserves the intervening representation, without
    // assigning it a string capacity or clearing it during Construct.
    u8 maUnobservedStatusStorage[0x3F];           // native +720D..724B
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
    // Native description axes end at 36: the last active ramp ends at 7390.
    // 8253DADC separately stores the timer at 73B8. No extra active entries.
    u8 maUnobservedPostAccelerationStorage[0x28]; // native +7390..73B7
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

// Native member accesses and independently bounded object-representation windows.
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
CGS_STATIC_ASSERT((offsetof(ICEController, mfHudRectX) == 0x71F0), "native HUD rectangle X");
CGS_STATIC_ASSERT((offsetof(ICEController, mfHudRectHeight) == 0x71FC), "native HUD rectangle height");
CGS_STATIC_ASSERT((offsetof(ICEController, maUnobservedHudStatusStorage) == 0x7200), "native controller maUnobservedHudStatusStorage");
CGS_STATIC_ASSERT((offsetof(ICEController, miStatusCount) == 0x7204), "native controller miStatusCount");
CGS_STATIC_ASSERT((offsetof(ICEController, miStatusMode) == 0x7208), "native controller miStatusMode");
CGS_STATIC_ASSERT((offsetof(ICEController, miStatusText) == 0x720C), "native controller miStatusText");
CGS_STATIC_ASSERT((offsetof(ICEController, maUnobservedStatusStorage) == 0x720D), "native controller maUnobservedStatusStorage");
CGS_STATIC_ASSERT((offsetof(ICEController, miEditFlag724C) == 0x724C), "native controller miEditFlag724C");
CGS_STATIC_ASSERT((offsetof(ICEController, miShowUi) == 0x7250), "native controller miShowUi");
CGS_STATIC_ASSERT((offsetof(ICEController, mfCursorParam) == 0x7254), "native controller mfCursorParam");
CGS_STATIC_ASSERT((offsetof(ICEController, mfCursorRatio) == 0x7258), "native controller mfCursorRatio");
CGS_STATIC_ASSERT((offsetof(ICEController, mfBarFade) == 0x725C), "native controller mfBarFade");
CGS_STATIC_ASSERT((offsetof(ICEController, mi16BubbleYaw) == 0x7260), "native controller mi16BubbleYaw");
CGS_STATIC_ASSERT((offsetof(ICEController, mi16BubblePitch) == 0x7262), "native controller mi16BubblePitch");
CGS_STATIC_ASSERT((offsetof(ICEController, mfBubbleRadius) == 0x7264), "native controller mfBubbleRadius");
CGS_STATIC_ASSERT((offsetof(ICEController, maChangeBuffer) == 0x7268), "native controller maChangeBuffer");
CGS_STATIC_ASSERT((offsetof(ICEController, maRampAccum) == 0x72FC), "native controller maRampAccum");
CGS_STATIC_ASSERT((offsetof(ICEController, maUnobservedPostAccelerationStorage) == 0x7390), "native controller maUnobservedPostAccelerationStorage");
CGS_STATIC_ASSERT((offsetof(ICEController, miMenuTimer) == 0x73B8), "native controller miMenuTimer");
CGS_STATIC_ASSERT((offsetof(ICEController, miBarFlipLatch) == 0x73BC), "native controller miBarFlipLatch");
CGS_STATIC_ASSERT((offsetof(ICEController, miBarFlipSub) == 0x73BD), "native controller miBarFlipSub");
CGS_STATIC_ASSERT((offsetof(ICEController, mfMarkIn) == 0x73C0), "native controller mfMarkIn");
CGS_STATIC_ASSERT((offsetof(ICEController, mfMarkOut) == 0x73C4), "native controller mfMarkOut");
CGS_STATIC_ASSERT((sizeof(ICEController) == 0x73D0), "native controller representation extent");
CGS_STATIC_ASSERT((offsetof(ICEController, maRampAccum) == offsetof(ICEController, maChangeBuffer) +
                  sizeof(((ICEController*)0)->maChangeBuffer)), "distinct adjacent native velocity banks");
CGS_STATIC_ASSERT((sizeof(((ICEController*)0)->miShowUi) == 1), "native UI flag width");
CGS_STATIC_ASSERT((sizeof(((ICEController*)0)->miBarFlipLatch) == 1), "native bar flip latch width");
}

#endif
