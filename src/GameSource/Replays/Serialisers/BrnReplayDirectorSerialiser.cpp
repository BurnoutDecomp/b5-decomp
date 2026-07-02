#include "GameSource/Replays/Serialisers/BrnReplayDirectorSerialiser.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Director/Camera/Camera.h"   // BrnDirector::Camera::Camera (the static-layout image)

// BrnReplays::DirectorSerialiser -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 of the TU's 3 ledger functions; GetStaticLayout is inline in
// the header, matching the original's h:107 placement):
//   DirectorSerialiser::Read  @0x82650340   DirectorSerialiser::Write @0x82650438
//
// Both X360 bodies leave Unlock's result in r3 (an incidental bool); the
// originals are modelled void.

namespace BrnReplays
{

// @ 0x82650340 -- cpp:38. Lock, resolve the static camera, then the mode ladder:
// the STALLED pair (3/6), the PREPARING pair (1/4) and RESTORING (7) are explicit
// no-ops; RECORDING (2) appends the whole camera image to the record stream and
// PLAYING (5) restores it. Always Unlocks.
void DirectorSerialiser::Read()
{
    BaseSerialiser::Lock();

    BrnDirector::Camera::Camera* lpStatic = GetStaticLayout();
    CGS_ASSERT(lpStatic != 0, "lpStatic");   // :38 (non-gating)

    const EMode leMode = GetMode();
    bool lbSkip = (leMode == E_MODE_RECORDING_STALLED || leMode == E_MODE_PLAYING_STALLED);
    if (!lbSkip)
    {
        lbSkip = (leMode == E_MODE_RECORDING_PREPARING || leMode == E_MODE_PLAYING_PREPARING);
        if (!lbSkip && leMode != E_MODE_RESTORING)
        {
            if (leMode == E_MODE_RECORDING)
                BaseSerialiser::Write(lpStatic, KI_STATIC_LAYOUT_SIZE);
            else if (leMode == E_MODE_PLAYING)
                BaseSerialiser::Read(lpStatic, KI_STATIC_LAYOUT_SIZE);
        }
    }

    BaseSerialiser::Unlock();
}

// @ 0x82650438 -- cpp:69. Lock, resolve the static camera, construct it in place
// exactly once (the +0x5C latch; note the X360 runs the construct block BEFORE
// its lpStatic NULL guard -- only the serialise dispatch below is guarded, and
// that order is kept), then RECORDING appends / PLAYING restores the camera
// image. Always Unlocks.
void DirectorSerialiser::Write()
{
    BaseSerialiser::Lock();

    BrnDirector::Camera::Camera* lpStatic = GetStaticLayout();
    CGS_ASSERT(lpStatic != 0, "lpStatic");   // :69 (non-gating)

    if (!mbStaticCameraConstructed)
    {
        lpStatic->Construct();
        mbStaticCameraConstructed = true;
    }

    if (lpStatic != 0)
    {
        const EMode leMode = GetMode();
        if (leMode == E_MODE_RECORDING)
            BaseSerialiser::Write(lpStatic, KI_STATIC_LAYOUT_SIZE);
        else if (leMode == E_MODE_PLAYING)
            BaseSerialiser::Read(lpStatic, KI_STATIC_LAYOUT_SIZE);
    }

    BaseSerialiser::Unlock();
}

}
