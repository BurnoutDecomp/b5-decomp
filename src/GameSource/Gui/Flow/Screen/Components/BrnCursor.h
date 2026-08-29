// BrnCursor.h
// Home of BrnGui::GuiCursor -- the GUI screen cursor component. It owns a device-space
// position inside a bounds rect, optionally snaps that position to a caller-supplied list
// of snap locations, and each frame publishes its position + state onto the owning GUI
// state's output event queue so the view layer can draw it.
//
// SHAPE. GuiCursor derives from CgsGui::GuiComponent and adds no virtuals of its own --
// the DecFIGS DWARF (references/DecFIGS/dwarfdump/.../BrnCursor.h:337) spells the base
// explicitly and marks no member `virtual` (the dumper does print `virtual` where it
// applies -- CgsGuiComponent.h:158 -- so its absence here is a positive fact, not a gap).
// The base is corroborated by the X360: GuiCursor::Construct @0x82416690 opens with
// `bl CgsGui::GuiComponent::Construct` on the same `this`, and its first own member store
// lands at +0x8C -- exactly where GuiComponent's 32-bit layout ends
// (vptr 0x00 / macName[128] 0x04 / muHashedName 0x84 / mpStateInterface 0x88).
//
// MEMBER LAYOUT. Names, types and order are DWARF (BrnCursor.h:177-:193); every offset in
// the comments below is X360-MEASURED, not inferred:
//   +0x8C mfMovementScalar        Construct: `stfs f31, 0x8C(r31)`   (f1, the 3rd param)
//   +0x90 mfFirstInputTime        Construct: `stfs f0, 0x90(r31)` (0.0)
//   +0x94 mfLastInputTime         Construct: `stfs f0, 0x94(r31)` (0.0)
//   +0xA0 mv2Position             Construct `stvx128 v13, r31, 0xA0` = {f2, f3, 0, 0};
//                                 read by Update, SetPosition, FindClosestSnapIndex,
//                                 ClampPosition (`addi r11, r7, 0xA0`).
//   +0xB0 mv2LastDelta            Construct zeroes it (`stvx128 v0, r31, 0xB0`);
//                                 SetDelta(Vector2,f32) accumulates into it;
//                                 UpdateToSnapLocations reads it (`a2 + 176`).
//   +0xC0 mv2LastOutputPosition   SetPosition compares against, then refreshes, `this+0xC0`.
//   +0xD0 mv4BoundsRect           Construct zeroes it; ClampPosition reads it against
//                                 mv2Position (`addi r10, r7, 0xD0`).
//   +0xE0 muLockedToIndex         Construct: `stw r6(-1), 0xE0` == KU_INVALID_SNAP_INDEX.
//   +0xE4 meDisplayState          Construct: `stw r5(2), 0xE4` == E_DISPLAY_INACTIVE;
//                                 SetInactive: `stw r11(2), 0xE4(r3)`.
//   +0xE8 meAnimationState        Construct: `stw r10(4), 0xE8` == E_ANIM_IDLE;
//                                 SetInactive: `stw r10(1), 0xE8` == E_ANIM_TRANS_OUT;
//                                 SetActive tests/clears it to 0 == E_ANIM_TRANS_IN.
//   +0xEC mbAlwaysSnap            Construct: `stb r9(0), 0xEC`; SetAlwaysSnap is the whole
//                                 one-instruction body `stb r4, 0xEC(r3)`.
// The two enum value sets fall out of those stores exactly as the DWARF names them
// (DisplayState INACTIVE==2, AnimationState IDLE==4 / TRANS_OUT==1 / TRANS_IN==0), which
// is why the enum-to-offset assignment above is measured rather than assumed.
//
// HOST-LAYOUT CAVEAT. The X360 offsets are COMMENTS ONLY. On the LLP64 host the base's
// vptr and mpStateInterface both widen, so nothing here sits at its console offset; the
// reconstruction is name-based throughout. _AssertLayout below pins only the RELATIVE
// deltas inside the pointer-free scalar run +0x8C..+0xEC, which the widening cannot move.
//
// BODIES. Everything declared without a body belongs to the BrnCursor.cpp TU (ten
// X360-attested symbols; only Update is reconstructed here so far). The handful of
// accessors defined inline are the ones the original defines in the header (DWARF puts
// their definition lines at BrnCursor.h:212/:334/:348, i.e. after the class body) and that
// the X360 therefore never emits as symbols -- every call site folds them, so no TU will
// ever home them and a bare declaration would never link.

#pragma once

#include <stddef.h>                                              // offsetof (_AssertLayout)

#include "types.hpp"
#include "BrnCommonTypes.h"                                      // Vector2 / Vector4
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"      // CgsGui::GuiComponent (base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::Event base

namespace BrnGui
{
    // Friend of GuiCursor: CrashNavMap::UpdateCursorStatus @0x824CB770 pokes
    // muLockedToIndex / meDisplayState directly (the X360 inlines both stores rather than
    // calling a setter), so it needs the private carve. Same precedent as the
    // GuiCache / MapIconManager friendships.
    struct CrashNavMap;

    // The cursor event Update publishes. The X360 fills a fixed 3-word header
    // { muHeader0 = 32, muEventType = 560, muHeader2 = 16 } then a 16-byte position lane
    // and two trailing state words, total 48 bytes, pushed with channel id 40. The
    // header word names mirror the CgsGui::GuiEvent base shape (header0 / type / header2).
    struct GuiEventCursorUpdate : public CgsModule::Event
    {
        u32 muHeader0;      // @0x00 = 32   (event +0x00)
        u32 muEventType;    // @0x04 = 560  (event +0x04, the cursor event type id)
        u32 muHeader2;      // @0x08 = 16   (event +0x08)
        u32 muReserved;     // @0x0C        (X360 leaves this gap word uninitialised)
        f32 maPosition[4];  // @0x10 .. 0x1F  (cursor position lane, copied from the cursor)
        s32 miState0;       // @0x20        (cursor display state word)
        s32 miState1;       // @0x24        (cursor animation state word)
        u32 maTail[2];      // @0x28 .. 0x2F  (X360 leaves these tail words uninitialised)
    };

    // The screen cursor component.
    class GuiCursor : public CgsGui::GuiComponent
    {
    public:
        // DWARF BrnCursor.h:47 -- values pinned by Construct (`stw 2, 0xE4`) and
        // SetInactive (`li r11, 2; stw r11, 0xE4(r3)`).
        enum DisplayState
        {
            E_DISPLAY_ACTIVE_SNAP   = 0,
            E_DISPLAY_ACTIVE_UNSNAP = 1,
            E_DISPLAY_INACTIVE      = 2,
        };

        // DWARF BrnCursor.h:54 -- values pinned by Construct (`stw 4, 0xE8` = IDLE),
        // SetInactive (`stw 1, 0xE8` = TRANS_OUT) and SetActive (clears to 0 = TRANS_IN).
        enum AnimationState
        {
            E_ANIM_TRANS_IN       = 0,
            E_ANIM_TRANS_OUT      = 1,
            E_ANIM_TRANS_SNAP_IN  = 2,
            E_ANIM_TRANS_SNAP_OUT = 3,
            E_ANIM_IDLE           = 4,
        };

        // DWARF BrnCursor.h:64-:67. Returned by value from UpdateToSnapLocations; the
        // X360 passes a 32-byte sret buffer and writes the locked index at sret+0x10,
        // i.e. straight after the 16-byte Vector2 lane -- which is this member order.
        struct SnapResults
        {
            static const u32 KI_INVALID_LOCK_INDEX = 0xFFFFFFFFu;   // DWARF h:65

            Vector2 lv2Offset;      // DWARF h:66  (sret +0x00)
            u32     luLockedIndex;  // DWARF h:67  (sret +0x10)
        };

        // ---- Bodies owned by the BrnCursor.cpp TU (declaration-only) -----------------
        // Signatures are the DWARF's, confirmed against the X360 prologues.

        // @0x82416690 -- chains to CgsGui::GuiComponent::Construct(name, iface, parent),
        // then mfMovementScalar = lfMovementScalar, mv2Position = {lfX, lfY}, zeroes
        // mv2LastDelta + mv4BoundsRect and sets muLockedToIndex/meDisplayState/
        // meAnimationState/mbAlwaysSnap to their idle defaults.
        // PPC FLOAT ABI: f1/f2/f3 consume the r6/r7/r8 GPR slots, so the parent-name
        // pointer arrives in r9 -- register NUMBER is not parameter POSITION. This is the
        // 6-parameter shape the DWARF declares (h:78), not a 4-parameter one.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       f32 lfMovementScalar, f32 lfX, f32 lfY,
                       const char* lpacParentName);

        // @0x82425078 (DWARF h:93) -- clamp mv2Position into mv4BoundsRect.
        bool ClampPosition();

