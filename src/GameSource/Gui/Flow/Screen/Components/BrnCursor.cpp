// BrnCursor.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::GuiCursor::Update @ 0x824941A8
//
//   if (!mpStateInterface) <fire "mpStateInterface" assert>   (non-fatal, DWARF :380)
//   build a 48-byte cursor event on the stack:
//       event.muHeader0   = 32      (li 0x20)
//       event.muEventType = 560     (li 0x230)
//       event.muHeader2   = 16      (li 0x10)
//       event.maPosition  = mv2Position      (lvx128 -> stvx128, 16-byte lane move)
//       event.miState0    = meDisplayState   (*(this + 0xE4))
//       event.miState1    = meAnimationState (*(this + 0xE8))
//   mpStateInterface->mOutEventQueue.AddEvent(&event, 40, 48)   (channel id 40 = GuiEventOut)
//
// The X360 reaches the queue as `mpStateInterface + 0xC` (the StateInterface's
// mOutEventQueue, a VariableEventQueue<65536,16>) and calls its 3-arg AddEvent. We reach
// it BY NAME through GetOutputEventQueue(); the queue's AddEvent is the inherited
// VariableEventQueue<65536,16>::AddEvent. The header/state words and position lane are
// copied into named event fields (the guest's lvx128/stvx128 is a plain 16-byte block
// move, lowered to scalar lane copies -- endian-independent, not a VMX pipeline). The
// X360-baked assert file/line are discarded per project convention.
//
// RETURN TYPE: void. `bl AddEvent` is the last call and is followed by the epilogue and a
// plain `blr` with r3 never touched, so the word Hex-Rays renders as a return value is
// AddEvent's leftover. The DWARF declares `void Update()` (BrnCursor.h:378).
//
// ===================================================================================
// F2 SMALL-CLOSURES WAVE (2026-08-29): the ten remaining X360-attested GuiCursor bodies
// land below, completing the TU at 11 of 11. Addresses, in declaration order:
//
//   Construct                        @0x82416690
//   ClampPosition                    @0x82425078
//   SetDelta(f32,f32,f32)            @0x82416750   (unnamed export: sub_82416750)
//   SetDelta(Vector2,f32)            @0x82410BF0
//   UpdateToSnapLocations            @0x8242C3A8
//   FindClosestSnapIndex             @0x824168B8
//   FindClosestSnapIndexInDirection  @0x82416AD0
//   SetActive                        @0x82416C70
//   SetInactive                      @0x82416C88
//   SetAlwaysSnap                    @0x82416CA0
//   SetPosition                      @0x82428AD8
//
// EVERY FLOAT BELOW IS A MEASURED BIG-ENDIAN IMAGE WORD, not a value read off the
// decompiler's rendering. The ARTIST image was decrypted (XEX2, devkit AES key, basic
// compression, image base 0x82000000) and each word read at its virtual address:
//   flt_82001CC0 = 00000000 =  0.0f          flt_82001C98 = 3F800000 =  1.0f
//   flt_820037C8 = BF800000 = -1.0f          flt_82005D9C = 461C4000 = 10000.0f
//   flt_8204F664 = 7F7FFFFF =  FLT_MAX       flt_8204C5B4 = 3F19999A =  0.60000002f
//   flt_8204C5B8 = 3E4CCCCD =  0.2f          flt_82004014 = 3DCCCCCD =  0.1f
//   flt_8204B630 = 34000000 =  FLT_EPSILON   flt_82005548 = 40200000 =  2.5f
//   flt_82004A20 = 41200000 = 10.0f          flt_820138DC = 42480000 = 50.0f
//   flt_82F256E8 = 40800000 =  4.0f          flt_82F256E4 = 40000000 =  2.0f
// flt_8204B630 == FLT_EPSILON is independently corroborated by the committed
// BrnBoostMessageManager.cpp:58 and BrnRoadRuleComponent.cpp:148 banners, which identify
// the same word from their own call sites.
//
// DE-SIMD'ING. The X360 spells every length as `vrsqrtefp` + two Newton-Raphson steps +
// a `vsel` against a zero-length compare. std::sqrt of the same dot product yields the
// same value INCLUDING the zero case, so it is the sanctioned scalar de-optimisation --
// the same call the committed BrnCrashNavMap_wJ_07.cpp makes, for the same reason.
// `fsel(a, b, c)` == `a >= 0.0f ? b : c` and is kept in the console's operand order.
// A `stvx128`/`lvx128` pair on a Vector2/Vector4 is a plain 16-byte block move; it is
// reproduced lane by lane, which is endian-independent.
//
// ASSERTS ARE NOT GUARDS. The X360-baked file/line pairs are discarded per project
// convention, and every assert is followed by the console's own unguarded continuation,
// reproduced as-is. Two of them (SetPosition's pair) stream the offending float into the
// message buffer via StrStream; CGS_ASSERT forwards a plain string, so the value is
// dropped and the axis is spelled into the message text instead.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnCursor.h"

