// ============================================================================
// SDKs/Packages/ICE/ICEControllerEditOps.cpp
//
// ICE::ICEController -- the EDIT-OPS group of the in-game ICE camera-take editor:
// ChangeKeyElementValue / ChangeKeyElementValues. These translate the per-frame
// joystick-velocity accumulator into edits on the live edit take.
//
//   * ChangeKeyElementValue(liChangeIndex) applies one element-description's worth
//     of accumulated joystick drive to the take. liChangeIndex selects the element
//     description (ICEElementDescriptions[liChangeIndex]); the description's input
//     type (mInputType @0x38) chooses the maths:
//       - eICE_INPUT_ANALOG      (1): a direct velocity*timestep delta, fed to
//                                     ICEAuthor::SetKeyElementValueChange.
//       - eICE_INPUT_ACCELERATED (3): a two-axis accelerated/decayed velocity ramp
//                                     held in a second 37-slot f32 accumulator, the
//                                     summed ramp scaled by timestep then fed to
//                                     ICEAuthor::SetKeyElementValueChange.
//       - eICE_INPUT_DISCREET    (2): a stepped integer increment (one quant step
//                                     per pass over a 0.1 dead-zone), clamped to the
//                                     description's [mMin, mMax] read as int bits,
//                                     written straight onto the edit take.
//
//   * ChangeKeyElementValues() is the driver: for each of the 37 change slots it
//     walks the element schema and, for every description bound to the current
//     channel whose inc/dec button indices match the slot, calls
//     ChangeKeyElementValue for that slot.
//
// The reconstruction reverses compiler inlining of the embedded ICETake / change-
// buffer / element-description accesses back into named-member access against the
// layout in ICEController.hpp and the element schema in ICEDataEnums.hpp.
//
// FLAG (ICEAuthor base): the controller IS-AN ICEAuthor (Construct chains the base).
// These bodies call ICEAuthor methods (SetKeyElementValueChange, IsEndKeyState,
// GetIntervalKey) on `this`. ICEController.hpp does not yet derive from ICEAuthor
// (the base header is owned in parallel), so those calls go through
// reinterpret_cast<ICE::ICEAuthor*>(this) -- the same object, same address. Move to
// plain base-method calls once ICEController becomes `: public ICEAuthor`.
//
// FLAG (missing header member -- second value-change accumulator): the accelerated
// (input-type 3) path reads/writes a SECOND 37-slot f32 accumulator that sits
// immediately after maChangeBuffer (at this+0x72FC == &maChangeBuffer[37]). The
// joystick path drives this ramp; the summed ramp is what reaches the take. The
// span is NOT named in ICEController.hpp (the header jumps from maChangeBuffer @0x7268
// straight to miMenuTimer @0x73B8). It is accessed here by indexing one past the end
// of maChangeBuffer (byte-exact +0x72FC) rather than editing the header. SUGGESTED:
// add `f32 maRampAccum[KI_ICE_NUM_CHANGE_ELEMENTS];  // @0x72FC` after maChangeBuffer.
//
// FLAG (SetKeyElementValueChange second arg): the analog/accelerated paths set up
// only the element index and the float delta for the call; the change-token middle
// argument (liChange) is left as the incoming register and only matters on the
// state-16 key path inside the author method. liChangeIndex is passed for it here so
// the call is well-formed; the consumer ignores it outside state 16.
// ============================================================================