        // @0x82416750 (DWARF h:100 / cpp:92) -- per-axis delta with autorepeat timing;
        // asserts |x| and |y| <= 1 ("Invalid x delta send to GuiCursor::SetDelta",
        // BrnCursor.cpp:94). Unnamed in the X360 export; identified by that assert string.
        void SetDelta(f32 lfXDelta, f32 lfYDelta, f32 lfTimeStep);

        // @0x82410BF0 (DWARF h:268) -- the vector form; asserts a positive time
        // ("Must have a valid time to use autorepeat", BrnCursor.h:272).
        void SetDelta(Vector2 lv2Delta, f32 lfTimeStep);

        // @0x8242C3A8 (DWARF h:113) -- snap to the nearest of luNumLocations locations
        // and report the offset applied plus the index locked onto.
        SnapResults UpdateToSnapLocations(Vector2* lpv2Locations, u32 luNumLocations,
                                          bool lbForceSnap);

        // @0x824168B8 (DWARF h:119) -- asserts luNumLocations != 0 ("No snap locations
        // provided", BrnCursor.cpp:275); returns KU_INVALID_SNAP_INDEX when none matches.
        u32 FindClosestSnapIndex(Vector2* lpv2Locations, u32 luNumLocations);

        // @0x82416AD0 (DWARF h:126) -- as above, but weighted towards a direction.
        u32 FindClosestSnapIndexInDirection(Vector2* lpv2Locations, u32 luNumLocations,
                                            Vector2 lv2Direction);

        // @0x82416C70 (DWARF h:130) -- whole body measured:
        //   `lwz r11,0xE8(r3); cmpwi r11,0; beqlr; li r11,0; stw r11,0xE8(r3)`
        // i.e. `if (meAnimationState != E_ANIM_TRANS_IN) meAnimationState = E_ANIM_TRANS_IN;`
        void SetActive();

        // @0x82416C88 (DWARF h:134) -- `meDisplayState = E_DISPLAY_INACTIVE;
        // meAnimationState = E_ANIM_TRANS_OUT;`
        void SetInactive();

        // @0x82416CA0 (DWARF h:147) -- the whole body is `stb r4, 0xEC(r3)`.
        void SetAlwaysSnap(bool lbAlwaysSnap);

        // @0x82428AD8 (DWARF h:230) -- store, optionally clamp, then mirror into
        // mv2LastOutputPosition; early-outs when the new position equals the last output.
        void SetPosition(Vector2 lv2Position, bool lbClamp);

        // @0x824941A8 (DWARF h:378) -- publish the cursor event onto the owning state's
        // output queue. Returns void: the X360 `bl AddEvent` is followed by the epilogue
        // and `blr` with r3 untouched, so the value Hex-Rays shows in r3 is the callee's
        // leftover, not a return value. Bodied in BrnCursor.cpp.
        void Update();

        // ---- Accessors the original defines inline in this header --------------------
        // No X360 symbol exists for any of these (they are not in the ledger); every call
        // site folds them, which is what makes writing the body here correct rather than
        // invented -- each one is a single named member the DWARF pairs it with.

        // DWARF h:212 -- the writer for mv4BoundsRect. CrashNavMap::OnEnter (@0x824CB158)
        // installs the reference device rect into that lane with an inlined 16-byte store
        // immediately after constructing the cursor: `li r11, 0x5F30; stvx128 v0, r31, r11`
        // @0x824CB2C8, where 0x5F30 == mCursor (this+0x5E60) + 0xD0.
        void SetBounds(Vector4 lv4BoundsRect) { mv4BoundsRect = lv4BoundsRect; }

        // DWARF h:334 -- the +0xA0 position lane; call sites read it as `lvx128` from
        // cursor+0xA0 (e.g. CrashNavMap::OnEnter's `lvx128 v1, r31, 24320`).
        Vector2 GetPosition() const { return mv2Position; }

        // DWARF h:348 -- non-const in the DWARF; kept non-const to match that shape.
        bool GetAlwaysSnap() { return mbAlwaysSnap; }

        // DWARF h:320 -- the reader paired with SetBounds (h:212). Same footing as
        // GetPosition: header-defined in the original, no X360 symbol, every call site
        // folds it. ADDITIVE GROW (F2 small-closures wave).
        Vector4 GetBounds() const { return mv4BoundsRect; }

        // NOT DECLARED HERE: ClearSelection() (DWARF h:362). It has no X360 symbol, no
        // reconstructed call site and no inline expansion anywhere in the export set, so
        // its body is unrecoverable and a bare declaration would only be an unlinkable
        // trap. [FLAG unrecovered] Add it with its body when a call site lands.