#include <cfloat>   // FLT_MAX (flt_8204F664 == 0x7F7FFFFF)
#include <cmath>    // std::sqrt / std::fabs (the de-SIMD'd vrsqrtefp and vandc chains)

namespace BrnGui
{

namespace
{
    // ---- literals the DWARF does NOT name --------------------------------------------
    // All five are open-coded float literals in the original (no DWARF symbol of their
    // own) and all five live inside UpdateToSnapLocations. [FLAG consumer-named] -- the
    // VALUES are the measured words listed in the banner; only the NAMES are ours.
    const f32 KF_SNAP_LOCK_DISTANCE = 2.5f;    // flt_82005548 -- closer than this: hard snap
    const f32 KF_SNAP_NEAR_DISTANCE = 10.0f;   // flt_82004A20 -- near band upper bound
    const f32 KF_SNAP_FAR_DISTANCE  = 50.0f;   // flt_820138DC -- beyond this: no pull at all
    const f32 KF_SNAP_NEAR_PULL     = 4.0f;    // flt_82F256E8 -- numerator in the near band
    const f32 KF_SNAP_FAR_PULL      = 2.0f;    // flt_82F256E4 -- numerator in the far band

    // The "has the stick moved at all" test in UpdateToSnapLocations. The X360 spells it
    // `vandc` (clear the sign bit) + `vcmpgtfp`, per axis, against FLT_EPSILON.
    const f32 KF_DELTA_EPSILON = 1.1920929e-7f;   // flt_8204B630 == 0x34000000

    const f32 KF_CRAZY_POSITION = 10000.0f;   // flt_82005D9C -- SetPosition's sanity bound
    const f32 KF_MAX_AXIS_DELTA = 1.0f;       // flt_82001C98 -- SetDelta's per-axis bound
    const f32 KF_Y_AXIS_SIGN    = -1.0f;      // flt_820037C8 -- the y flip into device space

