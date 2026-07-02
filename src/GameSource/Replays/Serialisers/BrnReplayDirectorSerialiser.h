#ifndef GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYDIRECTORSERIALISER_H
#define GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYDIRECTORSERIALISER_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT (GetStaticLayout's inline tripwires)
#include "GameSource/Replays/BrnReplayBaseSerialiser.h" // BrnReplays::BaseSerialiser

// ============================================================================
// GameSource/Replays/Serialisers/BrnReplayDirectorSerialiser.h
//
// BrnReplays::DirectorSerialiser -- the replay serialiser channel for the
// director: its 384-byte static buffer IS a BrnDirector::Camera::Camera (the
// replay camera image; Write constructs it in place once, then each frame the
// whole camera is appended to the record stream while RECORDING and restored
// from the playback stream while PLAYING). Reconstructed from
// BURNOUT_X360_ARTIST.XEX; the asserts name the original files
// (BrnReplayDirectorSerialiser.h:107/:108 -- GetStaticLayout is header-inline
// -- and BrnReplayDirectorSerialiser.cpp:38/:69).
//
// Bodied by this TU (3 ledger functions, class:BrnReplays::DirectorSerialiser):
//   GetStaticLayout @0x821F5C58 (inline here; the X360 exports one copy)
//   Read            @0x82650340   Write @0x82650438   (BrnReplayDirectorSerialiser.cpp)
// The rest of the surface (Construct etc.) is un-attested by this TU's export
// set and not declared.
// ============================================================================

namespace BrnDirector { namespace Camera { class Camera; } }

namespace BrnReplays
{
    class DirectorSerialiser : public BaseSerialiser
    {
    public:
        // The X360 static-layout byte size (0x180 == the X360 sizeof of the
        // Camera::Camera image the buffer holds). FLAG: the literal is the
        // SERIALISED size (the asm's li r5,0x180 + the :107 bound); the 64-bit
        // host sizeof(Camera::Camera) may drift from it -- the replay stream
        // format is pinned by this constant, not by the host sizeof.
        static const s32 KI_STATIC_LAYOUT_SIZE = 384;

        // @0x821F5C58 (this TU; header-inline in the original, h:107/:108). The
        // serialiser's static buffer viewed as the replay camera, behind the two
        // inline tripwires (the :107 text is streamed through a stack StrStream
        // into the assert message buffer on the X360; folded static here).
        BrnDirector::Camera::Camera* GetStaticLayout()
        {
            CGS_ASSERT(GetStaticBufferSize() >= KI_STATIC_LAYOUT_SIZE,
                       "Static buffer size is too small\n");                 // :107 (non-gating)
            CGS_ASSERT(GetStaticBuffer() != 0, "GetStaticBuffer()");         // :108 (non-gating)
            return static_cast<BrnDirector::Camera::Camera*>(GetStaticBuffer());
        }

        // @0x82650340 (this TU, cpp:38) -- the per-frame stream step: while
        // RECORDING append the whole camera image to the record buffer; while
        // PLAYING restore it from the playback buffer. The PREPARING / STALLED /
        // RESTORING modes are explicit no-ops (the asm's guard ladder).
        void Read();

        // @0x82650438 (this TU, cpp:69) -- as Read, but first construct the
        // camera in the static buffer exactly once (the mbStaticCameraConstructed
        // latch); the mode dispatch then only acts on RECORDING / PLAYING.
        void Write();

    private:
        // X360 +0x5C (right after the base's mbAllowStreaming @0x5A): raised by
        // Write's one-time in-place Camera::Construct on the static buffer.
        bool mbStaticCameraConstructed;
    };
}

#endif // GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYDIRECTORSERIALISER_H
