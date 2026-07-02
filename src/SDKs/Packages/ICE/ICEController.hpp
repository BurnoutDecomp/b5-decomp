#ifndef SDKS_PACKAGES_ICE_ICECONTROLLER_HPP
#define SDKS_PACKAGES_ICE_ICECONTROLLER_HPP

#include "types.hpp"
#include <cstddef>                                          // offsetof
#include "SDKs/Packages/ICE/ICEData.hpp"                    // ICE::ICETake (the embedded edit take)
#include "SDKs/Packages/ICE/ICEMemory.hpp"                  // ICE::ICEPointers (Construct arg), ICEMemory
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.hpp"    // ICE::ICEWidgetMenu (the menu widgets)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTimeBar.hpp" // ICE::ICEWidgetTimeBar (the two time bars)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetInfoList.hpp"// ICE::ICEWidgetInfoList (the read-out list)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTargetBox.hpp"// ICE::ICEWidgetTargetBox (the look-at target box)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetLogo.hpp"    // ICE::ICEWidgetLogo (the animated ICE logo)
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetWideScrMarker.hpp" // ICE::ICEWidgetWideScrMarker (the letterbox marker)

// ============================================================================
// SDKs/Packages/ICE/ICEController.hpp
//
// ICE::ICEController -- the in-game ICE camera-take EDITOR/driver embedded by value
// at the tail of ICE::ICEManager (the controller base sits inside the manager). It
// IS-AN ICEAuthor (Construct chains ICEAuthor::Construct first): the ICEAuthor base
// owns the live edit take, the runtime channel data, the camera-space handler and
// the take-authoring operations; the ICEController derived part adds the on-screen
// editor UI (the menu / time-bar / info-list / logo / target-box widgets), the
// joystick-velocity + cursor + bar-flip state, the per-channel value-change buffer,
// and the editor sub-state machine the manager polls each frame.
//
// LAYOUT MODEL (and the ICEAuthor caveat)
// ---------------------------------------
// ICEAuthor.hpp does NOT exist yet (another agent is reconstructing it in parallel),
// so this header CANNOT yet derive from a real ICEAuthor base. To keep the attested
// member OFFSETS exact for the bodies that run here, the whole layout is modelled as
// ONE flat struct: the leading region [+0x00 .. before the first ICEController-owned
// member] stands in for the not-yet-reconstructed ICEAuthor base, with only the
// members the bodies here actually touch named (the rest is reserved bytes). The
// ICEController-owned editor members follow at their attested offsets.
//
// FLAG (ICEAuthor base): when ICEAuthor.hpp lands its real layout, this struct must
// become `struct ICEController : public ICEAuthor` and the leading author-region
// members modelled here (mEditTake, miState, miStateArg, miCurrentChannel, the three
// subsystem pointers, mpRenderContext, the zoom-cursor floats, the bubble angles,
// ...) must MOVE onto the ICEAuthor base -- do NOT keep a fork. Every author-region
// member below is marked `// [author-base]`.
//
// FLAG (ICEAuthor methods): the bodies here call ICEAuthor methods (SetState,
// IsEndKeyState, GetIntervalKey, SetKeyElementValue(Change), SetParameterChange,
// GetWorldTransformationMatrix, ...) on `this` (a controller is-an author). Those
// are declared on ICEAuthor, not here. The foundational bodies in ICEController.cpp
// only call the ones they need; where a foundational body would need an ICEAuthor
// method that is not yet declared anywhere, it is noted in the .cpp and the call is
// expressed against a forward-declared ICEAuthor base (FLAGged at the call site).
//
// OFFSET MODEL (parity-by-name, NOT byte-exact on the 64-bit host): the `+0xNN`
// offsets in the comments below are the source-spine facts (the only source of truth for
// member placement). They are NOT reproduced byte-exactly here because the embedded
// ICETake is larger on the 64-bit compile host (0x870 vs the source-build 0x738) and the two
// embedded ICEWidgetTimeBars likewise widen, so absolute offsets past mEditTake
// drift -- exactly the pointer-widening caveat ICEManager.hpp/ICEData.hpp record. The
// members are therefore declared in source-spine ORDER and accessed BY NAME (the bodies never
// cast through an offset). Only mEditTake's +0x28 placement -- which the manager and
// the ICETake-op call sites depend on -- is pinned with static_assert; the leading
// author-head reserved block sizes it there. The slice other TUs already call
// (mEditTake/Render/EditorOn/SetState/GetEditTake/AreMenusActive + the three
// manager-nulled pointers) is grown in place -- not forked.
//
// FLAG (offset drift): the source-spine `+0xNN` offsets are documentation; members after
// mEditTake do NOT land at those absolute byte offsets on the host (the host ICETake
// is bigger). Parity is by named-member access, per the project rule.
// ============================================================================

