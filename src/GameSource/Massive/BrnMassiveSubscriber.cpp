#include "GameSource/Massive/BrnMassive.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // CgsDev::Log / Message / StrStreamBase

// ===========================================================================
// BrnMassiveSubscriber - Burnout's MassiveAd per-surface ad subscriber.
//
// One is inline-pooled inside BrnMassive per active in-world advert. It derives from
// the MassiveAd SDK subscriber and implements the client's three download callbacks,
// logging progress through the debug-print stream and advancing the download state
// (miState: 0 idle, 1 downloading, 2 timed out, 3 ready). Reconstructed from the
// console code (no debug-info declaration exists for this TU).
// ===========================================================================

namespace BrnMassive
{

static_assert(sizeof(BrnMassiveSubscriber::ImpressionData) == 32,
              "the impression record SetImpression copies is 32 bytes");

// The texture format the replacement ad must arrive in (DXT1).
static const unsigned int KU_DXT1_MEDIA_TYPE = 35;

// MassiveAd wraps a 52-byte header around the delivered texture payload.
static const int KI_MASSIVE_HEADER_SIZE = 52;

// ---------------------------------------------------------------------------
// Construct the SDK subscriber for the zone and announce it.
// ---------------------------------------------------------------------------
BrnMassiveSubscriber::BrnMassiveSubscriber(const char* lpcZoneName)
    : MassiveAdClient3::CMassiveAdObjectSubscriber(lpcZoneName)
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
// MediaDownload: the client has started streaming this subscriber's ad. Log it, mark
// the subscriber downloading and restart its idle counter.
// ---------------------------------------------------------------------------
int BrnMassiveSubscriber::MediaDownload(int liAdvertId)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[MASSIVE] Downloading Massive Advertisement ID:" << static_cast<u32>(liAdvertId) << "\n";
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "[MASSIVE] Expected Size " << muExpectedSize << "\n";
    }

    miState      = 1;
    muIdleFrames = 0;
    return 1;
}

// ---------------------------------------------------------------------------
// MediaDownloadComplete: validate the delivered ad (present, DXT1, payload exactly the
// replacement texture's size) and, on success, mark it ready and cache its ids.
// Returns 1 on success, 0 on any validation failure.
// ---------------------------------------------------------------------------
int BrnMassiveSubscriber::MediaDownloadComplete(const void* lpData, int liDataSize,
                                                unsigned int luMediaType, int liAdvertId)
{
    const u32 luPayloadSize = static_cast<u32>(liDataSize - KI_MASSIVE_HEADER_SIZE);

    CGS_ASSERT(mpTextureData != nullptr, "Invalid Pointer for Texture Data to be replaced by Massive");

    if (lpData == nullptr)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "[MASSIVE] Invalid Data Downloaded from Massive\n";
        }
        return 0;
    }

    if (luMediaType != KU_DXT1_MEDIA_TYPE)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "[MASSIVE] Downloaded Texture not in DXT1 Format\n";
        }
        return 0;
    }

    if (muExpectedSize != luPayloadSize)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[MASSIVE] Invalid Data Size Delivered from Massive expected "
                << muExpectedSize << " received " << luPayloadSize << "\n";
        }
        return 0;
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[MASSIVE] Download Complete for Massive Advertisement ID:" << static_cast<u32>(liAdvertId) << "\n";
    }

    miState        = 3;
    miInvElementID = GetInvElementID();
    muCrexID       = static_cast<u32>(GetCrexID());
    return 1;
}

// ---------------------------------------------------------------------------
// SetImpressionData: latch a new impression description when the stored one is
// superseded -- the new screen size is larger, or the stored one has already been
// handed to the SDK by Tick. Latching restarts the accumulators.
// ---------------------------------------------------------------------------
void BrnMassiveSubscriber::SetImpressionData(bool lbInView, f32 lfAngle, u32 luScreenSize,
                                             u16 luScreenWidth, u16 luScreenHeight)
{
    if (mImpression.muScreenSize < luScreenSize || mbImpressionReported)
    {
        mImpression.mfAngle        = lfAngle;
        mImpression.mbInView       = lbInView;
        mImpression.muScreenSize   = luScreenSize;
        mImpression.muScreenWidth  = luScreenWidth;
        mImpression.muScreenHeight = luScreenHeight;
        mImpression.muAccumulator1 = 0;
        mImpression.muAccumulator0 = 0;
        mbImpressionReported       = false;
    }
}

// ---------------------------------------------------------------------------
// Tick: hand the impression record to the SDK and mark it reported.
// ---------------------------------------------------------------------------
void BrnMassiveSubscriber::Tick()
{
    SetImpression(&mImpression);
    mbImpressionReported = true;
}

} // namespace BrnMassive
