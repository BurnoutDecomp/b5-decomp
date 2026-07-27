#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveAsset -- one downloaded MassiveAd media asset
// (vendor middleware). A CMassiveAsset owns the metadata + downloaded media
// buffer for a single ad asset (id / crex / media-type band / URL / 32-byte
// hash) and a list of per-interaction CMassiveRecords; it drives the asset's
// binary download as a CRequestBuilder (it issues a CRequestDownloadBinary and
// handles its completion / failure through the builder's HandleResponse /
// HandleError vftable slots). Sibling of the committed CRequestBuilder /
// CMassiveBaseObject / CMassiveZoneManager family.
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses (12 ledger functions):
//     MassiveAdClient3::CMassiveAsset::CMassiveAsset             @ 0x82BD9EE0
//     MassiveAdClient3::CMassiveAsset::~CMassiveAsset            @ 0x82BDA898
//     MassiveAdClient3::CMassiveAsset::`scalar deleting destructor' @ 0x82BDAAC8
//     MassiveAdClient3::CMassiveAsset::ClearMediaBuffer          @ 0x82BDA100
//     MassiveAdClient3::CMassiveAsset::HandleError               @ 0x82BDA380
//     MassiveAdClient3::CMassiveAsset::HandleResponse            @ 0x82BDA418
//     MassiveAdClient3::CMassiveAsset::RecordCreate              @ 0x82BDA4D0
//     MassiveAdClient3::CMassiveAsset::RecordFind                @ 0x82BDA590
//     MassiveAdClient3::CMassiveAsset::Resume                    @ 0x82BDA358
//     MassiveAdClient3::CMassiveAsset::RequestDownloadBinary     @ 0x82BDA188
//     MassiveAdClient3::CMassiveAsset::ReportImpressions         @ 0x82BDA980
//     MassiveAdClient3::CMassiveAsset::SendReport                @ 0x82BDA688
//
// RequestDownloadBinary and SendReport are now RECONSTRUCTED (earlier wave): their
// former collaborators are homed -- CRequestDownloadBinary (ctor / CreateRequest)
// and CRequestObject::Submit for the download request, and CRequestImpression
// Update::Add{Texture,Video,Audio,Model}Report + the CMassiveRecordImpression
// accumulator getters + CMassiveZoneManager::GetImpressionUpdate for the report.
//
// ReportImpressions @ 0x82BDA980 is now RECONSTRUCTED (this wave -- the last
// blocked body of the TU). It walks each record's two embedded impression
// accumulators + its interaction-record list, reporting each into the current
// zone's pending update and then resetting the record. The formerly-un-homed
// interaction SUB-RECORD type is now homed ADDITIVELY: its three attested data
// fields (+0x14 u32 / +0x18 u64 / +0x20 u16, all offsets read by bare lwz/ld/lhz
// in this TU's asm) extend the shared `InteractionRecord` struct in
// MassiveAdClient3Record.h (previously a one-slot vtable-only opaque). CMassive
// Asset is a friend of CMassiveRecord so the record's impressions / +0x14/+0x18
// fields / interaction list are read BY NAME rather than by offset.
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CMassiveAsset class / its methods) are PRESERVED VERBATIM --
// external middleware API, not project-owned code.
//
// Layout over the CRequestBuilder base (base = CMassiveBaseObject[+0x00..+0x13]
// + CMassiveList mRequestList[+0x14] + int mnSubmitStatus[+0x24]; own members
// begin at +0x28). Absolute offsets are X360-relative and reproduced BY NAME
// (the host base, list and pointers are wider). The base valid/state dword at
// +0x10 doubles as this asset's download state (0 idle/invalid, 19 downloading,
// 20 downloaded, 23 failed), written through the base SetValid helper. The ctor
// stores are (register args a2..a10 + stack args a30/a32/a38/a40):
//   +0x28  mRecordList   (CMassiveList of CMassiveRecord; zeroed in place)
//   +0x38  mpOrder       (0 at ctor; the owning CMassiveOrder, attached by
//                         CRequestEnterZone::ReadOrderBlock @ 0x82BDBEE0
//                         `*(asset + 0x38) = order` -- "Attached Asset(%d) to
//                         Order(%d)"; read back by CMassiveZoneManager::
//                         SetAssetExpiredByOrderID @ 0x82BD3710
//                         `*(mpOrder + 0x14) == nOrderId`)
//   +0x3C  mnAssetId     (a2; the asset "ID" logged everywhere)
//   +0x40  mnCrex        (a3; the asset "Crex"; the ctor's `if (a3)` gate)
//   +0x44  mnMediaType   (a4; the media-type band SendReport switches on)
//   +0x48  mnField48     (a5; the ctor's -595 guard, RequestDownloadBinary gate)
//   +0x4C  mnField4C     (a6; the ctor's -594 guard)
//   +0x50  mnField50     (a7; stored 64-bit; the ctor's -594 guard)
//   +0x58  mnField58     (a8)
//   +0x5C  msField5C     (a30; u16)
//   +0x5E  msField5E     (a32; u16)
//   +0x60  mfField60     (a9; stored as a 32-bit float from the f1 double)
//   +0x64  mfField64     (a10; stored as a 32-bit float from the f2 double)
//   +0x68  mnRefCount    (0; media-buffer reference count)
//   +0x6C  mpcUrl        (0 or a MassiveMalloc'd copy of a38)
//   +0x70  mpHash        (0 or a MassiveMalloc'd 32-byte copy of a40)
//   +0x74  mpMediaBuffer (0; the downloaded media, taken from the request)
//
// NOTE on the constructor signature: the X360 ctor takes its 13 attested values
// in r4..r10 / f1..f2 and four stack slots (a30/a32/a38/a40). Hex-Rays renders a
// 40-argument prototype, but every intermediate slot (a11..a29, a31, a33..a37,
// a39) is UNREFERENCED by the disassembly -- decompiler noise from an over-sized
// incoming-arg guess. The reconstruction keeps only the attested parameters.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"

namespace MassiveAdClient3
{

class CRequestObject;
class CMassiveRecord;
class CMassiveRecordImpression;
class CMassiveOrder;  // owning home: MassiveAdClient3Objects.h (pointer-only here)

class CMassiveAsset : public CRequestBuilder
{
    // The owning CMassiveZoneManager keeps this asset on its mAssetList and, on the
    // X360, reads the asset's id word directly (CMassiveZoneManager::AssetFind
    // @ 0x82BD2D40 compares asset+0x3C == nAssetId with a bare lwz). Granting
    // friendship keeps that a named-member read rather than an offset hack; the
    // zone manager owns the list the asset lives on.
    friend class CMassiveZoneManager;

public:
    // @ 0x82BD9EE0. Chains CRequestBuilder("CMassiveAsset"), installs this class's
    // vftable (off_82186FE8 -- modelled by the virtuals), stores the asset
    // metadata, and -- only when nCrex is non-zero -- validates the required
    // fields (recording -600/-595/-594 on the missing ones), copies the URL into a
    // MassiveMalloc'd buffer (or records -597 on an invalid URL), and copies the
    // 32-byte hash into a MassiveMalloc'd buffer (or records -596 on a null hash).
    // Marks the asset invalid on an allocation failure.
    CMassiveAsset(int nAssetId, int nCrex, int nMediaType, int nField48,
                  int nField4C, long long nField50, int nField58,
                  float fField60, float fField64,
                  unsigned short sField5C, unsigned short sField5E,
                  const char* pcUrl, const void* pHash);

    // @ 0x82BDA898 (slot 0; the scalar deleting destructor @ 0x82BDAAC8 is the
    // compiler-emitted thunk around it). Logs "*REMOVED ASSET", frees the URL and
    // hash buffers, deletes every listed CMassiveRecord (through its vftable) and
    // empties the record list, force-clears the media buffer, then the embedded
    // list and the CRequestBuilder base destruct.
    virtual ~CMassiveAsset();

    // @ 0x82BDAAC8. Scalar deleting destructor (the vftable slot-0 thunk): runs the
    // dtor, then frees the object through CMassiveBaseObject::operator delete when
    // the low bit of bDelete is set. Returns this.
    void* ScalarDeletingDestructor(char bDelete);

    // ----- CRequestBuilder completion callbacks (vftable slots 1 / 2) --------