    // The scalar de-SIMD of `vmulfp128 + vspltw 0 + vspltw 1 + vaddfp + vrsqrtefp(+NR)`:
    // the 2-lane length of a Vector2. Kept local so every site below reads identically.
    inline f32 Length2(f32 lfX, f32 lfY)
    {
        return std::sqrt((lfX * lfX) + (lfY * lfY));
    }
}

// ---- the five DWARF-named class statics (definitions at BrnCursor.cpp:23-:27) ---------
const f32 GuiCursor::KF_SNAPDIRECTION_MIN_DOT   = 0.1f;          // flt_82004014
const f32 GuiCursor::KF_SNAPDIRECTION_DOTWEIGHT = 0.0f;          // flt_82001CC0 -- see
                                                                 // FindClosestSnapIndexInDirection
const f32 GuiCursor::KF_SNAP_DEADZONE           = 0.60000002f;   // flt_8204C5B4
const f32 GuiCursor::KF_AUTOREPEAT_WAIT_DELAY   = 0.2f;          // flt_8204C5B8
const f32 GuiCursor::KF_AUTOREPEAT_REPEAT_DELAY = 0.1f;          // flt_82004014

// @ 0x824941A8
void GuiCursor::Update()
{
    CGS_ASSERT( mpStateInterface != nullptr, "mpStateInterface" );

    GuiEventCursorUpdate lEvent;
    lEvent.muHeader0   = 32;
    lEvent.muEventType = 560;
    lEvent.muHeader2   = 16;

    // 16-byte position lane (X360 lvx128 from this+0xA0 -> stvx128 into the event).
    lEvent.maPosition[0] = mv2Position.x;
    lEvent.maPosition[1] = mv2Position.y;
    lEvent.maPosition[2] = mv2Position.z;
    lEvent.maPosition[3] = mv2Position.w;

    lEvent.miState0 = static_cast<s32>( meDisplayState );     // *(this + 0xE4)
    lEvent.miState1 = static_cast<s32>( meAnimationState );   // *(this + 0xE8)

    // Push onto the owning state's large output queue with channel id 40 (GuiEventOut),
    // record size 48 bytes (the X360 li r5=0x28=40 type, li r6=0x30=48 size).
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>( &lEvent ), 40, 48 );
}

// @ 0x82416690
// Chain to the GuiComponent base, then plant the idle cursor: the caller's movement
// scalar, the caller's start position in the +0xA0 lane (z/w zeroed by the `std r9` that
// fills the top half of the stack quad the following `lvx128` reads), a zero delta, a
// zero bounds rect, no snap lock, inactive display, idle animation, snapping off.
//
// PPC FLOAT ABI: f1/f2/f3 consume the r6/r7/r8 GPR slots, so the parent name arrives in
// r9 (`mr r6, r9` @0x824166AC hands it straight to the base call as its 3rd argument).
// mv2LastOutputPosition (+0xC0) is deliberately NOT written here -- the console leaves it
// holding whatever the pool memory held, and SetPosition's first call compares against it.
void GuiCursor::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                          f32 lfMovementScalar, f32 lfX, f32 lfY,
                          const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

    mfMovementScalar = lfMovementScalar;          // stfs f31, 0x8C(r31)
    mfFirstInputTime = 0.0f;                      // stfs f0(flt_82001CC0), 0x90(r31)
    mfLastInputTime  = 0.0f;                      // stfs f0, 0x94(r31)

    // stvx128 v13, r31, 0xA0 -- the quad built at var_40/var_3C plus the zeroed top half.
    mv2Position.x = lfX;
    mv2Position.y = lfY;
    mv2Position.z = 0.0f;
    mv2Position.w = 0.0f;

    mv2LastDelta.SetZero();                       // stvx128 v0(zero), r31, 0xB0
    mv4BoundsRect.SetZero();                      // stvx128 v0(zero), r31, 0xD0

    muLockedToIndex  = KU_INVALID_SNAP_INDEX;     // stw r6(-1), 0xE0(r31)
    meDisplayState   = E_DISPLAY_INACTIVE;        // stw r5(2),  0xE4(r31)
    meAnimationState = E_ANIM_IDLE;               // stw r10(4), 0xE8(r31)
    mbAlwaysSnap     = false;                     // stb r9(0),  0xEC(r31)
}

