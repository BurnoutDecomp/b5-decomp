#include "SDKs/Packages/MassiveAd/MassiveAdClient3Subscriber.h"

#include <cstddef>  // std::size_t
#include <cstring>  // strlen, strncpy, memset, memcpy

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"           // MassiveMalloc, MassiveLog, MassiveFormatString, file hooks
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h" // CMassiveClientCore, CMassiveZoneManager
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ZoneManager.h"// CMassiveZoneManager::MAOFind / PreSubscriberAdd
#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"   // CMassiveAdObject::SubscriberAdd / GetCrexID / mnInvElementID

// ===========================================================================
// MassiveAdClient3::CMassiveAdObjectSubscriber definition home.
//
// SHAPE and BODIES are both reconstructed from BURNOUT_X360_ARTIST.XEX (no leak
// source / DecFIGS). Stores/branches are reproduced against the X360 disassembly;
// see MassiveAdClient3Subscriber.h for the per-offset layout map. The class
// installs its own X360 vftable (off_8218478C) over the object -- modelled by the
// virtual destructor, so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAdObjectSubscriber::CMassiveAdObjectSubscriber @ 0x82BCE968
//
// Store order (all over the fresh object, vftable installed by the virtual dtor):
//   mpAdObject = 0; mnField10 = 0; mnField54 = 0; mpcName = 0; mnLastError = 0.
// Then the guarded attach path, each early-out recording a negative error code:
//   no client core            -> -200
//   null / < 2-char name      -> -500
//   name allocation failure   -> -99  (+ "ALLOCATION Failed..." log)
//   no current zone           -> -191
// On success it copies the name, clears the live impression, finds the ad object
// in the current zone and attaches (SubscriberAdd) or queues (PreSubscriberAdd),
// then logs "*ADDED MAO SUB: <name>". mnLastError stays 0.
// ---------------------------------------------------------------------------
CMassiveAdObjectSubscriber::CMassiveAdObjectSubscriber(const char* pcName)
    : mpcName(0)        // a1[1]  = 0  (+0x04)
    , mpAdObject(0)     // a1[2]  = 0  (+0x08)
    , mnLastError(0)    // a1[3]  = 0  (+0x0C)
    , mnField10(0)      // a1[4]  = 0  (+0x10)
    , mnField54(0)      // a1[21] = 0  (+0x54)
{
    if (!CMassiveClientCore::Instance())
    {
        mnLastError = -200;
        return;
    }

    if (!pcName || std::strlen(pcName) < 2)
    {
        mnLastError = -500;
        return;
    }

    std::size_t luNameSize = std::strlen(pcName) + 1;
    mpcName = static_cast<char*>(MassiveMalloc(luNameSize));  // stored before the null-check
    if (!mpcName)
    {
        mnLastError = -99;
        MassiveLog(2, "CMassiveAdObjectSubscriber", "ALLOCATION Failed for Subscriber name.");
        return;
    }
    std::strncpy(mpcName, pcName, luNameSize);

    std::memset(macImpression, 0, sizeof(macImpression));  // clear the live impression (+0x14, 32 bytes)

    CMassiveZoneManager* lpCurrentZone = CMassiveClientCore::Instance()->GetCurrentZone();
    if (!lpCurrentZone)
    {
        mnLastError = -191;
        return;
    }

    CMassiveAdObject* lpAdObject = lpCurrentZone->MAOFind(pcName);
    if (lpAdObject)
        lpAdObject->SubscriberAdd(this);
    else
        CMassiveClientCore::Instance()->GetCurrentZone()->PreSubscriberAdd(this);

    MassiveLog(5, "CMassiveAdObjectSubscriber", "*ADDED MAO SUB: %s", pcName);
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectSubscriber::~CMassiveAdObjectSubscriber
//
// No standalone dtor body is attested for this class in the X360 ledger; the
// vftable install the ctor performs is modelled by declaring the dtor virtual.
// Defined empty (the pooled owner drives teardown).
// ---------------------------------------------------------------------------
CMassiveAdObjectSubscriber::~CMassiveAdObjectSubscriber()
{
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectSubscriber::SetImpression @ 0x82BCEB88
//
// asm: if (a2) { r31 = this+0x14; memset(this+0x14, 0, 0x20);
//                return memcpy(this+0x14, a2, 0x20); }
//      return this;
// A null argument leaves the live impression untouched and returns this; a valid
// one clears then overwrites the 32-byte live record. The X360 leaves the memcpy
// destination (the live buffer) / this in r3 across the two paths.
// ---------------------------------------------------------------------------
void* CMassiveAdObjectSubscriber::SetImpression(void* pImpressionData)
{
    if (pImpressionData)
    {
        std::memset(macImpression, 0, sizeof(macImpression));
        return std::memcpy(macImpression, pImpressionData, sizeof(macImpression));
    }
    return this;
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectSubscriber::GetImpression @ 0x82BCEBE8
//
// asm: memcpy(this+0x34, this+0x14, 0x20);   // snapshot = live
//      if (a2) memset(this+0x14, 0, 0x20);   // clear the live record
//      return this+0x34;                     // the snapshot
// bClear (a2) requests the live record be cleared after the snapshot is taken.
// ---------------------------------------------------------------------------
void* CMassiveAdObjectSubscriber::GetImpression(int bClear)
{
    std::memcpy(macImpressionSnapshot, macImpression, sizeof(macImpressionSnapshot));
    if (bClear)
        std::memset(macImpression, 0, sizeof(macImpression));
    return macImpressionSnapshot;
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectSubscriber::GetInvElementID @ 0x82BCEC38
//
// asm: r11 = this->mpAdObject (+0x08); if (r11) return r11->[+0x48]; return 0;
// Reads the delivered ad's inventory-element id from the attached ad object.
// ---------------------------------------------------------------------------
int CMassiveAdObjectSubscriber::GetInvElementID()
{
    if (mpAdObject)
        return mpAdObject->mnInvElementID;
    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectSubscriber::GetCrexID @ 0x82BCEC58
//
// asm: r3 = this->mpAdObject (+0x08); if (r3) tail-call CMassiveAdObject::GetCrexID();
//      else return 0;
// ---------------------------------------------------------------------------
int CMassiveAdObjectSubscriber::GetCrexID()
{
    if (mpAdObject)
        return mpAdObject->GetCrexID();
    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectSubscriber::MediaDownloadComplete @ 0x82BCEC70
//
// A null data buffer is a no-op returning 1. Otherwise the media-type code
// selects the asset's file extension, the cache path "game:\<advertId>_<name><ext>"
// is formatted (capped at 256 bytes) through MassiveFormatString, and -- when the
// file opens -- the nSize-byte buffer is written, flushed, and the file closed.
// Always returns 1.
//
// The nMediaType -> extension map is the exact X360 switch (unsigned compares):
//   0 -> (none)   1 -> .jpg   2 -> .gif   3 -> .png   4 -> .bmp   5 -> .tga
//   6,7 -> .dds   0x15..0x17 -> .bmp   0x801 -> .wav  0x1002 -> .3ds
//   everything else (incl. 0x08..0x14, 0x18+) -> .bin
// ---------------------------------------------------------------------------
int CMassiveAdObjectSubscriber::MediaDownloadComplete(const void* pData, int nSize,
                                                      unsigned int nMediaType, int nAdvertId)
{
    if (!pData)
        return 1;

    const char* lpcExtension = 0;  // media type 0 -> no extension (null, as the X360)
    switch (nMediaType)
    {
        case 1:      lpcExtension = ".jpg"; break;
        case 2:      lpcExtension = ".gif"; break;
        case 3:      lpcExtension = ".png"; break;
        case 4:      lpcExtension = ".bmp"; break;
        case 5:      lpcExtension = ".tga"; break;
        case 6:
        case 7:      lpcExtension = ".dds"; break;
        case 0x801:  lpcExtension = ".wav"; break;
        case 0x1002: lpcExtension = ".3ds"; break;
        case 0:      break;  // no extension
        default:
            lpcExtension = (nMediaType >= 0x15 && nMediaType <= 0x17) ? ".bmp" : ".bin";
            break;
    }

    char lacPath[288];  // v20 (the X360 reserves 288 bytes; the format is capped at 256)
    MassiveFormatString(lacPath, 0x100, "%s%d_%s%s", "game:\\", nAdvertId, mpcName, lpcExtension);

    void* lpFile = MassiveFileOpen(lacPath, "wb");
    if (lpFile)
    {
        MassiveFileWrite(pData, 1, nSize, lpFile);
        MassiveFileFlush(lpFile);
        MassiveFileClose(lpFile);
    }

    return 1;
}

} // namespace MassiveAdClient3
