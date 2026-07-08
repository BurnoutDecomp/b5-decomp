#include "GameSource/Massive/BrnMassive.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // CgsDev::Log / Message / StrStreamBase

// ===========================================================================
// BrnMassiveSubscriber - Burnout's MassiveAd per-surface ad subscriber.
//
// One is inline-pooled inside BrnMassive per active in-world ad surface. It
// derives from the MassiveAd SDK subscriber (MassiveAdClient3::CMassiveAdObject-
// Subscriber) and implements the client's download / impression callbacks. Each
// callback logs its progress through the debug-print stream (gated on
// CgsDev::Message::gxMessageFilterFlags) and advances the subscriber's state
// machine at +0x90 (muState).
//
// Reconstructed from the X360 ARTIST.XEX (pseudocode + assembly). No Feb-2007
// source and no DecFIGS DWARF for this TU; every constant / offset / branch /
// rodata string below is taken verbatim from the assembly in the dossier.
// ===========================================================================

namespace BrnMassive
{

// The downloaded-texture format the subscriber accepts (X360 asm: cmplwi 0x23).
static const int KI_DXT1_FORMAT = 35;

// MassiveAd wraps a 52-byte header around the delivered texture payload; the
// real image size is the delivered size minus this header (X360 asm: addi -0x34).
static const int KI_MASSIVE_HEADER_SIZE = 52;

// ---------------------------------------------------------------------------
// BrnMassiveSubscriber @ 0x823AB6C8
//
// Construct the SDK subscriber and, when message logging is enabled, announce the
// new subscriber and its zone name. The X360 stores the derived vtable
// (off_820369E8) at +0x00; that store is reproduced automatically here by
// constructing the polymorphic object. The subscriber's own fields are left
// uninitialised - BrnMassive::CreateSubscriber sets them immediately after.
// ---------------------------------------------------------------------------
BrnMassiveSubscriber::BrnMassiveSubscriber(const char* lpcZoneName)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        CgsDev::StrStreamBase& lOut = *CgsDev::Log::gpDebugPrint;
        lOut << "Creating Massive Subscriber IE: ";
        if (lpcZoneName == nullptr)
        {
            lOut << "<NULLSTRING>";
        }
        else
        {
            lOut << lpcZoneName;
        }
    }
}

// ---------------------------------------------------------------------------
// MediaDownload @ 0x823AB778
//
// The MassiveAd client is starting to stream the ad texture for this subscriber.
// Log the advertisement id and the expected payload size, mark the subscriber as
// downloading (state 1) and reset its idle-frame counter. Always returns 1.
// ---------------------------------------------------------------------------
int BrnMassiveSubscriber::MediaDownload(int liAdvertId, int /*liArg3*/)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[MASSIVE] Downloading Massive Advertisement ID:" << liAdvertId << "\n";
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "[MASSIVE] Expected Size " << muField84 << "\n";
    }

    muState = 1;        // +0x90 : downloading
    muIdleFrames = 0;   // +0x88
    return 1;
}

// ---------------------------------------------------------------------------
// MediaDownloadComplete @ 0x823AB858
//
// The MassiveAd client has finished delivering the ad texture. Validate the
// delivery, and on success advance the subscriber to state 3 (ready) and cache
// the delivered ad's ids. Returns 1 on success, 0 on any validation failure:
//   * no delivered data                        (liDataValid == 0)
//   * wrong texture format                      (liFormat != DXT1)
//   * payload size != the expected size         (muField84)
// The X360 asserts the replacement texture pointer (+0x8C) is non-null first.
// ---------------------------------------------------------------------------
int BrnMassiveSubscriber::MediaDownloadComplete(int liDataValid, int liDataSize,
                                                int liFormat, int liAdvertId)
{
    const int liPayloadSize = liDataSize - KI_MASSIVE_HEADER_SIZE;

    CGS_ASSERT(muField8C != 0, "Invalid Pointer for Texture Data to be replaced by Massive");

    if (liDataValid == 0)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "[MASSIVE] Invalid Data Downloaded from Massive\n";
        }
        return 0;
    }

    if (liFormat != KI_DXT1_FORMAT)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "[MASSIVE] Downloaded Texture not in DXT1 Format\n";
        }
        return 0;
    }

    if (muField84 != static_cast<u32>(liPayloadSize))
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[MASSIVE] Invalid Data Size Delivered from Massive expected "
                << muField84 << " received " << liPayloadSize << "\n";
        }
        return 0;
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[MASSIVE] Download Complete for Massive Advertisement ID:" << liAdvertId << "\n";
    }

    muState = 3;                                                // +0x90 : ready
    muField7C = static_cast<u32>(GetInvElementID());           // +0x7C
    muField78 = static_cast<u32>(GetCrexID());                 // +0x78
    return 1;
}

// ---------------------------------------------------------------------------
// SetImpressionData @ 0x823AB9E0
//
// Latch a fresh impression descriptor into the block at +0x58, but only when the
// new descriptor supersedes the current one: either the new size is larger than
// the stored size (+0x68), or an impression is already pending (mbField94). Latching
// resets the impression accumulators (+0x58/+0x5C) and clears the pending flag.
// Returns `this` (the X360 leaves r3 = this on every path).
// ---------------------------------------------------------------------------
int BrnMassiveSubscriber::SetImpressionData(u8 luFlags, f32 lfValue, int /*liArg4*/,
                                            u32 luSize, u16 luWidth, u16 luHeight)
{
    if (muField68 < luSize || mbField94 != 0)
    {
        mfField6C = lfValue;        // +0x6C
        mbHasContent = luFlags;     // +0x60
        muField68 = luSize;         // +0x68
        muWidth = luWidth;          // +0x62
        muHeight = luHeight;        // +0x64
        muField5C = 0;              // +0x5C
        muField58 = 0;              // +0x58
        mbField94 = 0;              // +0x94
    }

    return reinterpret_cast<int>(this);
}

// ---------------------------------------------------------------------------
// Tick @ 0x823ABA20
//
// Per-frame: hand the impression-data block (+0x58) to the SDK to record a viewed
// impression, then flag that an impression is pending (mbField94). Returns the SDK
// SetImpression result.
// ---------------------------------------------------------------------------
int BrnMassiveSubscriber::Tick()
{
    int liResult = SetImpression(&muField58);   // &+0x58
    mbField94 = 1;                               // +0x94
    return liResult;
}

} // namespace BrnMassive