// @ 0x82425078
// Push mv2Position back inside mv4BoundsRect, which is laid out (minX, minY, maxX, maxY)
// across the (x, y, z, w) lanes -- that assignment is MEASURED, not assumed: the four
// early tests permute the bounds quad by 0, 8, 4 and 12 bytes (`lvsl`/`vperm` with
// r8 = 0/8/4/12) before comparing against the splatted x, x, y, y of the position.
//
// Returns false and touches nothing when the position is already inside; the console
// tests first and clamps second rather than clamping unconditionally.
bool GuiCursor::ClampPosition()
{
    // `vcmpgtfp.` + `mfocrf`/`extrwi 1,24` reads CR6 bit 0 ("all lanes true"); every lane
    // is a splat of the same scalar, so each test is exactly the scalar compare.
    if (!(mv4BoundsRect.x > mv2Position.x) &&    // r8 = 0  : bounds.x > pos.x
        !(mv2Position.x > mv4BoundsRect.z) &&    // r8 = 8  : pos.x    > bounds.z
        !(mv4BoundsRect.y > mv2Position.y) &&    // r8 = 4  : bounds.y > pos.y
        !(mv2Position.y > mv4BoundsRect.w))      // r8 = 12 : pos.y    > bounds.w
    {
        return false;                            // li r3, 0; blr
    }

    // `fsel f0, f12, f13, f0` == `f12 >= 0.0f ? f13 : f0`, kept in operand order. Each
    // axis is max-against-the-low-edge then min-against-the-high-edge, x first, then y.
    f32 lfX = ((mv4BoundsRect.x - mv2Position.x) >= 0.0f) ? mv4BoundsRect.x : mv2Position.x;
    lfX     = ((mv4BoundsRect.z - lfX)           >= 0.0f) ? lfX             : mv4BoundsRect.z;
    mv2Position.x = lfX;

    f32 lfY = ((mv4BoundsRect.y - mv2Position.y) >= 0.0f) ? mv4BoundsRect.y : mv2Position.y;
    lfY     = ((mv4BoundsRect.w - lfY)           >= 0.0f) ? lfY             : mv4BoundsRect.w;
    mv2Position.y = lfY;

    return true;                                 // li r3, 1
}

// @ 0x82416750  (exported unnamed as sub_82416750; identified by its two assert strings,
// which name the function outright: "... send to GuiCursor::SetDelta")
// The per-axis entry point: scale both axes by mfMovementScalar, flip the y axis (device
// space grows downward while the stick's +y is up) and hand the pair to the vector form.
void GuiCursor::SetDelta(f32 lfXDelta, f32 lfYDelta, f32 lfTimeStep)
{
    // BrnCursor.cpp:94 / :95 on the console. Non-fatal: the body proceeds either way.
    CGS_ASSERT(std::fabs(lfXDelta) <= KF_MAX_AXIS_DELTA,
               "Invalid x delta send to GuiCursor::SetDelta");
    CGS_ASSERT(std::fabs(lfYDelta) <= KF_MAX_AXIS_DELTA,
               "Invalid y delta send to GuiCursor::SetDelta");

    Vector2 lv2Delta;
    lv2Delta.x = mfMovementScalar * lfXDelta;                     // fmuls f0, f0, f31
    lv2Delta.y = (mfMovementScalar * lfYDelta) * KF_Y_AXIS_SIGN;  // fmuls f13, then * -1.0f
    lv2Delta.z = 0.0f;                                            // std r25(0), 0(r11)
    lv2Delta.w = 0.0f;

    SetDelta(lv2Delta, lfTimeStep);
}

