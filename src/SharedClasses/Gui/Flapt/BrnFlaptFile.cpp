#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnFlapt::MovieClip member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU (class:BrnFlapt::MovieClip) bodies the two X360-emitted functions:
//
//   GetKeyframeForFrame    @ 0x8246B098
//   FindLabelledFrameIndex @ 0x8246B190
//
// Both originally streamed a dynamic failure message into the assert buffer via
// CgsDev::StrStreamBase (the "Tried to get frame N when num frames=M" / "Tried to
// find missing frame labelled 0xID, string=..." text). Under the project's house
// CGS_ASSERT the message is forwarded as a single static string and the streamed
// construction is semantically vacuous (the assert is advisory and only runs on
// failure); the observable behaviour — the value returned on the success path — is
// unchanged. The X360-baked BrnFlaptFile.h file/line cites are discarded per
// project convention.

namespace BrnFlapt
{

// ---- GetKeyframeForFrame @ 0x8246B098 ------------------------------------
// Bound-check the requested frame against mu16NumFrames (lhz 8(this)), then map it
// through the optional keyframe-remap table at +0x10: if present, return the u16
// entry mpau16KeyframeRemap[luFrame] (lhzx, zero-extended); otherwise the frame
// index passes through unchanged.
u32 MovieClip::GetKeyframeForFrame(u32 luFrame) const
{
    CGS_ASSERT(luFrame < mu16NumFrames,
        "luFrame < muNumFrames");

    if (mpau16KeyframeRemap != 0)
    {
        return mpau16KeyframeRemap[luFrame];
    }
    return luFrame;
}

// ---- FindLabelledFrameIndex @ 0x8246B190 ---------------------------------
// Linear-search the label table at +0x38 (8-byte MovieClipLabel entries) for an
// entry whose id (first dword) equals luLabelId, scanning up to mu8NumLabels
// (lbz 5(this)) entries. On a hit, return the matching frame id from the parallel
// u16 table mpau16LabelledFrameIds at +0x3C (lhzx, zero-extended). On miss — or if
// there are no labels at all — assert and return -1.
//
// The X360 re-reads mu8NumLabels (lbz 5(this)) on every loop iteration and asserts
// each visited entry pointer is non-null; reproduced as the natural index scan.
s32 MovieClip::FindLabelledFrameIndex(u32 luLabelId, const char* lpcLabelText) const
{
    (void)lpcLabelText;   // only consumed by the (advisory) failure message

    if (mu8NumLabels != 0)
    {
        u32 luIndex = 0;
        while (true)
        {
            CGS_ASSERT(&mpaLabels[luIndex] != 0, "lpLabel");

            if (mpaLabels[luIndex].muLabelId == luLabelId)
            {
                CGS_ASSERT(mpau16LabelledFrameIds != 0, "mpauLabelledFrameIds");
                return mpau16LabelledFrameIds[luIndex];
            }

            ++luIndex;
            if (luIndex >= mu8NumLabels)
            {
                break;
            }
        }
    }

    CGS_ASSERT(false,
        "Tried to find missing frame labelled ");
    return -1;
}

}
