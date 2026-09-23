#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::TranslateFrame50HzTo60Hz / TranslateFrame60HzTo50Hz.
//
// A 16-bit frame counter from a console running the other simulation rate is placed on
// the local 16-bit ring and rescaled to the local rate. The frame and the local current
// frame are both extended by lu16NumWraps whole rings of KU16_WRAP_FRAMES; the frame is
// then moved one ring up or down when it lies more than half a ring from the current
// frame (a frame below one ring that must move down is mirrored instead and the result
// mirrored back). The rescale is integer and rounds to nearest: 50 -> 60 is
// (12 f + 5) / 10, 60 -> 50 is (5 f + 3) / 6, both modulo one ring.
//
// When the two frames are close, the result is cross-checked against the float
// reference (OLDTranslateFrame*) within a tolerance that grows with the wrap count.
// Kept apart from the mounted CgsMessage.cpp: mount it together with
// CgsOldFrameConversionFunctions.cpp.

namespace CgsNetwork
{
    namespace
    {
        const u64 KU16_WRAP_FRAMES      = 65535;
        const u64 KU16_HALF_WRAP_FRAMES = 32768;
        const s32 KI_DEFAULT_TOLERANCE  = 21845;   // KU16_DEFAULT_TOLERANCE
    }

    // The frame and the current frame, lu16NumWraps rings up, are summed in 32 bits and
    // sign-extended into the 64-bit working values.

