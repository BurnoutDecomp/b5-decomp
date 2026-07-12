#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveAdObjectAudioDynamic -- a dynamic-audio ad-object
// (vendor middleware). A CMassiveAdObject subtype and a sibling of the DONE
// CMassiveAdObjectTexture / CMassiveAdObjectVideo (W28/W47): unlike those leaf
// visual objects it is a COMPOSITE -- it owns a list of "slave" ad objects
// (CMassiveAdObjectAudio instances it spawns per delivered asset) and fans its
// own lifecycle operations (Tic / ReportImpressions / Suspend / Resume /
// SetAssetExpired) out across them. It installs its own X360 vftable
// (off_82187264). It is built by CRequestEnterZone::ReadIEBlock (parsing a
// delivered ad).
//
// SHAPE and BODIES are reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly (there is no Feb-2007 source / DecFIGS DWARF for this subsystem).
// Per-function X360 addresses:
//     CMassiveAdObjectAudioDynamic::CMassiveAdObjectAudioDynamic   @ 0x82BDAC48
//     CMassiveAdObjectAudioDynamic::`scalar deleting destructor'   @ 0x82BDACB8
//     CMassiveAdObjectAudioDynamic::Tic                            @ 0x82BDE098
//     CMassiveAdObjectAudioDynamic::Rep                            @ 0x82BDE110
//     CMassiveAdObjectAudioDynamic::Suspend                        @ 0x82BDE188
//     CMassiveAdObjectAudioDynamic::Resume                         @ 0x82BDE1F8
//     CMassiveAdObjectAudioDynamic::SetAssetExpired                @ 0x82BDE018
//     CMassiveAdObjectAudioDynamic::Sub                            @ 0x82BDE268
//     CMassiveAdObjectAudioDynamic::CreateSlaveM                   @ 0x82BDDE20  (BLOCKED, see below)
//     CMassiveAdObjectAudioDynamic::GetNextAssetID                 @ 0x82BDDF30  (BLOCKED, see below)
//     CMassiveAdObjectAudioDynamic::Initialize                     @ 0x82BDED98  (BLOCKED, see below)
//
// This is vendor/SDK code reconstructed in its canonical vendor home (a sibling
// of MassiveAdClient3AdObject.h / MassiveAdClient3ObjectTexture.h /
// MassiveAdClient3ObjectVideo.h under SDKs/Packages/MassiveAd). The
// MassiveAdClient3 namespace and the class name are external middleware
// identifiers PRESERVED VERBATIM (not the project mp/lf/KI_ scheme).
//
// Layout (X360-relative offsets, reproduced by NAME over the CMassiveAdObject
// base; the leading vftable pointer is modelled by the virtual dtor, so absolute
// host offsets differ from the X360 4-byte-pointer offsets and are not asserted):
//   +0x60  mfField60           (float)         -- preserved by the ctor across the
//                                                 base chain (loaded into f31, base
//                                                 ctor, stored back); the value is
//                                                 placed by the caller before
//                                                 construction and this ctor does
//                                                 not set it. Role not grounded (its
//                                                 only readers are outside this TU),
//                                                 same shape as the video object's
//                                                 mfField60.
//   +0x64  mnCurrentAssetID    (int)           -- the currently-selected asset id;
//                                                 cleared when that asset expires.
//   +0x68  muActiveAssetCount  (unsigned short)-- a rotation/active-asset counter,
//                                                 decremented as assets expire.
//   +0x6C  mSlaveList          (CMassiveList)  -- the slave ad objects this dynamic
//                                                 object spawns and drives (+0x6C
//                                                 head / +0x70 tail / +0x74 cursor /
//                                                 +0x78 count).
//
// BLOCKED functions (declared here so the class shape is complete and the
// grounded methods compile against them; their bodies reach into collaborator
// layouts that are not yet homed):
//   - CreateSlaveM @ 0x82BDDE20 constructs a CMassiveAdObjectAudio "slave"
//     (off_821871C4) -- an un-homed sibling ad-object class -- inline (operator
//     new + CMassiveAdObject base ctor + manual vftable install), calls the base
//     CMassiveAdObject::AssetIDAdd (a not-yet-homed base method), reads a base
//     field at X360 +0x38 that the minimal CMassiveAdObject pin does not model,
//     and links the slave into mSlaveList. Homing it faithfully needs the slave
//     class and the base's asset machinery first.
//   - GetNextAssetID @ 0x82BDDF30 walks the base CMassiveAdObject's delivered-
//     asset list (X360 +0x50) and its u16 count (+0x4C) -- neither modelled by the
//     minimal base pin -- and reads each list element's first dword as an asset id
//     through an un-homed element type. Left for when the base asset list and its
//     element type are recovered.
//   - Initialize @ 0x82BDED98 pulls the current zone off CMassiveClientCore, walks
//     the zone manager's internal list (X360 +0x28), reads un-homed base fields
//     (+0x18), CompareStrings-matches an un-homed list element's name (+0x04), and
//     spawns a slave per match via CreateSlaveM. Blocked with CreateSlaveM.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"  // CMassiveAdObject
#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"          // CMassiveList