namespace ICE
{

// ICEAuthor -- the take-authoring base the controller derives from. Forward-declared
// only (its home ICEAuthor.hpp is owned by another agent). The controller's bodies
// reach author methods through `this` once the real base lands; until then the flat
// layout below stands in for the base storage. FLAG: forward decl only.
class ICEAuthor;

// The per-channel value-change accumulator the JoyHandler/Update path zeroes each
// pass: a 37-entry s32 buffer (one slot per element description). Construct and
// JoyHandler clear all 37 entries (`do { *p++ = 0; } while (--n);`, n == 37).
static const s32 KI_ICE_NUM_CHANGE_ELEMENTS = 37;

// The number of menu/time-bar widget slots DestructMenus walks when freeing (the
// loop runs 7 times over the +0xF80.. pointer block).
static const s32 KI_ICE_NUM_MENU_WIDGETS = 7;

struct ICEController
{
    // ====================================================================
    // [author-base] leading region (stands in for the ICEAuthor base until
    // ICEAuthor.hpp lands). Members are declared in source-spine ORDER; the comment after
    // each gives its source spine `+0xNN` offset (documentation -- see the OFFSET MODEL note
    // above: absolute offsets drift on the host because the embedded ICETake/widgets
    // are wider, so they are NOT reproduced byte-exactly; access is by name).
    // ====================================================================

    // @0x00  Author-base head preceding the edit take (vptr / resource-manager /
    // camera pointers the bodies read at +0x08 / +0x18 / +0x20). Reserved so the edit
    // take lands at the attested +0x28 (the one byte-exact anchor). [author-base]
    u8   mAuthorHead[0x28];

    // @0x28  The live "edit" camera take. ICEManager::GetCameraTake returns
    // &mController.mEditTake when not playing back a movie; the editor bodies pass
    // its address as `this + 40` to ICETake ops (Insert/Delete/GetValueInt/SetValue/
    // Harden/...). The ONE byte-exact anchor (static_assert below). [author-base]
    ICETake mEditTake;                                       // @0x28

    // @0xE98  Editor sub-state / mode. >0 means the editor owns the camera (the
    // manager skips playback while active). The bodies switch on it (values 1..18 ==
    // E_ICE_EDITOR_STATE) and read it as `*(this+3736)`. [author-base]
    s32  miState;                                            // @0xE98
    // @0xE9C  State argument (the key/element index passed to ICEAuthor::IsEndKeyState
    // when miState == 16). [author-base]
    s32  miStateArg;                                         // @0xE9C
    // @0xEA0  Current channel/track index (0..11); the channel the editor edits.
    // Read as `*(this+3744)`. [author-base]
    s32  miCurrentChannel;                                   // @0xEA0

    // @0xEC0/+0xEC4  Channel-0 bubble cursor read-outs (RenderBars channel-0 path).
    // [author-base]
    f32  mfBubbleCursorA;                                    // @0xEC0
    f32  mfBubbleCursorB;                                    // @0xEC4

    // @0xF1C..+0xF28  The zoom-range cursor pairs SetZoomRange / RenderBars / MoveCursor
    // drive: the non-shake pair (+0xF1C/+0xF20) and the shake pair (+0xF24/+0xF28).
    // MoveCursor / RenderBars pick the pair by miState (shake when miState == 8).
    // [author-base]
    f32  mfZoomCursorLo;                                     // @0xF1C  non-shake low edge
    f32  mfZoomCursorHi;                                     // @0xF20  non-shake high edge
    f32  mfZoomCursorShakeLo;                                // @0xF24  shake low edge
    f32  mfZoomCursorShakeHi;                                // @0xF28  shake high edge

    // @0xF50  Per-element checkbox/toggle base (the copy-elements menu flips a bool at
    // this+0xF50 + elementByteOffset). Modelled as the leading word of that region; the
    // full per-element toggle span lives in the author-base value table. [author-base]
    s32  miElementToggleBase;                                // @0xF50