    private:
        // CrashNavMap::UpdateCursorStatus writes muLockedToIndex / meDisplayState and
        // reads KU_INVALID_SNAP_INDEX directly (all three stores are inlined in the X360).
        friend struct CrashNavMap;

        // DWARF BrnCursor.h:163. Construct writes it into muLockedToIndex as `li r6, -1`.
        static const u32 KU_INVALID_SNAP_INDEX = 0xFFFFFFFFu;

        // DWARF h:165/:166/:168/:169/:170 -- five private class statics whose DEFINITIONS
        // the DWARF places at BrnCursor.cpp:23-:27, which is where they are defined here.
        // Every value below is the MEASURED big-endian rodata word behind the literal the
        // X360 loads at the one site that uses it (see the BrnCursor.cpp banner for the
        // address of each): they are not float literals guessed from the pseudocode.
        static const f32 KF_SNAPDIRECTION_MIN_DOT;      // h:165
        static const f32 KF_SNAPDIRECTION_DOTWEIGHT;    // h:166
        static const f32 KF_SNAP_DEADZONE;              // h:168
        static const f32 KF_AUTOREPEAT_WAIT_DELAY;      // h:169
        static const f32 KF_AUTOREPEAT_REPEAT_DELAY;    // h:170

        // ALSO DWARF-attested at h:172-:175 / cpp:29-:32 and deliberately NOT declared:
        // mXAxisName[3], mYAxisName[3], macTransitionVar[15], macShouldTransition[21].
        // None of the eleven X360 bodies reads them (they belong to ClearSelection / the
        // input-binding path), so their contents are unattested. [FLAG unrecovered]

        // DWARF h:177-:193. Trailing comments are X360 offsets -- documentation only.
        f32            mfMovementScalar;        // X360 +0x8C
        f32            mfFirstInputTime;        // X360 +0x90
        f32            mfLastInputTime;         // X360 +0x94
        Vector2        mv2Position;             // X360 +0xA0
        Vector2        mv2LastDelta;            // X360 +0xB0
        Vector2        mv2LastOutputPosition;   // X360 +0xC0
        Vector4        mv4BoundsRect;           // X360 +0xD0
        u32            muLockedToIndex;         // X360 +0xE0
        DisplayState   meDisplayState;          // X360 +0xE4
        AnimationState meAnimationState;        // X360 +0xE8
        bool           mbAlwaysSnap;            // X360 +0xEC

        // Never called. Pins the pointer-free scalar run's RELATIVE deltas, which are
        // invariant under the 32->64 bit pointer widening; absolute console offsets are
        // deliberately NOT asserted (the base widens underneath this run).
        static void _AssertLayout()
        {
            static_assert(offsetof(GuiCursor, mfFirstInputTime)
                        - offsetof(GuiCursor, mfMovementScalar) == 0x04, "cursor scalars");
            static_assert(offsetof(GuiCursor, mfLastInputTime)
                        - offsetof(GuiCursor, mfFirstInputTime) == 0x04, "cursor scalars");
            static_assert(offsetof(GuiCursor, mv2LastDelta)
                        - offsetof(GuiCursor, mv2Position) == 0x10, "cursor vector lanes");
            static_assert(offsetof(GuiCursor, mv2LastOutputPosition)
                        - offsetof(GuiCursor, mv2LastDelta) == 0x10, "cursor vector lanes");
            static_assert(offsetof(GuiCursor, mv4BoundsRect)
                        - offsetof(GuiCursor, mv2LastOutputPosition) == 0x10, "cursor vector lanes");
            static_assert(offsetof(GuiCursor, muLockedToIndex)
                        - offsetof(GuiCursor, mv4BoundsRect) == 0x10, "cursor state words");
            static_assert(offsetof(GuiCursor, meDisplayState)
                        - offsetof(GuiCursor, muLockedToIndex) == 0x04, "cursor state words");
            static_assert(offsetof(GuiCursor, meAnimationState)
                        - offsetof(GuiCursor, meDisplayState) == 0x04, "cursor state words");
            static_assert(offsetof(GuiCursor, mbAlwaysSnap)
                        - offsetof(GuiCursor, meAnimationState) == 0x04, "cursor state words");
            static_assert(offsetof(SnapResults, luLockedIndex) == 0x10,
                          "SnapResults locked index follows the 16-byte offset lane (sret+0x10)");
        }
    };
}