    u16 TranslateFrame50HzTo60Hz(u16 lu16Frame50Hz, u16 lu16CurrentFrame50Hz, u16 lu16NumWraps)
    {
        u64 luEstimatedTime50Hz =
            static_cast<s32>(static_cast<u32>(lu16Frame50Hz) + static_cast<u32>(lu16NumWraps) * static_cast<u32>(KU16_WRAP_FRAMES));
        const u64 luCurrentTime50Hz =
            static_cast<s32>(static_cast<u32>(lu16CurrentFrame50Hz) + static_cast<u32>(lu16NumWraps) * static_cast<u32>(KU16_WRAP_FRAMES));

        bool lbMirrored = false;
        if (luEstimatedTime50Hz < luCurrentTime50Hz + KU16_HALF_WRAP_FRAMES)
        {
            if (!(luEstimatedTime50Hz + KU16_HALF_WRAP_FRAMES > luCurrentTime50Hz))
            {
                luEstimatedTime50Hz += KU16_WRAP_FRAMES;
            }
        }
        else if (luEstimatedTime50Hz < KU16_WRAP_FRAMES)
        {
            lbMirrored          = true;
            luEstimatedTime50Hz = KU16_WRAP_FRAMES - luEstimatedTime50Hz;
        }
        else
        {
            luEstimatedTime50Hz -= KU16_WRAP_FRAMES;
        }

        CGS_ASSERT((static_cast<u16>(luEstimatedTime50Hz - luCurrentTime50Hz) < KU16_HALF_WRAP_FRAMES)
                       || (static_cast<u16>(luCurrentTime50Hz - luEstimatedTime50Hz) < KU16_HALF_WRAP_FRAMES),
                   "((uint16_t) (luEstimatedTime50Hz - luCurrentTime50Hz) < KU16_HALF_WRAP_FRAMES) || "
                   "((uint16_t) (luCurrentTime50Hz - luEstimatedTime50Hz) < KU16_HALF_WRAP_FRAMES)");

        u16 lu16Frame60Hz = static_cast<u16>(((luEstimatedTime50Hz * 12 + 5) / 10) % KU16_WRAP_FRAMES);
        if (lbMirrored)
        {
            lu16Frame60Hz = static_cast<u16>(KU16_WRAP_FRAMES - lu16Frame60Hz);
        }

        s32 liFrameDistance = static_cast<s32>(lu16Frame50Hz) - static_cast<s32>(lu16CurrentFrame50Hz);
        if (liFrameDistance < 0)
        {
            liFrameDistance = -liFrameDistance;
        }

        if (liFrameDistance < KI_DEFAULT_TOLERANCE)
        {
            const u16 lu16OldFrame60Hz = OLDTranslateFrame50HzTo60Hz(lu16Frame50Hz, lu16CurrentFrame50Hz,
                                                                    lu16NumWraps);
            if (lu16NumWraps <= 1000)
            {
                if (lu16NumWraps > 100)
                {
                    CGS_ASSERT(lu16Frame60Hz == lu16OldFrame60Hz
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz - 1)
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz - 2)
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz + 1)
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz + 2),
                               "Translate50HzTo60Hz(,,,) failed. lu16Frame60Hz= lu16OldFrame60Hz=");
                }
                else if (lu16NumWraps > 7)
                {
                    CGS_ASSERT(lu16Frame60Hz == lu16OldFrame60Hz
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz - 1)
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz + 1),
                               "Translate50HzTo60Hz(,,,) failed lu16Frame60Hz= lu16OldFrame60Hz=");
                }
                else
                {
                    CGS_ASSERT(lu16Frame60Hz == lu16OldFrame60Hz,
                               "Translate50HzTo60Hz(,,,) failed lu16Frame60Hz= lu16OldFrame60Hz=");
                }
            }
        }

        return lu16Frame60Hz;
    }

    u16 TranslateFrame60HzTo50Hz(u16 lu16Frame60Hz, u16 lu16CurrentFrame60Hz, u16 lu16NumWraps)
    {
        u64 luEstimatedTime60Hz =
            static_cast<s32>(static_cast<u32>(lu16Frame60Hz) + static_cast<u32>(lu16NumWraps) * static_cast<u32>(KU16_WRAP_FRAMES));
        const u64 luCurrentTime60Hz =
            static_cast<s32>(static_cast<u32>(lu16CurrentFrame60Hz) + static_cast<u32>(lu16NumWraps) * static_cast<u32>(KU16_WRAP_FRAMES));

        bool lbMirrored = false;
        if (luEstimatedTime60Hz < luCurrentTime60Hz + KU16_HALF_WRAP_FRAMES)
        {
            if (!(luEstimatedTime60Hz + KU16_HALF_WRAP_FRAMES > luCurrentTime60Hz))
            {
                luEstimatedTime60Hz += KU16_WRAP_FRAMES;
            }
        }
        else if (luEstimatedTime60Hz < KU16_WRAP_FRAMES)
        {
            lbMirrored          = true;
            luEstimatedTime60Hz = KU16_WRAP_FRAMES - luEstimatedTime60Hz;
        }
        else
        {
            luEstimatedTime60Hz -= KU16_WRAP_FRAMES;
        }

        CGS_ASSERT((static_cast<u16>(luEstimatedTime60Hz - luCurrentTime60Hz) < KU16_HALF_WRAP_FRAMES)
                       || (static_cast<u16>(luCurrentTime60Hz - luEstimatedTime60Hz) < KU16_HALF_WRAP_FRAMES),
                   "((uint16_t) (luEstimatedTime60Hz - luCurrentTime60Hz) < KU16_HALF_WRAP_FRAMES) || "
                   "((uint16_t) (luCurrentTime60Hz - luEstimatedTime60Hz) < KU16_HALF_WRAP_FRAMES)");

        u16 lu16Frame50Hz = static_cast<u16>(((luEstimatedTime60Hz * 5 + 3) / 6) % KU16_WRAP_FRAMES);
        if (lbMirrored)
        {
            lu16Frame50Hz = static_cast<u16>(KU16_WRAP_FRAMES - lu16Frame50Hz);
        }

        s32 liFrameDistance = static_cast<s32>(lu16Frame60Hz) - static_cast<s32>(lu16CurrentFrame60Hz);
        if (liFrameDistance < 0)
        {
            liFrameDistance = -liFrameDistance;
        }

        if (liFrameDistance < KI_DEFAULT_TOLERANCE)
        {
            const u16 lu16OldFrame50Hz = OLDTranslateFrame60HzTo50Hz(lu16Frame60Hz, lu16CurrentFrame60Hz,
                                                                    lu16NumWraps);
            if (lu16NumWraps < 0x8000)
            {
                if (lu16NumWraps > 100)
                {
                    CGS_ASSERT(lu16Frame50Hz == lu16OldFrame50Hz
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 1)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 2)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 3)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 1)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 2)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 3),
                               "Translate60HzTo50Hz(,,) tol= new= old= failed");
                }
                else
                {
                    CGS_ASSERT(lu16Frame50Hz == lu16OldFrame50Hz
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 1)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 2)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 1)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 2),
                               "Translate60HzTo50Hz(,,) tol= new= old= failed");
                }
            }
        }

        return lu16Frame50Hz;
    }
}
