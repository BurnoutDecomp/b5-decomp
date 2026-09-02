#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::EffectsSerialiser)
//
//   Construct       @ 0x8264C9D8:
//       return BrnReplays::BaseSerialiser::Construct(
//                  this, 8, 0, 4816, 4784, "Effects", 0);
//   GetStaticLayout @ 0x82278698:
//       asserts miStaticBufferSize >= 4784, then returns mpStaticBuffer.
//   Read            @ 0x82650508:
//       Lock; fetch + assert the static layout; on RECORDING write the static
//       buffer out, on PLAYING read it in (4784 bytes); Unlock.
//   Write           @ 0x82650600:
//       Lock; if the static layout exists, write it out (4784 bytes) on
//       RECORDING or read it in (4784 bytes) on PLAYING; Unlock. (r4/r5 at the
//       Write/Read call sites still hold the static layout ptr + 4784 from the
//       GetStaticLayout call above -- NOT the no-arg overloads.)
//
// EffectsSerialiser is a leaf replay serialiser deriving from the shared
// BaseSerialiser: Construct parameterises BaseSerialiser::Construct with this
// stream's id (8), buffer size (4816), static size (4784) and channel name
// ("Effects"); the rest forward into the shared lock/transfer machinery.
//
// 2026-09-02 (tyre-mark wave): this TU used to carry its OWN 3-member
// `struct BaseSerialiser { meMode; mpStaticBuffer; miStaticBufferSize; }` and a
// private EffectsSerialiser over it -- a fork of the real BrnReplayBaseSerialiser.h
// layout (mpStaticBuffer @+0x18 there, @+0x04 here). The bodies now run on the real
// class the header declares; nothing else changed.
namespace BrnReplays
{
    s32 EffectsSerialiser::Construct()
    {
        return BaseSerialiser::Construct(8, 0, 4816, 4784, "Effects", 0);
    }

    EffectsSerialiserStaticLayout* EffectsSerialiser::GetStaticLayout()
    {
        CGS_ASSERT(miStaticBufferSize >= EffectsSerialiserStaticLayout::KI_STATIC_LAYOUT_SIZE,
                   "Static buffer size is too small\n");
        return reinterpret_cast<EffectsSerialiserStaticLayout*>(mpStaticBuffer);
    }

    s32 EffectsSerialiser::Read()
    {
        BaseSerialiser::Lock();
        EffectsSerialiserStaticLayout* lpStatic = GetStaticLayout();
        CGS_ASSERT(lpStatic != nullptr, "lpStatic");
        switch (meMode)
        {
        case E_MODE_RECORDING:
            BaseSerialiser::Write(lpStatic, EffectsSerialiserStaticLayout::KI_STATIC_LAYOUT_SIZE);
            break;
        case E_MODE_PLAYING:
            BaseSerialiser::Read(lpStatic, EffectsSerialiserStaticLayout::KI_STATIC_LAYOUT_SIZE);
            break;
        default:
            break;
        }
        return BaseSerialiser::Unlock() ? 1 : 0;
    }

    s32 EffectsSerialiser::Write()
    {
        BaseSerialiser::Lock();
        EffectsSerialiserStaticLayout* lpStatic = GetStaticLayout();
        if (lpStatic)
        {
            switch (meMode)
            {
            case E_MODE_RECORDING:
                BaseSerialiser::Write(lpStatic, EffectsSerialiserStaticLayout::KI_STATIC_LAYOUT_SIZE);
                break;
            case E_MODE_PLAYING:
                BaseSerialiser::Read(lpStatic, EffectsSerialiserStaticLayout::KI_STATIC_LAYOUT_SIZE);
                break;
            default:
                break;
            }
        }
        return BaseSerialiser::Unlock() ? 1 : 0;
    }
}