    // @0xF6C  Editor render context: passed by address (`this + 3948`) to the ICERender
    // / widget Render entry points as the draw context. [author-base]
    s32  mRenderContext;                                     // @0xF6C

    // @0xF70  Frame-timestep source: a pointer-to-float dereferenced ONCE to read the
    // current per-frame timestep the velocity/parameter maths scale by. The member at
    // +0xF70 holds the pointer; the read loads the word at +0xF70 then reads the float
    // it points at (a single indirection). FIRST of the three subsystem pointers
    // ICEManager::Destruct nulls
    // (+0xF70 then +0xF74 then +0xF78). Kept under the name ICEManager.cpp already uses
    // (mpMenuB) so that consumer is not broken; its real role is the timestep source.
    // FLAG: name retained for the existing ICEManager consumer; role is the timestep
    // pointer, not a menu. [author-base]
    f32* mpMenuB;                                            // @0xF70  (timestep source)

    // @0xF74  The editor action queue (ICE::ActionRef<20>): JoyHandler Peek()s and
    // Pop()s input actions from it. Second of the three manager-nulled pointers; kept
    // under the existing consumer name (mpMenuA). FLAG: name retained for the existing
    // ICEManager consumer; role is the action queue. [author-base]
    void* mpMenuA;                                           // @0xF74  (action queue)

    // @0xF78  The ICE memory manager (ConstructMenus allocates each widget from it via
    // GetMemory; DestructMenus frees through its embedded edit heap). Third of the
    // three manager-nulled pointers; kept under the existing consumer name (mpMenuC).
    // FLAG: name retained for the existing ICEManager consumer; role is the memory
    // manager. [author-base]
    ICEMemory* mpMenuC;                                      // @0xF78  (ICE memory manager)

    // @0xF7C  A fourth owned pointer Construct seeds from the ICEPointers bundle
    // (a2[5]); read in RenderBars via a virtual call (the current sub-take source).
    // [author-base]
    void* mpSubTakeSource;                                   // @0xF7C

    // ====================================================================
    // ICEController-owned editor members. The seven menu widget pointers
    // DestructMenus frees occupy +0xF80..+0xF98 (KI_ICE_NUM_MENU_WIDGETS slots).
    // ====================================================================

    // @0xF80..+0xF98  The seven editor menu widget pointers (each a heap ICEWidgetMenu*,
    // allocated by ConstructMenus from the ICE memory manager, freed by DestructMenus).
    // RefreshMenu / RenderMenus / JoyHandler select among them by editor state. FLAG:
    // per-slot roles are inferred from the Construct sizes + the menu the Refresh/Render
    // switch selects.
    ICEWidgetMenu* mpMenuMain;                               // @0xF80  main editor menu
    ICEWidgetMenu* mpMenuIntervalMain;                       // @0xF84  interval main menu
    ICEWidgetMenu* mpMenuCopy;                               // @0xF88  copy-elements menu
    ICEWidgetMenu* mpMenuCommit;                             // @0xF8C  commit/discard menu
    ICEWidgetMenu* mpMenuAssembly;                           // @0xF90  assembly-line menu
    ICEWidgetMenu* mpMenuCancel;                             // @0xF94  cancel/delete menu
    ICEWidgetMenu* mpMenuIcon;                               // @0xF98  icon menu (32x32)

    // @0xFA0  The main embedded time bar (this+4000; ICEWidgetTimeBar). RenderBars
    // Reset()s it and fills its markers; ConstructMenus positions it.
    ICEWidgetTimeBar mTimeBar;                               // @0xFA0

    // @0x3F40  The second embedded time bar (this+16192; ICEWidgetTimeBar). The
    // assembly/secondary bar RenderBars draws below the main bar.
    ICEWidgetTimeBar mTimeBar2;                              // @0x3F40

    // @0x6E20  The embedded info-list widget (this+28192; RenderBars fills it via
    // RefreshInfoList and draws it). Its marker/line count lives at +0x6E20.
    ICEWidgetInfoList mInfoList;                             // @0x6E20

    // @0x7170  The look-at target box widget. RenderTargetBox passes `this+0x7170` to
    // ICEWidgetTargetBox::Render and fills its transform rows at +0x7180 (= widget+0x10,
    // i.e. mTargetBox.mTransform). The widget is 0x50 bytes (a 0x10 leading region + a
    // 0x40 four-row transform), so it ends at +0x71C0; the 0x20-byte gap to the
    // wide-screen marker is reserved.
    ICEWidgetTargetBox mTargetBox;                           // @0x7170
    u8   maTargetBoxPad[0x20];                               // @0x71C0  reserved gap to the next widget

