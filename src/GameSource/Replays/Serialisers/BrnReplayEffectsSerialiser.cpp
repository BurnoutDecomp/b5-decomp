#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

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
//       Lock; if the static layout exists, dispatch the no-arg Write()/Read()
//       overloads on RECORDING/PLAYING; Unlock.
//
// EffectsSerialiser is a leaf replay serialiser deriving from the shared
// BaseSerialiser: Construct parameterises BaseSerialiser::Construct with this
// stream's id (8), buffer size (4816), static size (4784) and channel name
// ("Effects"); the rest forward into the shared lock/transfer machinery.
// BaseSerialiser is reconstructed by its own TU; here a semantic-parity slice is
// declared for the compile-only gate (named members only, NOT byte-exact — the
// full layout + bodies live in BaseSerialiser's TU).

namespace BrnReplays
{
    struct BaseSerialiser
    {
        // DWARF: BrnReplayBaseSerialiser.h:54 — full mode enum.
        enum EMode
        {
            E_MODE_IDLE                = 0,
            E_MODE_RECORDING_PREPARING = 1,
            E_MODE_RECORDING           = 2,
            E_MODE_RECORDING_STALLED   = 3,
            E_MODE_PLAYING_PREPARING   = 4,
            E_MODE_PLAYING             = 5,
            E_MODE_PLAYING_STALLED     = 6,
            E_MODE_RESTORING           = 7,
            E_MODE_COUNT               = 8
        };

        // Only the members EffectsSerialiser actually touches are modelled here.
        // BaseSerialiser owns several more (mbLocked, mpBuffer, miBufferSize,
        // macName, mfTime, ...); deferred to its own TU.
        EMode meMode;
        void* mpStaticBuffer;
        s32   miStaticBufferSize;

        // Declare-only: defined in BaseSerialiser's TU.
        int Construct(int nId, int nMode, int nBufferSize, int nStaticSize,
                      const char* pName, int nFlag);
        void Lock();
        int  Unlock();

        // Sized transfer overloads (used by Read()).
        int Write(const void* lpData, int nSize);
        int Read(void* lpData, int nSize);

        // No-arg transfer overloads (used by Write()).
        int Write();
        int Read();
    };

    struct EffectsSerialiser : BaseSerialiser
    {
        int   Construct();
        void* GetStaticLayout();
        int   Read();
        int   Write();
    };

    int EffectsSerialiser::Construct()
    {
        return BaseSerialiser::Construct(8, 0, 4816, 4784, "Effects", 0);
    }

    void* EffectsSerialiser::GetStaticLayout()
    {
        CGS_ASSERT(miStaticBufferSize >= 4784, "Static buffer size is too small\n");
        return mpStaticBuffer;
    }

    int EffectsSerialiser::Read()
    {
        BaseSerialiser::Lock();

        void* lpStatic = GetStaticLayout();
        CGS_ASSERT(lpStatic != nullptr, "lpStatic");

        switch (meMode)
        {
        case E_MODE_RECORDING:
            BaseSerialiser::Write(lpStatic, 4784);
            break;
        case E_MODE_PLAYING:
            BaseSerialiser::Read(lpStatic, 4784);
            break;
        default:
            break;
        }

        return BaseSerialiser::Unlock();
    }

    int EffectsSerialiser::Write()
    {
        BaseSerialiser::Lock();

        if (GetStaticLayout())
        {
            switch (meMode)
            {
            case E_MODE_RECORDING:
                BaseSerialiser::Write();
                break;
            case E_MODE_PLAYING:
                BaseSerialiser::Read();
                break;
            default:
                break;
            }
        }

        return BaseSerialiser::Unlock();
    }
}
