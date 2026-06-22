#include "GameSource/Sound/Collision/BrnCollisionFrameInformation.h"

// =============================================================================
// BrnSound::Logic::FrameInformation -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnCollisionFrameInformation.h
// for the layout rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly two entries:
//   IsPaused           @ 0x826958E8
//   UpdateFatalityFlag @ 0x826ADD18
//
// dep_flags: none un-homed for THIS TU. All members touched (meFatality,
// mu32FatalStartCount, maPaused) are modelled BY NAME in the owning header.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// FrameInformation::IsPaused  @ 0x826958E8
//
//   v1 = 0;
//   for ( i = this + 80; !*(i + 4); i += 8 )   ; this+80 == &maPaused storage
//     if ( ++v1 ) return 0;                     ; first cleared field -> not paused
//   return 1;
//
// The X360 walks the maPaused bit-field storage and returns paused (1) only when
// the first checked field is non-zero, otherwise not-paused (0). Semantically this
// is "is any pause source active": IsPaused() == !maPaused.IsZero(). The committed
// CgsContainers::BitArray<3> stores its three flags in a single u64 field, so the
// IsZero() query is the faithful reconstruction of the X360 "first non-zero field"
// test.
// FLAG (X360 stride detail): the X360 loop strides by 8 reading the high word of
// each bit-field; against the committed single-u64 BitArray<3> that reduces to the
// IsZero() test below. No behaviour is fabricated -- "paused iff a pause flag is
// set" is the observable semantics.
// ---------------------------------------------------------------------------
bool FrameInformation::IsPaused() const
{
    return !maPaused.IsZero();
}

// ---------------------------------------------------------------------------
// FrameInformation::UpdateFatalityFlag  @ 0x826ADD18
//
// Drives the crash "fatality" state machine from the per-frame crash-input flag.
// The X360 reads meFatality.mCurrentValue (result[16]) and rewrites the DataPoint
// as a paired (cur, prev) store -- i.e. meFatality.Update(newState):
//
//   cur = meFatality.GetCurrent();
//   if ( cur != E_FATAL_OFF )
//   {
//     if ( cur == E_FATAL_ON )            { if ( crashInput ) return; }
//     else                                 // cur == E_FATAL_START (>=3 is a guard)
//     {
//       if ( cur >= 3 ) return;
//       if ( crashInput )
//       {
//         if ( --mu32FatalStartCount == 0 )
//           meFatality.Update(E_FATAL_ON);
//         return;
//       }
//     }
//     meFatality.Update(E_FATAL_OFF);      // crash-input dropped -> fatality off
//     return;
//   }
//   if ( crashInput )                      // OFF -> arm the start window
//   {
//     meFatality.Update(E_FATAL_START);
//     mu32FatalStartCount = KU32_FATAL_START_FRAME_COUNT;
//   }
//
// The asm `result[16]=X; result[17]=old` paired stores are exactly the DataPoint
// Update semantics (new current, displaced previous).
// ---------------------------------------------------------------------------
void FrameInformation::UpdateFatalityFlag(bool lbCrashInput)
{
    const EFatalityFlag leCurrent = meFatality.GetCurrent();

    if (leCurrent != E_FATAL_OFF)
    {
        if (leCurrent == E_FATAL_ON)
        {
            if (lbCrashInput)
            {
                return; // still crashing -> stay ON
            }
        }
        else
        {
            // leCurrent == E_FATAL_START (the X360 `>= 3` is an out-of-range guard).
            if (leCurrent >= E_FATAL_OFF + 3)
            {
                return;
            }

            if (lbCrashInput)
            {
                if (--mu32FatalStartCount == 0u)
                {
                    meFatality.Update(E_FATAL_ON); // start window elapsed -> commit ON
                }
                return;
            }
        }

        // Crash input dropped (or ON with no input) -> fatality off.
        meFatality.Update(E_FATAL_OFF);
        return;
    }

    // Currently OFF: a crash input arms the start window.
    if (lbCrashInput)
    {
        meFatality.Update(E_FATAL_START);
        mu32FatalStartCount = KU32_FATAL_START_FRAME_COUNT;
    }
}

} // namespace Logic
} // namespace BrnSound