    // @0x71E0  The wide-screen / letterbox marker widget. Render draws it via
    // ICEWidgetWideScrMarker::Render(this+0x71E0) -- a SEPARATE widget from the target
    // box (it is NOT folded into mTargetBox's bytes). It has no data members (empty
    // struct, see ICEWidgetWideScrMarker.hpp), so it occupies one byte here; the three
    // bytes before mLogo are natural alignment pad.
    ICEWidgetWideScrMarker mWideScrMarker;                   // @0x71E0

    // @0x71E4  The animated "ICE" logo widget. Render draws it via
    // ICEWidgetLogo::Render(this+0x71E4) -- also a SEPARATE widget. ConstructMenus seeds
    // its screen-position floats (the +0x71E4/+0x71E8 lanes).
    ICEWidgetLogo mLogo;                                     // @0x71E4

    // ====================================================================
    // Status-text + edit-mode flags + cursor + bubble + value-change state.
    // (The source spine places these at +0x7204..+0x73C4; here they follow the widgets in
    // source-spine order and are accessed by name -- absolute offsets drift, see the note.)
    // ====================================================================

    // @0x7204/+0x7208/+0x720C  Status-text state: a count (+0x7204), a mode (+0x7208 ==
    // 2 selects the status print), and the text buffer base (+0x720C). Construct zeroes
    // the first two and the buffer. FLAG: roles inferred from the Render path.
    s32  miStatusCount;                                      // @0x7204
    s32  miStatusMode;                                       // @0x7208
    s32  miStatusText;                                       // @0x720C

    // @0x724C  An edit-flag Construct zeroes. FLAG: role unattested beyond the zero.
    s32  miEditFlag724C;                                     // @0x724C

    // @0x7250  "Show editor UI" toggle (Construct sets it to 1). Toggled by the input
    // handler; RenderBars/Render gate the overlay on it.
    s32  miShowUi;                                           // @0x7250
    // @0x7251/+0x7252/+0x7253  Three edit-mode bool flags (Construct zeroes them):
    //   miFlagOrient @0x7251  (the "edit orientation" toggle, state 14/15)
    //   miFlagAux    @0x7252  (a second edit toggle, state 14/15 + target box gate)
    //   miFlagReset  @0x7253  (a one-shot "reset bubble" request)
    u8   miFlagOrient;                                       // @0x7251
    u8   miFlagAux;                                          // @0x7252
    u8   miFlagReset;                                        // @0x7253

    // @0x7254  Cursor parameter origin (MoveCursor writes the take parameter here).
    f32  mfCursorParam;                                      // @0x7254
    // @0x7258  Cursor velocity/ratio (MoveCursor writes the shuttle ratio here).
    f32  mfCursorRatio;                                      // @0x7258
    // @0x725C  Status-bar fade/alpha (ConstructMenus tail seeds 3.0; Update decays it).
    f32  mfBarFade;                                          // @0x725C

    // @0x7260/+0x7262  Bubble look angles (two s16 the manager ctor writes via `sth`;
    // ToBubble/FromBubble read/write them as the yaw/pitch the bubble cursor encodes).
    // The manager note ("two sth to controller+0x7260/+0x7262") pins this pair's role.
    s16  mi16BubbleYaw;                                      // @0x7260
    s16  mi16BubblePitch;                                    // @0x7262
    // @0x7264  Bubble radius (ToBubble writes it; Construct seeds 0.0).
    f32  mfBubbleRadius;                                     // @0x7264

    // @0x7268  The 37-entry per-channel value-change / joystick-velocity buffer (this+
    // 29288). Construct + JoyHandler zero all 37 entries (`do{*p++=0;}while(--n)`, n==
    // 37). It is read two ways: as s32 change accumulators (ChangeKeyElementValue) and
    // as f32 joystick velocities -- SetJoyVelocities writes `*(4*(axis+7322)+this)`,
    // i.e. this+0x7268 + axis*4 == maChangeBuffer[axis] reinterpreted as f32 (7322*4 ==
    // 29288 == 0x7268). So the joystick velocities and the cursor/zoom velocity reads
    // the editing maths difference (the `this+29404/29408/29412/29416/29420/29424/29428/
    // 29432` floats) ALL live inside this one array (indices 29..36). They are accessed
    // through the typed helpers below rather than as separate overlapping members.
    // FLAG: this single array is the joystick-velocity AND the value-change region; the
    // f32/s32 dual view is modelled by helpers, not by aliased members.
    s32  maChangeBuffer[KI_ICE_NUM_CHANGE_ELEMENTS];         // @0x7268

