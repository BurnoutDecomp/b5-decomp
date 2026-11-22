#pragma once

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"

// ===========================================================================
// MassiveAdClient3::XMassiveAdClientSubscriber -- a per-surface ad subscriber
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
// disassembly (there is no LegacySrc source / DecFIGS DWARF for this subsystem).
// Per-function X360 addresses:
//     MassiveAdClient3::XMassiveAdClientSubscriber::XMassiveAdClientSubscriber @ 0x82BCE968
//     MassiveAdClient3::XMassiveAdClientSubscriber::SetImpression             @ 0x82BCEB88
//     MassiveAdClient3::XMassiveAdClientSubscriber::GetImpression             @ 0x82BCEBE8
//     MassiveAdClient3::XMassiveAdClientSubscriber::GetInvElementID           @ 0x82BCEC38
//     MassiveAdClient3::XMassiveAdClientSubscriber::GetCrexID                 @ 0x82BCEC58
//     MassiveAdClient3::XMassiveAdClientSubscriber::MediaDownloadComplete     @ 0x82BCEC70
//
// The class name and impression signatures come from the SDK's
// massiveadobjectsubscriber.obj. The impression layout follows ARTIST.
//
// Layout (console offsets, reproduced by NAME; the leading vftable pointer is the
// three-slot callback table below, so absolute host offsets differ from the
// console's 4-byte-pointer offsets and are not asserted):
//   +0x00  vftable pointer            (off_8218478C; installed by the ctor)
//   +0x04  mpcName                    (MassiveMalloc'd copy of the slot name)
//   +0x08  mpAdObject                 (the attached CMassiveAdObject, or 0)
//   +0x0C  mnLastError                (ctor error code; 0 on success)
//   +0x10  mnField10                  (zero-initialised; semantics unknown)
//   +0x14  m_CurrentImpression       (the live impression record)
//   +0x34  m_CopyImpression          (GetImpression's returned copy)
//   +0x54  mnField54                  (zero-initialised; semantics unknown)
//   == 0x58 total (BrnMassiveSubscriber's first own field lands at +0x58)
// ===========================================================================

namespace MassiveAdClient3
{

class XMassiveAdClientSubscriber
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
    // The audio/texture/video Dynamic siblings do the same.
    friend class CMassiveAdObjectModelDynamic;
    friend class CMassiveAdObjectTextureDynamic;
    friend class CMassiveAdObjectAudioDynamic;
    friend class CMassiveAdObjectVideoDynamic;

    // The ad placements link, notify and score their subscribers through these fields directly.
    friend class CMassiveAdObject;
    friend class CMassiveAdObjectTexture;
    friend class CMassiveAdObjectAudio;
    friend class CMassiveAdObjectVideo;
    friend class CMassiveAdObjectModel;

public:
    // @ 0x82BCE968. Copies the slot name, then -- when the client core and its
    // current zone are live and the name is valid (>= 2 chars) -- attaches to the
    // matching ad object or queues for a future one. Records a negative error
    // code in mnLastError on any early-out (no core / bad name / alloc failure /
    // no zone).
    XMassiveAdClientSubscriber(const char* pcName);

    // Non-virtual: the class's vftable holds only the three download callbacks
    // below. Re-installs this class's vftable, detaches from the attached ad object
    // (or from the current zone's pre-subscriber queue) and frees the name copy.
    ~XMassiveAdClientSubscriber();

    // ----- download callbacks (the vftable, in slot order) --------------------
    // Slot 0: an ad download for this subscriber has started. Base: returns 1.
    virtual int MediaDownload(int nAdvertId);

    // Slot 1: the delivered asset is ready (body below).
    virtual int MediaDownloadComplete(const void* pData, int nSize, unsigned int nMediaType,
                                      int nAdvertId);

    // Slot 2: per-tick hook. Base: empty.
    virtual void Tick();

    // @ 0x82BCEB88. Records a viewed impression: clears then copies the 32-byte
    // impression block into m_CurrentImpression when pImpression is provided.
    void SetImpression(MAD_Impression* pImpression);

    // @ 0x82BCEBE8. Snapshots the current impression and clears it when bClear is set.
    // Returns the snapshot buffer.
    MAD_Impression* GetImpression(int bClear);

    // @ 0x82BCEC38. Returns the delivered ad's inventory-element id, or 0 when no
    // ad object is attached.
    unsigned int GetInvElementID();

    // @ 0x82BCEC58. Returns the delivered ad's creative id, or 0 when no ad object
    // is attached.
    unsigned int GetCrexID();

    // MediaDownloadComplete (slot 1, declared above): on a valid data buffer,
    // derives the asset's file extension from nMediaType, builds the cache path
    // "game:\<advertId>_<name><ext>", and writes the nSize-byte buffer to it
    // through the MassiveAd file hooks. Always returns 1.

private:
    char*             mpcName;                     // +0x04
    CMassiveAdObject* mpAdObject;                  // +0x08
    int               mnLastError;                 // +0x0C
    int               mnField10;                   // +0x10
    MAD_Impression    m_CurrentImpression;          // +0x14 (live record)
    MAD_Impression    m_CopyImpression;             // +0x34 (GetImpression copy)
    int               mnField54;                   // +0x54
};

} // namespace MassiveAdClient3