// @ 0x82410BF0
// The vector form, and the fork that decides what "moving the cursor" even means:
//
//  * snapping OFF -- the cursor is a free pointer, so the delta is simply integrated into
//    the position and nothing else happens (no autorepeat, no latch).
//  * snapping ON  -- the delta is a DISCRETE menu nudge, so it is gated by an autorepeat
//    timer and only ever LATCHED into mv2LastDelta for UpdateToSnapLocations to consume;
//    the position is not touched here at all. The first push past the deadzone fires
//    immediately, then the stick must be held KF_AUTOREPEAT_WAIT_DELAY before the second
//    step and KF_AUTOREPEAT_REPEAT_DELAY between every step after that. Letting the stick
//    fall back inside the deadzone clears both timers, so the next push fires at once.
//
// lfTimeStep is an ABSOLUTE time, not a frame delta: it is compared against the stored
// times PLUS a delay, and stored into them verbatim. (The DWARF's parameter name is kept.)
void GuiCursor::SetDelta(Vector2 lv2Delta, f32 lfTimeStep)
{
    if (!mbAlwaysSnap)
    {
        // `lvx128 / vaddfp128 / stvx128` on this+0xA0 -- an all-lane add.
        mv2Position.x += lv2Delta.x;
        mv2Position.y += lv2Delta.y;
        mv2Position.z += lv2Delta.z;
        mv2Position.w += lv2Delta.w;
        return;
    }

    // BrnCursor.h:272 on the console. Non-fatal: the timing arithmetic below runs anyway.
    CGS_ASSERT(lfTimeStep > 0.0f, "Must have a valid time to use autorepeat");

    const f32 lfMagnitude = Length2(lv2Delta.x, lv2Delta.y);

    if (lfMagnitude > KF_SNAP_DEADZONE)
    {
        bool lbAcceptStep = false;

        if (mfFirstInputTime == 0.0f)
        {
            mfFirstInputTime = lfTimeStep;   // stfs f30, 0x90(r31)
            lbAcceptStep = true;
        }
        else if (lfTimeStep > (mfFirstInputTime + KF_AUTOREPEAT_WAIT_DELAY) &&
                 lfTimeStep > (mfLastInputTime  + KF_AUTOREPEAT_REPEAT_DELAY))
        {
            mfLastInputTime = lfTimeStep;    // stfs f30, 0x94(r31)
            lbAcceptStep = true;
        }

        if (lbAcceptStep)
        {
            mv2LastDelta = lv2Delta;         // stvx128 v127, r31, 0xB0
        }
    }
    else
    {
        mfFirstInputTime = 0.0f;             // stfs f31(flt_82001CC0), 0x90(r31)
        mfLastInputTime  = 0.0f;             // stfs f31, 0x94(r31)
    }
}

// @ 0x824168B8
// Plain nearest-neighbour over the caller's snap list, measured from the CURRENT cursor
// position. Three asserts bracket it and none of them is a guard: with an empty list the
// console still runs the (zero-trip) loop and still returns KU_INVALID_SNAP_INDEX.
u32 GuiCursor::FindClosestSnapIndex(Vector2* lpv2Locations, u32 luNumLocations)
{
    u32 luClosestIndex    = KU_INVALID_SNAP_INDEX;   // li r20, -1
    f32 lfClosestDistance = FLT_MAX;                 // flt_8204F664 == 0x7F7FFFFF

    // BrnCursor.cpp:275.
    CGS_ASSERT(luNumLocations != 0, "No snap locations provided");

    for (u32 luIndex = 0; luIndex < luNumLocations; ++luIndex)
    {
        // `vsubfp v0, v9(position), v0(location)` then the 2-lane length.
        const f32 lfDistance = Length2(mv2Position.x - lpv2Locations[luIndex].x,
                                       mv2Position.y - lpv2Locations[luIndex].y);

        if (lfDistance < lfClosestDistance)
        {
            lfClosestDistance = lfDistance;
            luClosestIndex    = luIndex;
        }
    }

    // BrnCursor.cpp:290 / :291.
    CGS_ASSERT(luClosestIndex != KU_INVALID_SNAP_INDEX, "Did not find an index to snap to.");
    CGS_ASSERT(luClosestIndex < luNumLocations,
               "Found an index larger than the number of indices to snap to");

    return luClosestIndex;
}

// @ 0x82416AD0
// Nearest-neighbour WEIGHTED BY DIRECTION: the caller's direction is normalised once, each
// candidate's offset from the cursor is normalised per iteration, and only candidates whose
// dot with the direction reaches KF_SNAPDIRECTION_MIN_DOT are considered at all. The score
// is `dot / distance + dot * KF_SNAPDIRECTION_DOTWEIGHT` and the LARGEST score wins, so
// among the candidates lying in roughly the right direction the nearest one is preferred.
//
// KF_SNAPDIRECTION_DOTWEIGHT IS 0.0f IN THE SHIPPED IMAGE (flt_82001CC0 == 0x00000000, the
// same rodata zero Construct stores into the input timers). The `fmadds f0, f0, f12, f10`
// that applies it is emitted regardless, so it is reproduced rather than folded away -- but
// the second term contributes nothing at runtime. [FLAG tuned-to-zero]
//
// No assert fires here, and there is no zero-length guard on the DIRECTION (unlike the
// per-candidate offset, which does get the `vsel` zero guard): the only caller,
// UpdateToSnapLocations, has already established that the direction is non-zero.
//
// The initial best score is 0.0f rather than -FLT_MAX, so a candidate must beat zero
// outright; with a non-negative distance and a dot of at least +0.1 that is always true
// for anything that passes the gate.
u32 GuiCursor::FindClosestSnapIndexInDirection(Vector2* lpv2Locations, u32 luNumLocations,
                                               Vector2 lv2Direction)
{
    u32 luBestIndex = KU_INVALID_SNAP_INDEX;   // li r3, -1
    f32 lfBestScore = 0.0f;                    // fmr f13, f12 (flt_82001CC0)

    const f32 lfDirectionLength = Length2(lv2Direction.x, lv2Direction.y);
    const f32 lfDirectionX      = lv2Direction.x / lfDirectionLength;
    const f32 lfDirectionY      = lv2Direction.y / lfDirectionLength;

    for (u32 luIndex = 0; luIndex < luNumLocations; ++luIndex)
    {
        // `vsubfp v10, v0(location), v8(position)` -- note this one is location MINUS
        // position, the opposite order to FindClosestSnapIndex above.
        const f32 lfOffsetX = lpv2Locations[luIndex].x - mv2Position.x;
        const f32 lfOffsetY = lpv2Locations[luIndex].y - mv2Position.y;

        const f32 lfDistance = Length2(lfOffsetX, lfOffsetY);
        const f32 lfDot      = (lfDirectionX * (lfOffsetX / lfDistance)) +
                               (lfDirectionY * (lfOffsetY / lfDistance));

        if (lfDot >= KF_SNAPDIRECTION_MIN_DOT)
        {
            const f32 lfScore = (lfDot / lfDistance) + (lfDot * KF_SNAPDIRECTION_DOTWEIGHT);

            if (lfScore > lfBestScore)
            {
                lfBestScore = lfScore;
                luBestIndex = luIndex;
            }
        }
    }

    return luBestIndex;
}