    // @ 0x82BDA418 (slot 1 override). On a completed download request (server type
    // 84): takes ownership of the request's media buffer, logs success, marks the
    // asset downloaded (state 20) and records a benchmark sample -- or, on an empty
    // response, marks it failed (state 23, error -593). Always removes the request
    // from the builder's collection.
    int HandleResponse(CRequestObject* pRequest) override;

    // @ 0x82BDA380 (slot 2 override). On a failed download request (server type
    // 84): records -598 with the request's URL and marks the asset failed
    // (state 23). Then decrements the media-buffer reference count (floored at 0)
    // and removes the request from the collection. The error-code argument is
    // unused by the X360.
    int HandleError(CRequestObject* pRequest, int nErrorCode) override;

    // ----- media buffer ------------------------------------------------------

    // @ 0x82BDA100. Decrements the media-buffer reference count (floored at 0) and
    // -- when bForce is set or the count reaches 0 -- frees the downloaded media
    // buffer through the MassiveAd heap hook and nulls it.
    void ClearMediaBuffer(int bForce);

    // ----- impression records ------------------------------------------------

    // @ 0x82BDA4D0. Allocates a fresh CMassiveRecord (owning asset back-pointer,
    // this asset's crex, and nRecordId), links it into the record list, and returns
    // it. Records -99 and marks the asset invalid on an allocation failure.
    CMassiveRecord* RecordCreate(int nRecordId);

    // @ 0x82BDA590. Finds the record whose field matches nRecordId; on a hit it
    // clears that record's two impression mnField30 slots and returns it, otherwise
    // it creates + appends a new record (like RecordCreate) and returns that.
    CMassiveRecord* RecordFind(int nRecordId);

    // ----- lifecycle ---------------------------------------------------------

    // @ 0x82BDA358. Resumes the base builder's outstanding requests (name-hides
    // CRequestBuilder::Resume) and returns 1.
    int Resume();

    // @ 0x82BDA188. Kicks off the asset's media download: short-circuits when the
    // asset is already downloading (state 19 / has a media buffer / non-zero ref
    // count) by re-arming the state and bumping the ref count, else validates the
    // URL (-597) and hash (-596) and builds + submits a CRequestDownloadBinary
    // (marking the asset downloading, state 19). A missing media-type band records
    // -595; an allocation failure records -99 and clears the state.
    int RequestDownloadBinary();

    // @ 0x82BDA980 (BLOCKED -- see the file header). Reports every record's
    // impressions + interactions into the zone's pending CRequestImpressionUpdate;
    // reads the un-homed interaction sub-record type; body left for that slice.
    int ReportImpressions();

    // @ 0x82BDA688. Adds one media-type impression-report block (texture / video /
    // audio / model, chosen by mnMediaType) for the given accumulator to the
    // current zone's pending CRequestImpressionUpdate; records -591 when no zone /
    // pending update / accumulator is present.
    int SendReport(int nRecordField18, int bImpressionSlot,
                   CMassiveRecordImpression* pImpression);

private:
    CMassiveList   mRecordList;   // +0x28 (CMassiveRecord list)
    CMassiveOrder* mpOrder;       // +0x38 (0 at ctor; attached by ReadOrderBlock)
    int            mnAssetId;     // +0x3C (a2; the asset "ID")
    int            mnCrex;        // +0x40 (a3; the asset "Crex")
    int            mnMediaType;   // +0x44 (a4; SendReport's media-type band)
    int            mnField48;     // +0x48 (a5)
    int            mnField4C;     // +0x4C (a6)
    long long      mnField50;     // +0x50 (a7; stored 64-bit)
    int            mnField58;     // +0x58 (a8)
    unsigned short msField5C;     // +0x5C (a30)
    unsigned short msField5E;     // +0x5E (a32)
    float          mfField60;     // +0x60 (a9)
    float          mfField64;     // +0x64 (a10)
    int            mnRefCount;    // +0x68 (media-buffer reference count)
    char*          mpcUrl;        // +0x6C (owned URL copy, or 0)
    void*          mpHash;        // +0x70 (owned 32-byte hash copy, or 0)
    void*          mpMediaBuffer; // +0x74 (downloaded media, or 0)
};

} // namespace MassiveAdClient3
