#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::GuiModuleSerialiser)
//
//   Construct       @ 0x8264CFA8:
//       result = BrnReplays::BaseSerialiser::Construct(
//                    this, 10, 0, 2048, 992, "GuiModule", 0);
//       *(this + 92) = 0;   // clears this serialiser's own reset flag
//       return result;
//   GetStaticLayout @ 0x82410D90:
//       asserts miStaticBufferSize >= 992, then returns mpStaticBuffer
//       (reinterpreted as the GuiModule static layout).
//   Read            @ 0x82650748:
//       Lock; the X360 mode cascade reduces to PLAYING-only — read the static
//       layout in (992 bytes); Unlock.
//   Write           @ 0x82650678:
//       Lock; for the RECORDING family (PREPARING/RECORDING/STALLED) reset the
//       static layout once on first use, then write it out (992 bytes); Unlock.
//
// GuiModuleSerialiser is a leaf replay serialiser deriving from the shared
// BaseSerialiser: Construct parameterises BaseSerialiser::Construct with this
// stream's id (10), buffer size (2048), static size (992) and channel name
// ("GuiModule"), and additionally clears this serialiser's own one-shot
// mbStaticLayoutReset flag. BaseSerialiser is reconstructed by its own TU; here
// a semantic-parity slice is declared for the compile-only gate (named members
// only, NOT byte-exact — the full layout + bodies live in BaseSerialiser's TU).
// GuiModuleStaticLayout is likewise a compile-only slice deferred to its own TU.

namespace BrnReplays
{
    struct BaseSerialiser
    {
        // DWARF: BrnReplayBaseSerialiser.h — full mode enum.
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

        // Only the members GuiModuleSerialiser actually touches are modelled
        // here. BaseSerialiser owns several more (mbLocked, mpBuffer,
        // miBufferSize, macName, mfTime, ...); deferred to its own TU.
        EMode meMode;
        void* mpStaticBuffer;
        s32   miStaticBufferSize;

        // Declare-only: defined in BaseSerialiser's TU. (Shapes must match the
        // canonical BrnReplayBaseSerialiser.h declarations exactly -- MSVC mangles
        // return types, so Lock/Unlock are bool here, matching the committed
        // bodies.)
        int  Construct(int nId, int nMode, int nBufferSize, int nStaticSize,
                       const char* pName, int nFlag);
        bool Lock();
        bool Unlock();

        // Sized transfer overloads (both Read() and Write() use these here).
        int Write(const void* lpData, int nSize);
        int Read(void* lpData, int nSize);
    };

    // Compile-only slice: the GuiModule replay static layout. Reset() is
    // declare-only here, bodied in BrnReplayGuiModuleStaticLayout.cpp -- which
    // spells it `GuiModuleStaticLayout* Reset()` (the X360 returns the layout as
    // a tail artefact); the shape must match exactly (MSVC mangles return types).
    struct GuiModuleStaticLayout
    {
        GuiModuleStaticLayout* Reset();
    };

    struct GuiModuleSerialiser : BaseSerialiser
    {
        int                    Construct();
        GuiModuleStaticLayout* GetStaticLayout();
        int                    Read();
        int                    Write();

        // This serialiser's own one-shot flag (X360 +92): the static layout is
        // Reset() exactly once, on the first recording Write(). Named-member
        // parity, NOT byte-exact at +92.
        bool mbStaticLayoutReset;
    };

    int GuiModuleSerialiser::Construct()
    {
        int liResult = BaseSerialiser::Construct(10, 0, 2048, 992, "GuiModule", 0);
        mbStaticLayoutReset = false;
        return liResult;
    }

    GuiModuleStaticLayout* GuiModuleSerialiser::GetStaticLayout()
    {
        CGS_ASSERT(miStaticBufferSize >= 992, "Static buffer size is too small\n");
        return reinterpret_cast<GuiModuleStaticLayout*>(mpStaticBuffer);
    }

    int GuiModuleSerialiser::Read()
    {
        BaseSerialiser::Lock();

        // X360 mode cascade (outer {4,5,6}, skipping {1,3,4,6,7}) reduces to
        // PLAYING-only: read the static layout in during playback.
        if (meMode == E_MODE_PLAYING)
        {
            GuiModuleStaticLayout* lpStaticLayout = GetStaticLayout();
            CGS_ASSERT(lpStaticLayout != nullptr, "lpStaticLayout");
            BaseSerialiser::Read(lpStaticLayout, 992);
        }

        return BaseSerialiser::Unlock();
    }

    int GuiModuleSerialiser::Write()
    {
        BaseSerialiser::Lock();

        // X360 mode gate reduces to the RECORDING family: write the static
        // layout out during recording (preparing / recording / stalled).
        if (meMode == E_MODE_RECORDING_PREPARING ||
            meMode == E_MODE_RECORDING ||
            meMode == E_MODE_RECORDING_STALLED)
        {
            // Reset the static layout exactly once, on first use.
            if (!mbStaticLayoutReset)
            {
                mbStaticLayoutReset = true;
                GetStaticLayout()->Reset();
            }

            // The X360 fetches the layout a second time after the reset branch.
            GuiModuleStaticLayout* lpStaticLayout = GetStaticLayout();
            CGS_ASSERT(lpStaticLayout != nullptr, "lpStaticLayout");
            BaseSerialiser::Write(lpStaticLayout, 992);
        }

        return BaseSerialiser::Unlock();
    }
}