// @ 0x8242C3A8
// The per-frame snap driver, and the only thing that ever consumes the mv2LastDelta latch
// SetDelta builds.
//
// Pick a target: while snapping is on, a latched stick nudge means "move in that direction"
// (directional search) and no nudge means "re-find where I already am" (plain nearest);
// with snapping off it is always the plain nearest. Either way the latch is consumed --
// zeroed -- so one stick push produces exactly one step.
//
// Then decide how hard to pull. With snapping on the cursor is placed exactly on the
// target. With snapping off the pull is graded by distance: inside 2.5 units it locks on
// outright, out to 10 units it is dragged 4/d of the way there, out to 50 units 2/d of the
// way, and beyond 50 it is left alone entirely -- which is what makes a free cursor feel
// magnetic near an icon and free in open space. Only the lock-on cases count as "snapped",
// and that flag is what drives the display state and the reported lock index.
//
// lbForceSnap is the CLAMP flag (it is forwarded to ClampPosition and to SetPosition, not
// to either search); the DWARF's parameter name is kept.
GuiCursor::SnapResults GuiCursor::UpdateToSnapLocations(Vector2* lpv2Locations,
                                                        u32 luNumLocations,
                                                        bool lbForceSnap)
{
    SnapResults lResults;

    bool lbSnapped = false;   // r28
    u32  luIndex;             // r3

    if (mbAlwaysSnap)
    {
        lbSnapped = true;   // li r28, 1 -- set BEFORE the search, unconditionally

        // The latch is read once into v1 and reused as the direction argument.
        const Vector2 lv2LastDelta = mv2LastDelta;

        // `vandc` clears the sign bit (abs) and `vcmpgtfp` compares against FLT_EPSILON:
        // "did the stick move at all this frame", per axis.
        if (std::fabs(lv2LastDelta.x) > KF_DELTA_EPSILON ||
            std::fabs(lv2LastDelta.y) > KF_DELTA_EPSILON)
        {
            luIndex = FindClosestSnapIndexInDirection(lpv2Locations, luNumLocations,
                                                      lv2LastDelta);
        }
        else
        {
            luIndex = FindClosestSnapIndex(lpv2Locations, luNumLocations);
        }

        mv2LastDelta.SetZero();   // stvx128 v0(zero), r0, r29 -- on BOTH arms
    }
    else
    {
        luIndex = FindClosestSnapIndex(lpv2Locations, luNumLocations);
    }

    if (luIndex != KU_INVALID_SNAP_INDEX)
    {
        const Vector2 lv2Target = lpv2Locations[luIndex];   // lvx128 v8, r10(index*16), r30

        // `vsubfp v12, v13(position), v8(target)` -- kept whole for the pull below.
        const f32 lfOffsetX = mv2Position.x - lv2Target.x;
        const f32 lfOffsetY = mv2Position.y - lv2Target.y;
        const f32 lfOffsetZ = mv2Position.z - lv2Target.z;
        const f32 lfOffsetW = mv2Position.w - lv2Target.w;

        const f32 lfDistance = Length2(lfOffsetX, lfOffsetY);

        if (mbAlwaysSnap)
        {
            lbSnapped   = true;
            mv2Position = lv2Target;                 // stvx128 v8, r0, r11
        }
        else if (lfDistance < KF_SNAP_LOCK_DISTANCE)
        {
            lbSnapped   = true;
            mv2Position = lv2Target;
        }
        else
        {
            f32  lfPull    = 0.0f;
            bool lbHasPull = false;

            if (lfDistance < KF_SNAP_NEAR_DISTANCE)
            {
                lfPull    = KF_SNAP_NEAR_PULL / lfDistance;   // fdivs f0, f13, f0
                lbHasPull = true;
            }
            else if (lfDistance < KF_SNAP_FAR_DISTANCE)
            {
                lfPull    = KF_SNAP_FAR_PULL / lfDistance;
                lbHasPull = true;
            }

            // Beyond KF_SNAP_FAR_DISTANCE the console branches straight past the store,
            // so the position is left exactly as it was.
            if (lbHasPull)
            {
                // `vspltw` the scalar, `vmulfp128` against the offset, `vsubfp` from the
                // position -- an all-lane operation, so all four lanes are written.
                mv2Position.x -= lfOffsetX * lfPull;
                mv2Position.y -= lfOffsetY * lfPull;
                mv2Position.z -= lfOffsetZ * lfPull;
                mv2Position.w -= lfOffsetW * lfPull;
            }
        }
    }

    if (lbSnapped)
    {
        // Read BEFORE the index store, exactly as the console orders the two.
        const DisplayState lePreviousState = meDisplayState;

        muLockedToIndex = luIndex;

        if (lePreviousState != E_DISPLAY_ACTIVE_SNAP ||
            (mbAlwaysSnap && luIndex != KU_INVALID_SNAP_INDEX))
        {
            meDisplayState = E_DISPLAY_ACTIVE_SNAP;
        }
    }
    else
    {
        const DisplayState lePreviousState = meDisplayState;

        muLockedToIndex = KU_INVALID_SNAP_INDEX;

        if (lePreviousState != E_DISPLAY_ACTIVE_UNSNAP)
        {
            meDisplayState = E_DISPLAY_ACTIVE_UNSNAP;
        }
    }

    // v126 is captured BEFORE the clamp and v127 after it but before SetPosition, so the
    // reported offset is exactly what ClampPosition moved -- SetPosition's own optional
    // clamp is deliberately not folded back into it.
    const Vector2 lv2BeforeClamp = mv2Position;

    if (lbForceSnap)
    {
        ClampPosition();
    }

    const Vector2 lv2AfterClamp = mv2Position;

    SetPosition(lv2AfterClamp, lbForceSnap);

    lResults.lv2Offset.x   = lv2BeforeClamp.x - lv2AfterClamp.x;   // vsubfp128 v0, v126, v127
    lResults.lv2Offset.y   = lv2BeforeClamp.y - lv2AfterClamp.y;
    lResults.lv2Offset.z   = lv2BeforeClamp.z - lv2AfterClamp.z;
    lResults.lv2Offset.w   = lv2BeforeClamp.w - lv2AfterClamp.w;
    lResults.luLockedIndex = muLockedToIndex;                      // stw r11, 0x10(r26)

    return lResults;
}