#include "SDKs/Packages/ICE/ICEController.hpp"
#include "SDKs/Packages/ICE/ICEAuthor.hpp"    // ICE::ICEAuthor base methods reached via `this`
#include "SDKs/Packages/ICE/ICEData.hpp"      // ICE::ICETake / ICEValue ops on mEditTake
#include "SDKs/Packages/ICE/ICEDataEnums.hpp" // ICE::ICEElementDescriptions[] (extern), ICEInputType
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace ICE
{

// ---------------------------------------------------------------------------
// RampElementVelocity (file-local helper for the accelerated input path)
//
// Advance one velocity-ramp slot toward a target driven by lfVelocity, then clamp to
// the per-pass envelope. With no drive (lfVelocity == 0) the ramp decays toward zero
// by mfIncDecDecel (flooring at zero, from either side). With drive it integrates
// mfIncDecAccel*lfVelocity into the ramp and clamps the result to [-mfIncDecSpeed,
// +mfIncDecSpeed]. Returns the new ramp value (also written back into the slot).
// Mirrors both symmetric branches of the input-type-3 maths.
// ---------------------------------------------------------------------------
static f32 RampElementVelocity(f32& lrRamp, f32 lfVelocity, const ICEElementDescription& lrDesc)
{
    const f32 lfSpeed = lrDesc.mfIncDecSpeed;   // description +0x44 (ramp clamp magnitude)
    const f32 lfAccel = lrDesc.mfIncDecAccel;   // description +0x48 (drive integrator gain)
    const f32 lfDecel = lrDesc.mfIncDecDecel;   // description +0x4C (idle decay step)

    f32 lfRamp = lrRamp;

    if (lfVelocity == 0.0f)
    {
        // Idle: decay toward zero by lfDecel from whichever side the ramp sits on.
        if (lfRamp >= 0.0f)
        {
            // Decrement a positive ramp by lfDecel, flooring at zero.
            lfRamp = (lfRamp <= 0.0f) ? 0.0f
                   : ((lfRamp - lfDecel) <= 0.0f ? 0.0f : (lfRamp - lfDecel));
        }
        else
        {
            // Increment a negative ramp toward zero by lfDecel, capping at zero.
            const f32 lfStepped = lfRamp + lfDecel;
            lfRamp = (lfStepped <= 0.0f) ? lfStepped : 0.0f;
        }
    }
    else
    {
        // Driven: integrate accel*velocity into the ramp, then clamp to +/- speed.
        lfRamp = (lfAccel * lfVelocity) + lfRamp;
        if (lfRamp < -lfSpeed) { lfRamp = -lfSpeed; }   // floor at -mfIncDecSpeed
        if (lfRamp >  lfSpeed) { lfRamp =  lfSpeed; }   // ceiling at +mfIncDecSpeed
    }

    lrRamp = lfRamp;
    return lfRamp;
}

// ---------------------------------------------------------------------------
// ChangeKeyElementValue
//
// Apply the accumulated joystick drive for one element description (liChangeIndex
// into ICEElementDescriptions[]) to the live edit take. The description's two
// inc/dec button indices (miIncButton @0x3C, miDecButton @0x40) select the pair of
// joystick-velocity slots in the change buffer that drive this element; mInputType
// (@0x38) selects the maths. liChangeIndex is the element index passed straight to
// the author key-element op / the take SetValue channel.
// ---------------------------------------------------------------------------
void ICEController::ChangeKeyElementValue(s32 liChangeIndex)
{
    const ICEElementDescription& lrDesc = ICEElementDescriptions[liChangeIndex];

    const s32 liVelA = lrDesc.miIncButton;   // description +0x3C (first velocity slot)
    const s32 liVelB = lrDesc.miDecButton;   // description +0x40 (second velocity slot)

    // The change buffer's f32 view holds the per-axis joystick velocities. The second
    // axis is taken negated (the dec axis opposes the inc axis).
    const f32 lfVelA =  JoyVelocity(liVelA);
    const f32 lfVelB = -JoyVelocity(liVelB);

    f32 lfDelta = 0.0f;

    switch (lrDesc.mInputType)
    {
        case eICE_INPUT_ANALOG:
        {
            // Direct velocity delta scaled by the current timestep, applied only when
            // the two opposing axes do not cancel.
            const f32 lfSum = lfVelB + JoyVelocity(liVelA);
            if (lfSum != 0.0f)
            {
                const f32 lfTimestep = *mpMenuB;   // *mpMenuB (this+0xF70 points at the timestep)
                lfDelta = (lrDesc.mfIncDecSpeed * lfTimestep) * lfSum;       // description +0x44
            }
            // FLAG (ICEAuthor base): this->SetKeyElementValueChange via the base cast.
            reinterpret_cast<ICE::ICEAuthor*>(this)->SetKeyElementValueChange(liChangeIndex, liChangeIndex, lfDelta);
            break;
        }

        case eICE_INPUT_ACCELERATED:
        {
            // Second 37-slot ramp accumulator immediately after maChangeBuffer
            // (this+0x72FC). FLAG (missing header member): see file header.
            f32* lpRamp = reinterpret_cast<f32*>(&maChangeBuffer[KI_ICE_NUM_CHANGE_ELEMENTS]);

            // Ramp each axis' slot from its joystick velocity, sum the two ramps, then
            // scale by the timestep. The second axis uses the negated velocity (lfVelB).
            const f32 lfRampA = RampElementVelocity(lpRamp[liVelA], lfVelA, lrDesc);
            const f32 lfRampB = RampElementVelocity(lpRamp[liVelB], lfVelB, lrDesc);

            const f32 lfTimestep = *mpMenuB;   // *mpMenuB (this+0xF70 points at the timestep)
            lfDelta = (lfRampB + lfRampA) * lfTimestep;

            // FLAG (ICEAuthor base): this->SetKeyElementValueChange via the base cast.
            reinterpret_cast<ICE::ICEAuthor*>(this)->SetKeyElementValueChange(liChangeIndex, liChangeIndex, lfDelta);
            break;
        }

        case eICE_INPUT_DISCREET:
        {
            // Stepped integer increment: take the absolute combined drive and require it
            // to clear a 0.1 dead-zone before stepping once per pass.
            const f32 lfSum = lfVelB + JoyVelocity(liVelA);
            const f32 lfMagnitude = (lfSum >= 0.0f) ? lfSum : -lfSum;
            if (lfMagnitude > 0.1f)
            {
                const s32 liStepSign = (lfSum > 0.0f) ? 1 : -1;

                // Resolve whether the current state edits the END key edge.
                bool lbEndKey;
                if (miState == 16)
                {
                    // FLAG (ICEAuthor base): this->IsEndKeyState via the base cast.
                    lbEndKey = reinterpret_cast<ICE::ICEAuthor*>(this)->IsEndKeyState(miStateArg);
                }
                else
                {
                    lbEndKey = (miState == 13 || miState == 15 || miState == 18);
                }

                // The interval whose key edge is edited is the current interval for the
                // channel, read from an author-base interval table (16-byte records at
                // this+0x100 indexed by channel; the field read is the leading u16).
                // FLAG (unmodelled author-base member): this per-channel interval table
                // is not named in any present header, so it is read by byte offset here
                // (this + 16*(miCurrentChannel+16)). Replace with the named accessor once
                // the author-base interval table lands.
                const u16 lu16Interval = *reinterpret_cast<const u16*>(
                    reinterpret_cast<const u8*>(this) + (16 * (miCurrentChannel + 16)));
                // FLAG (ICEAuthor base): this->GetIntervalKey via the base cast.
                const s32 liIntervalKey = reinterpret_cast<ICE::ICEAuthor*>(this)->GetIntervalKey(lu16Interval);

                const u16 lu16Key = (u16)(liIntervalKey + (lbEndKey ? 1 : 0));

                // Current stored int value at that key, plus one quant step in the drive
                // direction; quant step is the description's inc/dec base speed truncated.
                const s32 liCurrent = mEditTake.GetValueInt(liChangeIndex, lu16Key);
                s32 liValue = ((s32)lrDesc.mfIncDecSpeed * liStepSign) + liCurrent;

                // Clamp to the description's [mMin, mMax] read as raw int bits, gated on
                // mMax >= mMin (both read as int bits): lower-bound then upper-bound.
                const s32 liMin = lrDesc.mMin.GetSignedInt();   // description +0x18 (int view)
                const s32 liMax = lrDesc.mMax.GetSignedInt();   // description +0x1C (int view)
                if (liMax >= liMin)
                {
                    if (liMin > liValue) { liValue = liMin; }
                    if (liMax < liValue) { liValue = liMax; }
                }

                mEditTake.SetValue(liChangeIndex, lu16Key, ICEValue(liValue));
            }
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// ChangeKeyElementValues
//
// Driver over the 37 change slots: for each slot index, walk the editor's element
// region (the first KI_ICE_AUTHOR_ELEMENTS == 28 descriptions, the range whose end
// the loop bounds against) and, for every description whose channel matches the
// current channel and whose inc/dec button index equals the slot, apply that slot's
// accumulated change via ChangeKeyElementValue. (Each description binds one slot
// through either its inc or its dec button index.)
// ---------------------------------------------------------------------------
void ICEController::ChangeKeyElementValues()
{
    for (s32 liSlot = 0; liSlot < KI_ICE_NUM_CHANGE_ELEMENTS; ++liSlot)
    {
        for (s32 liElement = 0; liElement < KI_ICE_AUTHOR_ELEMENTS; ++liElement)
        {
            const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];

            if ((liSlot == lrDesc.miIncButton || liSlot == lrDesc.miDecButton) &&
                lrDesc.miChannelNumber == miCurrentChannel)
            {
                ChangeKeyElementValue(liElement);
            }
        }
    }
}

} // namespace ICE