    // @0x73B8  Menu-auto-advance countdown (Update decrements it; state 1 reseeds 15).
    s32  miMenuTimer;                                        // @0x73B8
    // @0x73BC  Bar-flip latch (BarFlipping sets it when a flip starts, clears it when
    // both axes recentre).
    s32  miBarFlipLatch;                                     // @0x73BC
    // @0x73BD  A one-byte sub-latch BarFlipping shares (Construct zeroes it).
    u8   miBarFlipSub;                                       // @0x73BD

    // @0x73C0/+0x73C4  The mark in/out parameters (IsMarked tests both; the segment ops
    // read them as the marked-range endpoints). Construct seeds both to -1.0.
    f32  mfMarkIn;                                           // @0x73C0
    f32  mfMarkOut;                                          // @0x73C4

    // ---- typed views over the joystick-velocity / value-change buffer ----
    // The change buffer is the f32 joystick-velocity region (see maChangeBuffer above);
    // SetJoyVelocities and the editing maths read/write it as floats at index axis.
    f32&       JoyVelocity(s32 liAxis)       { return reinterpret_cast<f32*>(maChangeBuffer)[liAxis]; }
    f32        JoyVelocity(s32 liAxis) const { return reinterpret_cast<const f32*>(maChangeBuffer)[liAxis]; }

    // ====================================================================
    // Methods. ALL 23 declared with verified signatures. The foundational
    // group gets bodies in ICEController.cpp; the rest are declaration-only
    // here (their bodies land with the fan-out TUs; the per-TU cl /c gate
    // does not link, so declarations suffice).
    // ====================================================================

    // ---- Foundational group (bodies in ICEController.cpp) ----
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

    // ---- Fan-out group (declaration-only here; bodies in their own TUs) ----
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

    // ---- Editor entry points driven by BrnDirector::ICEWrapper (EditorOn/EditorOff).
    // These are ICEAuthor base methods (ICEAuthor::EditorOn, ICEAuthor::SetState),
    // invoked on the editor `this`. Declared here on the slice until the ICEAuthor base
    // lands (their `this` is identical). FLAG: move onto the ICEAuthor base when it
    // lands -- do NOT keep a fork. (EditorOn RETYPED 2026-07 to match the real
    // ICEAuthor::EditorOn(ICETakeData*) -- ICEAuthor.hpp:252; the old `s32 liMode`
    // parameter was a mis-model.)
    void EditorOn(ICETakeData* lpTakeData);
    void SetState(s32 liState);

    // ---- Accessors preserved for the existing consumers (ICEManager / ICEWrapper) ----
    // ICEManager::GetCameraTake returns the embedded edit take.
    ICETake* GetEditTake() { return &mEditTake; }
    // Editor-active poll used by ICEManager::Update and BrnDirector::ICEWrapper.
    bool AreMenusActive() const { return miState > 0; }
};

// Pin the offsets the foundational bodies read/write. The author-region offsets that
// depend on sizeof(ICETake) (anything >= miState) are NOT static_asserted here
// (sizeof(ICETake) differs on the 64-bit host; the source-spine spine is the offset source --
// the bodies access by named member, not by cast). Only the leading author-head edit
// take and the offsets independent of pointer widening are asserted.
static_assert(offsetof(ICEController, mEditTake) == 0x28, "ICEController::mEditTake @0x28");

// Pin the RELATIVE placement of the three trailing widgets (these differences are
// independent of sizeof(ICETake)/the wider host time bars, so they hold on the host):
// mTargetBox @+0x7170, mWideScrMarker @+0x71E0 (delta 0x70), mLogo @+0x71E4 (delta 0x74).
static_assert(offsetof(ICEController, mWideScrMarker) - offsetof(ICEController, mTargetBox) == 0x70,
              "ICEController::mWideScrMarker sits 0x70 after mTargetBox (+0x7170 -> +0x71E0)");
static_assert(offsetof(ICEController, mLogo) - offsetof(ICEController, mTargetBox) == 0x74,
              "ICEController::mLogo sits 0x74 after mTargetBox (+0x7170 -> +0x71E4)");

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICECONTROLLER_HPP