// @ 0x82416C70
// Whole body measured: `lwz r11,0xE8(r3); cmpwi r11,0; beqlr; li r11,0; stw r11,0xE8(r3)`.
void GuiCursor::SetActive()
{
    if (meAnimationState != E_ANIM_TRANS_IN)
    {
        meAnimationState = E_ANIM_TRANS_IN;
    }
}

// @ 0x82416C88
// Whole body: `li r11,2; li r10,1; stw r11,0xE4(r3); stw r10,0xE8(r3); blr`.
void GuiCursor::SetInactive()
{
    meDisplayState   = E_DISPLAY_INACTIVE;
    meAnimationState = E_ANIM_TRANS_OUT;
}

// @ 0x82416CA0
// Whole body: `stb r4, 0xEC(r3); blr`.
void GuiCursor::SetAlwaysSnap(bool lbAlwaysSnap)
{
    mbAlwaysSnap = lbAlwaysSnap;
}

// @ 0x82428AD8
// The public position writer. It early-outs when the caller hands back the position it was
// last given, which is what keeps a stationary cursor from re-clamping and re-asserting
// every frame; note the guard compares against mv2LastOutputPosition, NOT mv2Position, so
// an internal move (the snap pull above) is still published on the next call.
void GuiCursor::SetPosition(Vector2 lv2Position, bool lbClamp)
{
    // `vrlimi128 v0, v1, 3, 2` duplicates (x, y) into the z/w lanes of both operands before
    // `vcmpeqfp.`, so the all-lanes-equal bit is exactly "x equal AND y equal".
    if (lv2Position.x == mv2LastOutputPosition.x &&
        lv2Position.y == mv2LastOutputPosition.y)
    {
        return;
    }

    mv2Position = lv2Position;              // stvx128 v1, r0, r30 (this+0xA0)

    if (lbClamp)
    {
        ClampPosition();
    }

    mv2LastOutputPosition = mv2Position;    // lvx128 from +0xA0, stvx128 to +0xC0

    // BrnCursor.h:245 / :249. The console streams the offending float into the message
    // ("Crazy position being printed in the cursor : " << value << "\n"); CGS_ASSERT takes
    // a plain string, so the axis is named in the text instead and the value is dropped.
    CGS_ASSERT(std::fabs(mv2Position.x) < KF_CRAZY_POSITION,
               "Crazy position being printed in the cursor : x");
    CGS_ASSERT(std::fabs(mv2Position.y) < KF_CRAZY_POSITION,
               "Crazy position being printed in the cursor : y");
}

} // namespace BrnGui