namespace MassiveAdClient3
{

class CMassiveAdObjectSubscriber;

// ---------------------------------------------------------------------------
// CMassiveAdObjectAudioDynamic -- a composite CMassiveAdObject that drives a
// list of slave audio ad objects.
// ---------------------------------------------------------------------------
class CMassiveAdObjectAudioDynamic : public CMassiveAdObject
{
public:
    // @ 0x82BDAC48. Chains the CMassiveAdObject base ctor (pcName, a3, a7, a8),
    // installs this class's vftable (off_82187264, modelled by the virtual dtor),
    // zeroes mnCurrentAssetID / muActiveAssetCount and the mSlaveList container.
    // mfField60 (+0x60) is preserved across the base chain (the caller sets it
    // before construction), so this ctor does not initialise it. The a6 argument
    // is part of the ABI but unused by this ctor (its register is clobbered before
    // the base chain).
    CMassiveAdObjectAudioDynamic(const char* pcName, int a3, int a6, int a7, int a8);

    // @ 0x82BDACB8 (scalar deleting destructor). No own explicit teardown -- the
    // X360 scalar deleting destructor installs this class's vftable, chains
    // ~CMassiveAdObjectAudioDynamic (which tears down mSlaveList), and (when the
    // low bit of the delete flag is set) frees the object through
    // CMassiveBaseObject::operator delete. The vftable install + member/base-dtor
    // chain are the compiler-emitted virtual-dtor body (mSlaveList's own dtor runs
    // automatically); the conditional free is the deleting-destructor thunk MSVC
    // synthesises around it.
    virtual ~CMassiveAdObjectAudioDynamic();

    // @ 0x82BDE098. Composite override of the ad-object tick (base vftable +0x0C):
    // ticks each slave and returns the first non-zero sub-result (a stop/error
    // code), or 0 when all slaves returned 0.
    int Tic() override;

    // @ 0x82BDE188. Composite override of the ad-object suspend (base vftable
    // +0x14): suspends each slave. Always returns 0 (the X360 returns the
    // exhausted list cursor, which is null at the loop exit).
    int Suspend() override;

    // @ 0x82BDE1F8. Composite override of the ad-object resume (base vftable
    // +0x18): resumes each slave. Always returns 0 (the X360 returns the exhausted
    // list cursor, which is null at the loop exit).
    int Resume() override;

    // @ 0x82BDE018. Override of the ad-object asset-expiry (base vftable +0x08):
    // chains the base CMassiveAdObject::SetAssetExpired first and returns 0 if it
    // rejects; otherwise decrements muActiveAssetCount (clamped at 0), clears
    // mnCurrentAssetID when it matches the expiring asset id, and returns 1.
    int SetAssetExpired(int nAssetId) override;

    // @ 0x82BDE110. The composite "report impressions" pass: reports each slave's
    // impressions (the slave's own vftable +0x10) and returns the first non-zero
    // sub-result, or 0. Kept under its X360 identity name Rep (not marked an
    // override of the base ReportImpressions +0x10 slot: the composite forwarding
    // is modelled by NAME -- it calls slave->ReportImpressions() directly -- so
    // the vtable slot mapping is not asserted, matching the base pin's stance).
    int Rep();

    // @ 0x82BDE268. Subscribe entry point: with a null subscriber it records
    // error -500 via the base SetLastError; otherwise it spawns a slave audio ad
    // object bound to that subscriber (CreateSlaveM). Returns the error code on
    // the null path, else 0.
    int Sub(CMassiveAdObjectSubscriber* pSubscriber);

    // ----- BLOCKED (declared for class shape; bodies un-homed, see header top) --

    // @ 0x82BDDE20. Spawns and links a slave CMassiveAdObjectAudio. BLOCKED:
    // constructs an un-homed sibling class inline and calls not-yet-homed base
    // asset machinery. Returns the new slave (or null).
    CMassiveAdObject* CreateSlaveM(CMassiveAdObjectSubscriber* pSubscriber);

    // @ 0x82BDDF30. Override of the base asset-id cursor (vftable +0x20). BLOCKED:
    // walks the un-homed base delivered-asset list.
    int GetNextAssetID() override;

    // @ 0x82BDED98. Builds slaves for the current zone's matching entries.
    // BLOCKED: walks un-homed zone-manager / base internals and calls CreateSlaveM.
    int Initialize();

private:
    float          mfField60;          // +0x60 (preserved across ctor; role not grounded)
    int            mnCurrentAssetID;   // +0x64
    unsigned short muActiveAssetCount; // +0x68 (sth/lhz)
    CMassiveList   mSlaveList;         // +0x6C (spawned slave ad objects)
};

} // namespace MassiveAdClient3
