#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveAdObjectSubscriber -- a per-surface ad subscriber
// (vendor middleware).
//
// A subscriber names one ad slot / impression event; on construction it looks the
// slot up in the client's current ad zone and either attaches to the matching ad
// object (CMassiveAdObject::SubscriberAdd) or is queued for a future ad
// (CMassiveZoneManager::PreSubscriberAdd). It owns a heap copy of its name plus a
// double-buffered 32-byte "impression" record (current + snapshot) and forwards
// download callbacks (MediaDownloadComplete writes the delivered asset to the
// media cache; GetCrexID / GetInvElementID read back the delivered ad's ids).
// BrnMassive::BrnMassiveSubscriber (GameSource) derives from this class.
//
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly (there is no Feb-2007 source / DecFIGS DWARF for this subsystem).
// Per-function X360 addresses:
//     MassiveAdClient3::CMassiveAdObjectSubscriber::CMassiveAdObjectSubscriber @ 0x82BCE968
//     MassiveAdClient3::CMassiveAdObjectSubscriber::SetImpression             @ 0x82BCEB88
//     MassiveAdClient3::CMassiveAdObjectSubscriber::GetImpression             @ 0x82BCEBE8
//     MassiveAdClient3::CMassiveAdObjectSubscriber::GetInvElementID           @ 0x82BCEC38
//     MassiveAdClient3::CMassiveAdObjectSubscriber::GetCrexID                 @ 0x82BCEC58
//     MassiveAdClient3::CMassiveAdObjectSubscriber::MediaDownloadComplete     @ 0x82BCEC70
//
// This is vendor/SDK code reconstructed in its canonical vendor home (a sibling
// of MassiveAdClient3.h under SDKs/Packages/MassiveAd). The MassiveAdClient3
// namespace and the class/method names are external middleware identifiers
// PRESERVED VERBATIM (not the project mp/lf/KI_ scheme).
//
// Layout (X360-relative offsets, reproduced by NAME; the leading vftable pointer
// is modelled by the virtual dtor, so absolute host offsets differ from the X360
// 4-byte-pointer offsets and are not asserted):
//   +0x00  vftable pointer            (off_8218478C; installed by the ctor)
//   +0x04  mpcName                    (MassiveMalloc'd copy of the slot name)
//   +0x08  mpAdObject                 (the attached CMassiveAdObject, or 0)
//   +0x0C  mnLastError                (ctor error code; 0 on success)
//   +0x10  mnField10                  (zero-initialised; semantics unknown)
//   +0x14  macImpression[32]          (the live impression record)
//   +0x34  macImpressionSnapshot[32]  (GetImpression's returned copy)
//   +0x54  mnField54                  (zero-initialised; semantics unknown)
//   == 0x58 total (BrnMassiveSubscriber's first own field lands at +0x58)
// ===========================================================================

namespace MassiveAdClient3
{

class CMassiveAdObject;

class CMassiveAdObjectSubscriber
{
    // A pre-subscriber waits on its owning zone's mPreSubscriberList; on the X360
    // CMassiveZoneManager::PreSubscriberAssignT @ 0x82BD3630 reads the queued
    // subscriber's name directly (CompareStrings(pcName, subscriber+0x04)) to match
    // it against a newly-arrived ad object. Granting friendship keeps that a named-
    // member read rather than an offset hack; the zone manager owns the queue.
    friend class CMassiveZoneManager;

    // CMassiveAdObjectModelDynamic::Initialize name-matches each queued subscriber's
    // private mpcName (+0x04) directly -- `CompareStrings(name, subscriber+0x04)`
    // @ 0x82BDEF4C -- the same attested pattern already granted to the zone manager.
    friend class CMassiveAdObjectModelDynamic;

public:
    // @ 0x82BCE968. Copies the slot name, then -- when the client core and its
    // current zone are live and the name is valid (>= 2 chars) -- attaches to the
    // matching ad object or queues for a future one. Records a negative error
    // code in mnLastError on any early-out (no core / bad name / alloc failure /
    // no zone).
    CMassiveAdObjectSubscriber(const char* pcName);

    // The X360 ctor installs this class's vftable (off_8218478C); modelled by a
    // virtual destructor. No standalone dtor body is attested in this TU (the
    // pooled owner, BrnMassive, drives teardown), so it is defined empty here to
    // supply the polymorphic machinery the ctor's vftable store implies.
    virtual ~CMassiveAdObjectSubscriber();

    // @ 0x82BCEB88. Records a viewed impression: clears then copies the 32-byte
    // impression block from pImpressionData into macImpression. A null argument is
    // a no-op. Returns the (buffer / this) pointer the X360 leaves in r3.
    void* SetImpression(void* pImpressionData);

    // @ 0x82BCEBE8. Snapshots the live impression (macImpression ->
    // macImpressionSnapshot) and, when bClear is set, clears the live record.
    // Returns the snapshot buffer.
    void* GetImpression(int bClear);

    // @ 0x82BCEC38. Returns the delivered ad's inventory-element id, or 0 when no
    // ad object is attached.
    int GetInvElementID();

    // @ 0x82BCEC58. Returns the delivered ad's creative id, or 0 when no ad object
    // is attached.
    int GetCrexID();

    // @ 0x82BCEC70. On a valid data buffer, derives the asset's file extension
    // from nMediaType, builds the cache path "game:\<advertId>_<name><ext>", and
    // writes the nSize-byte buffer to it through the MassiveAd file hooks. Always
    // returns 1.
    int MediaDownloadComplete(const void* pData, int nSize, unsigned int nMediaType,
                              int nAdvertId);

private:
    char*             mpcName;                     // +0x04
    CMassiveAdObject* mpAdObject;                  // +0x08
    int               mnLastError;                 // +0x0C
    int               mnField10;                   // +0x10
    unsigned char     macImpression[32];           // +0x14 (live record)
    unsigned char     macImpressionSnapshot[32];   // +0x34 (GetImpression copy)
    int               mnField54;                   // +0x54
};

} // namespace MassiveAdClient3
