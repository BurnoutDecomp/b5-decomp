#include "SDKs/Packages/MassiveAd/MassiveAdClient3Asset.h"

#include <new>      // placement new (raw heap-hook allocation + explicit construction)
#include <cmath>    // std::cos (attested, result-discarded call in the ctor)
#include <cstddef>  // std::size_t
#include <cstdint>  // std::uintptr_t (owning-asset back-pointer stored as int)
#include <cstring>  // strlen, strncpy, memcpy

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"            // CMassiveBaseObject, heap/log hooks
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"     // CRequestObject
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"  // CMassiveClientCore
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Record.h"      // CMassiveRecord
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RecordImpression.h"        // CMassiveRecordImpression (SendReport source)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ZoneManager.h"             // CMassiveZoneManager (current zone + pending update)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestImpressionUpdate.h" // CRequestImpressionUpdate (Add*Report)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestDownloadBinary.h"   // CRequestDownloadBinary (asset media download)

// ===========================================================================
// MassiveAdClient3 -- CMassiveAsset.
//
// SHAPE and BODIES are reconstructed from BURNOUT_X360_ARTIST.XEX (there is no
// leak source / DecFIGS for this vendor middleware). Stores are reproduced
// member-for-member against the X360 disassembly; see MassiveAdClient3Asset.h
// for the per-offset layout map and the list of BLOCKED bodies (RequestDownload
// Binary / ReportImpressions / SendReport) left for a later ledger slice.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAsset::CMassiveAsset @ 0x82BD9EE0
//
// Chains CRequestBuilder("CMassiveAsset") (installs the request base + zeroes the
// request list), stores the asset metadata (register args a2..a10 + stack args
// a30/a32/a38/a40; a9/a10 truncate from double to the float members), and -- only
// when nCrex (a3) is non-zero -- validates the payload:
//   - records -600 when the id is missing, -595 when field48 is missing, and -594
//     when field4C or field50 is missing (each SetLastError overwrites the last,
//     none early-outs -- the X360 fires them in sequence);
//   - copies the URL into a MassiveMalloc'd buffer when it is a valid string, else
//     records -597 and nulls the URL (a failed URL allocation records -99, marks
//     the asset invalid, and stops);
//   - copies the 32-byte hash into a MassiveMalloc'd buffer, else records -596 and
//     nulls the hash (a failed hash allocation records -99 and stops);
//   - the trailing `std::cos(mfField60)` mirrors the X360's `bl cos` whose result
//     is discarded (an artefact preserved for asm parity; it has no side effect).
// ---------------------------------------------------------------------------
CMassiveAsset::CMassiveAsset(int nAssetId, int nCrex, int nMediaType, int nField48,
                             int nField4C, long long nField50, int nField58,
                             float fField60, float fField64,
                             unsigned short sField5C, unsigned short sField5E,
                             const char* pcUrl, const void* pHash)
    : CRequestBuilder("CMassiveAsset"),
      mRecordList(),
      mnField38(0),          // *(this + 0x38) = 0
      mnAssetId(nAssetId),   // *(this + 0x3C) = a2
      mnCrex(nCrex),         // *(this + 0x40) = a3
      mnMediaType(nMediaType), // *(this + 0x44) = a4
      mnField48(nField48),   // *(this + 0x48) = a5
      mnField4C(nField4C),   // *(this + 0x4C) = a6
      mnField50(nField50),   // *(this + 0x50) = a7 (std, 64-bit)
      mnField58(nField58),   // *(this + 0x58) = a8
      msField5C(sField5C),   // *(this + 0x5C) = a30
      msField5E(sField5E),   // *(this + 0x5E) = a32
      mfField60(fField60),   // *(this + 0x60) = a9 (stfs)
      mfField64(fField64),   // *(this + 0x64) = a10 (stfs)
      mnRefCount(0),         // *(this + 0x68) = 0
      mpcUrl(0),             // *(this + 0x6C) = 0
      mpHash(0),             // *(this + 0x70) = 0
      mpMediaBuffer(0)       // *(this + 0x74) = 0
{
    if (!nCrex)  // if (a3)
        return;

    if (!nAssetId)
        SetLastError(-600, reinterpret_cast<const char*>(0)); // &unk_820046A7
    if (!nField48)
        SetLastError(-595, reinterpret_cast<const char*>(0)); // &unk_820046A7
    if (!nField4C || !nField50)
        SetLastError(-594, reinterpret_cast<const char*>(0)); // &unk_820046A7

    if (CMassiveBaseObject::IsValidString(pcUrl))
    {
        std::size_t luLength = std::strlen(pcUrl) + 1;             // v50
        char* lpcUrl = static_cast<char*>(MassiveMalloc(luLength)); // v51
        mpcUrl = lpcUrl;                                            // *(this + 0x6C)
        if (!lpcUrl)
        {
            SetLastError(-99, reinterpret_cast<const char*>(0)); // &unk_820046A7
            SetValid(0);                                          // *(this + 0x10) = 0
            return;
        }
        std::strncpy(lpcUrl, pcUrl, luLength);
    }
    else
    {
        SetLastError(-597, "INVALID URL for Asset ID: %d", mnAssetId);
        mpcUrl = 0;  // *(this + 0x6C) = 0
    }

    if (!pHash)
    {
        SetLastError(-596, "INVALID Hash for Asset ID: %d", mnAssetId);
        mpHash = 0;  // *(this + 0x70) = 0
    }
    else
    {
        void* lpHash = MassiveMalloc(32);  // v52
        mpHash = lpHash;                   // *(this + 0x70)
        if (!lpHash)
        {
            SetLastError(-99, reinterpret_cast<const char*>(0)); // &unk_820046A7
            return;
        }
        std::memcpy(lpHash, pHash, 32);
    }

    (void)std::cos(mfField60);  // 0x82BDA0E4: `bl cos` with the result discarded
}

// ---------------------------------------------------------------------------
// CMassiveAsset::~CMassiveAsset @ 0x82BDA898
//
// Logs the removal, frees the URL and hash buffers, deletes every listed
// CMassiveRecord through its vftable (`(**data)(data, 1)` == delete), empties the
// record list, and force-clears the media buffer. The embedded record list and
// the CRequestBuilder base then destruct automatically. The vftable slot-0
// rewrite is compiler-emitted for the virtual dtor.
// ---------------------------------------------------------------------------
CMassiveAsset::~CMassiveAsset()
{
    MassiveLog(5, GetName(), "*REMOVED ASSET: %d", mnAssetId);

    if (mpcUrl)  // v4 = *(this + 0x6C)
    {
        MassiveFree(mpcUrl); // off_82F91C18(v4) -- the free hook
        mpcUrl = 0;          // *(this + 0x6C) = 0
    }
    if (mpHash)  // v5 = *(this + 0x70)
    {
        MassiveFree(mpHash);
        mpHash = 0;          // *(this + 0x70) = 0
    }

    mRecordList.GoToStart();
    while (mRecordList.GetCurrent())  // while (*(this + 0x30))
    {
        CMassiveRecord* lpRecord =
            static_cast<CMassiveRecord*>(mRecordList.GetCurrData());
        if (lpRecord)
            delete lpRecord;  // (**data)(data, 1) -- virtual deleting destructor
        mRecordList.GoToNext();
    }
    mRecordList.RemoveAll();

    ClearMediaBuffer(1);  // ClearMediaBuffer(this, 1)
}

// ---------------------------------------------------------------------------
// CMassiveAsset::`scalar deleting destructor' @ 0x82BDAAC8
//
// The vftable slot-0 thunk: run the dtor, then free the object through the base
// operator delete when the low bit of the flag is set. Returns this.
// ---------------------------------------------------------------------------
void* CMassiveAsset::ScalarDeletingDestructor(char bDelete)
{
    this->~CMassiveAsset();
    if (bDelete & 1)
        CMassiveBaseObject::operator delete(this);
    return this;
}

// ---------------------------------------------------------------------------
// CMassiveAsset::HandleResponse @ 0x82BDA418  (CRequestBuilder vtable slot 1)
//
// A request owned by this asset completed. For a download request (server type
// 84) with a non-empty payload: take ownership of the wire buffer as the media
// buffer, log success, mark the asset downloaded (state 20), and record a
// benchmark sample (payload length + the request's +0x38 field) on the client
// core. An empty payload marks the asset failed (state 23, error -593). In every
// case remove the request from the builder's collection.
// ---------------------------------------------------------------------------
int CMassiveAsset::HandleResponse(CRequestObject* pRequest)
{
    if (pRequest->GetServerType() == 84)  // *(a2 + 0x14) == 84
    {
        if (pRequest->GetDataLength())    // *(a2 + 0x24)
        {
            mpMediaBuffer = pRequest->TakeDataOwnership(); // *(this + 0x74)
            MassiveLog(5, GetName(), "ASSET SUCCESS: %s", pRequest->GetRequestURL());
            SetValid(20);  // *(this + 0x10) = 20

            CMassiveClientCore* lpCore = CMassiveClientCore::Instance();
            lpCore->AddBenchmarkData(pRequest->GetDataLength(), pRequest->GetField38());
        }
        else
        {
            SetValid(23);  // *(this + 0x10) = 23
            SetLastError(-593, "Invalid Size: %d", 0);
        }
    }

    return RemoveFromRequestCollect(pRequest);
}

// ---------------------------------------------------------------------------
// CMassiveAsset::HandleError @ 0x82BDA380  (CRequestBuilder vtable slot 2)
//
// A request owned by this asset failed. For a download request (server type 84):
// record -598 with the request's URL and mark the asset failed (state 23). Then
// decrement the media-buffer reference count (floored at 0) and remove the request
// from the collection. The error-code argument is unused by the X360.
// ---------------------------------------------------------------------------
int CMassiveAsset::HandleError(CRequestObject* pRequest, int /*nErrorCode*/)
{
    if (pRequest->GetServerType() == 84)  // *(a2 + 0x14) == 84
    {
        SetLastError(-598, "Asset Failed To Download: %s", pRequest->GetRequestURL());
        SetValid(23);  // *(this + 0x10) = 23
    }

    if (mnRefCount)     // v5 = *(this + 0x68); v6 = v5 - 1; if (!v5) v6 = 0
        --mnRefCount;

    return RemoveFromRequestCollect(pRequest);
}

// ---------------------------------------------------------------------------
// CMassiveAsset::ClearMediaBuffer @ 0x82BDA100
//
// Decrements the media-buffer reference count (floored at 0). When bForce is set
// or the count has reached 0, frees the downloaded media buffer through the
// MassiveAd heap hook and nulls it.
// ---------------------------------------------------------------------------
void CMassiveAsset::ClearMediaBuffer(int bForce)
{
    if (mnRefCount)          // v3 = *(this + 0x68); if (v3)
        --mnRefCount;
    else
        mnRefCount = 0;      // *(this + 0x68) = 0 (explicit X360 store; already 0)

    if (bForce || mnRefCount == 0)  // if (a2 || !*(this + 0x68))
    {
        if (mpMediaBuffer)  // *(this + 0x74)
        {
            MassiveFree(mpMediaBuffer); // off_82F91C18 -- the free hook
            mpMediaBuffer = 0;          // *(this + 0x74) = 0
        }
    }
}

// ---------------------------------------------------------------------------
// CMassiveAsset::RecordCreate @ 0x82BDA4D0
//
// Allocates a fresh CMassiveRecord (owning-asset back-pointer, this asset's crex
// at +0x40, and the caller's nRecordId), wraps it in a list node, and appends it
// to the record list. On an allocation failure records -99 ("... in RecordCreate")
// and marks the asset invalid. Returns the new record (0 on failure).
// ---------------------------------------------------------------------------
CMassiveRecord* CMassiveAsset::RecordCreate(int nRecordId)
{
    void* lpRecordMem = CMassiveListNode::operator new(sizeof(CMassiveRecord)); // li r3, 0xF8
    CMassiveRecord* lpRecord = lpRecordMem
        ? ::new (lpRecordMem) CMassiveRecord(
              // nType == the owning asset back-pointer (X360 stores `this`; the
              // committed CMassiveRecord models the field as int -> truncating cast
              // on the 64-bit host, lossless on the 32-bit X360).
              static_cast<int>(reinterpret_cast<std::uintptr_t>(this)),
              mnCrex,      // *(this + 0x40)
              nRecordId)   // a2
        : 0;

    if (!lpRecord)
    {
        SetLastError(-99, "ALLOCATION Failed for CMassiveRecord in RecordCreate");
        SetValid(0);  // *(this + 0x10) = 0
        return 0;
    }

    void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode)); // li r3, 0xC
    CMassiveListNode* lpNode =
        lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpRecord) : 0;
    mRecordList.Append(lpNode);  // Append(this + 0x28, node)
    return lpRecord;
}

// ---------------------------------------------------------------------------
// CMassiveAsset::RecordFind @ 0x82BDA590
//
// Walks the record list for the record whose field (record+0x18) equals nRecordId.
// On a hit it clears that record's two impression mnField30 slots (record+0x50 and
// record+0xB0, via CMassiveRecord::ClearImpressionField30) and returns it. On a
// miss it creates + appends a new record exactly like RecordCreate (recording -99
// "... in RecordFind" + marking the asset invalid on an allocation failure) and
// returns that.
// ---------------------------------------------------------------------------
CMassiveRecord* CMassiveAsset::RecordFind(int nRecordId)
{
    CMassiveList* lpList = &mRecordList;  // this + 0x28

    lpList->GoToStart();
    while (lpList->GetCurrent())  // while (*(this + 0x30))
    {
        CMassiveRecord* lpCurrent =
            static_cast<CMassiveRecord*>(lpList->GetCurrData());
        if (lpCurrent->GetField18() == nRecordId)  // *(cur + 0x18) == a2
        {
            lpCurrent->ClearImpressionField30();  // *(cur + 0x50) = 0; *(cur + 0xB0) = 0
            return lpCurrent;
        }
        lpList->GoToNext();
    }

    void* lpRecordMem = CMassiveListNode::operator new(sizeof(CMassiveRecord)); // li r3, 0xF8
    CMassiveRecord* lpRecord = lpRecordMem
        ? ::new (lpRecordMem) CMassiveRecord(
              static_cast<int>(reinterpret_cast<std::uintptr_t>(this)),
              mnCrex,      // *(this + 0x40)
              nRecordId)   // a2
        : 0;

    if (!lpRecord)
    {
        SetLastError(-99, "ALLOCATION Failed for CMassiveRecord in RecordFind");
        SetValid(0);  // *(this + 0x10) = 0
        return 0;
    }

    void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode)); // li r3, 0xC
    CMassiveListNode* lpNode =
        lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpRecord) : 0;
    lpList->Append(lpNode);
    return lpRecord;
}

// ---------------------------------------------------------------------------
// CMassiveAsset::Resume @ 0x82BDA358
//
// Resumes the base builder's outstanding requests (name-hides CRequestBuilder::
// Resume) and returns 1.
// ---------------------------------------------------------------------------
int CMassiveAsset::Resume()
{
    CRequestBuilder::Resume();
    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveAsset::RequestDownloadBinary @ 0x82BDA188
//
// Kicks off the asset's media download. Logs the attempt, then short-circuits
// when the asset is already downloading -- state 19, an existing media buffer,
// or a live reference count: it logs the reason, re-arms the state (20 when a
// buffer already exists, else 19) and bumps the reference count. Otherwise, when
// the asset carries a media-type band (mnField48), it validates the URL (-597)
// and hash (-596), allocates + builds a CRequestDownloadBinary through the list-
// node heap hook and submits it -- marking the asset downloading (state 19) and
// bumping the reference count on a clean submit, or recording -99 / clearing the
// state on an allocation or submit failure. A missing media-type band records
// -595. All -59x paths return the SetLastError result; the request path returns
// the Submit result (0 on success).
// ---------------------------------------------------------------------------
int CMassiveAsset::RequestDownloadBinary()
{
    MassiveLog(5, GetName(), "Request Download Binary ID: %d Crex: %d",
               mnAssetId, mnCrex);

    // Already downloading / buffered / referenced -> re-arm and count, don't
    // issue a second request.
    if (GetValid() == 19 || mpMediaBuffer || mnRefCount)
    {
        const char* pcReason;
        if (mpMediaBuffer)
            pcReason = "Buffer Exists";
        else if (mnRefCount)
            pcReason = "Reference Count";
        else
            pcReason = "In Download State";
        MassiveLog(5, GetName(), "Asset is already downloading: %s", pcReason);

        // `((cntlzw(mpMediaBuffer) & 0x20) == 0) + 19` == (mpMediaBuffer ? 20 : 19).
        SetValid(mpMediaBuffer ? 20 : 19);
        ++mnRefCount;
        return 0;
    }

    if (!mnField48)
        return SetLastError(-595, reinterpret_cast<const char*>(0)); // &unk_820046A7

    if (!CMassiveBaseObject::IsValidString(mpcUrl))
        return SetLastError(-597, reinterpret_cast<const char*>(0)); // &unk_820046A7

    if (!mpHash)
        return SetLastError(-596, reinterpret_cast<const char*>(0)); // &unk_820046A7

    void* lpReqMem = CMassiveListNode::operator new(sizeof(CRequestDownloadBinary)); // li r3, 0x58
    CRequestDownloadBinary* lpRequest =
        lpReqMem ? ::new (lpReqMem) CRequestDownloadBinary() : 0;
    if (!lpRequest)
    {
        MassiveLog(2, GetName(),
                   "ALLOCATION Failed for CRequestDownloadBinary.  Asset: %d", mnCrex);
        SetValid(0);  // *(this + 0x10) = 0
        return -99;
    }

    lpRequest->CreateRequest(this, mpcUrl, mpHash, mnField48);
    int lnSubmit = lpRequest->Submit();
    if (lnSubmit)
    {
        SetValid(0);       // *(this + 0x10) = 0
        return lnSubmit;   // the non-zero Submit failure code
    }

    SetValid(19);  // *(this + 0x10) = 19 (downloading)
    ++mnRefCount;
    return lnSubmit;  // 0 on a clean submit
}

// ---------------------------------------------------------------------------
// CMassiveAsset::ReportImpressions @ 0x82BDA980
//
// Flushes every record's accumulated impressions + interactions into the current
// zone's pending CRequestImpressionUpdate, then resets each record. With no
// current zone it records -592 and returns that. For each listed CMassiveRecord:
//   - reports the primary impression accumulator (mImpression0 @ record+0x20)
//     when it has both a non-zero visibility count (+0x58) and elapsed time
//     (+0x34), tagged as the primary slot (1);
//   - reports the secondary accumulator (mImpression1 @ record+0x80) the same way,
//     tagged as the secondary slot (0);
//   - walks the interaction sub-record list and emits one AddInteractionR per
//     entry, reading three fields of the listed sub-record (+0x14 -> uTag31,
//     +0x18 -> nTag51, +0x20 -> sTag32) plus the record's own +0x14/+0x18;
//   - resets the record (clears both accumulators + the interaction list).
// Returns 0.
//
// Faithful note: the X360 loads the current interaction sub-record with THREE
// separate GetCurrData calls (one opaque call per field read; the cursor does not
// advance between them, so all three yield the same pointer) -- reproduced here as
// three sequenced reads to preserve those calls exactly.
// ---------------------------------------------------------------------------
int CMassiveAsset::ReportImpressions()
{
    CMassiveClientCore* lpCore = CMassiveClientCore::Instance();
    CMassiveZoneManager* lpZone = lpCore->GetCurrentZone();
    if (!lpZone)
        return SetLastError(-592, reinterpret_cast<const char*>(0)); // &unk_820046A7

    mRecordList.GoToStart();
    while (mRecordList.GetCurrent())  // *(this + 0x30) -- the record-list cursor
    {
        CMassiveRecord* lpRecord =
            static_cast<CMassiveRecord*>(mRecordList.GetCurrData());

        // Primary impression accumulator (record + 0x20).
        if (lpRecord->mImpression0.GetField58() && lpRecord->mImpression0.GetField34())
            SendReport(lpRecord->mnField18, 1, &lpRecord->mImpression0);

        // Secondary impression accumulator (record + 0x80).
        if (lpRecord->mImpression1.GetField58() && lpRecord->mImpression1.GetField34())
            SendReport(lpRecord->mnField18, 0, &lpRecord->mImpression1);

        // Interaction sub-records: one interaction report per listed entry.
        if (lpRecord->mInteractionRecords.GetCount() != 0)  // *(record + 0xF0)
        {
            lpRecord->mInteractionRecords.GoToStart();
            while (lpRecord->mInteractionRecords.GetCurrent())  // *(record + 0xEC)
            {
                InteractionRecord* lpI1 = static_cast<InteractionRecord*>(
                    lpRecord->mInteractionRecords.GetCurrData());
                InteractionRecord* lpI2 = static_cast<InteractionRecord*>(
                    lpRecord->mInteractionRecords.GetCurrData());
                InteractionRecord* lpI3 = static_cast<InteractionRecord*>(
                    lpRecord->mInteractionRecords.GetCurrData());

                lpZone->GetImpressionUpdate()->AddInteractionR(  // *(CurrentZone + 0x6C)
                    static_cast<unsigned char>(lpI3->GetField14()),      // r4 uTag31 = *(v9 + 0x14)
                    static_cast<unsigned int>(lpRecord->mnField18),      // r5 nTag38 = record + 0x18
                    static_cast<unsigned int>(lpRecord->mnField14),      // r6 nTag21 = record + 0x14
                    static_cast<unsigned long long>(lpI2->GetField18()), // r7 nTag51 = *(v8 + 0x18)
                    lpI1->GetField20());                                 // r8 sTag32 = *(v7 + 0x20)

                lpRecord->mInteractionRecords.GoToNext();
            }
        }

        lpRecord->Reset();
        mRecordList.GoToNext();
    }

    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveAsset::SendReport @ 0x82BDA688
//
// Adds ONE media-type impression-report block for the given accumulator to the
// current zone's pending CRequestImpressionUpdate. The report shape is chosen by
// the asset's media-type band (mnMediaType): texture (< 0x400), video (< 0x800),
// audio (< 0x1000) or model (< 0x2000); a band >= 0x2000 emits nothing. Each
// block reuses the accumulator's count/divisor/dividend slots -- texture/video/
// model divide the field18 hit count by msField1C for the tag-53 ratio and (for
// texture/video) the mfField20 total by msField24 for the tag-48 float; audio
// divides mfField28 by msField2C for the tag-49 float. bImpressionSlot selects
// the viewability tag (1 primary / 2 secondary). Reports only when a current zone
// with a pending update exists AND the accumulator is non-null (else -591); a
// per-band zero guard skips an empty accumulator. The integer divides carry the
// X360's divide-by-zero trap semantics implicitly (msField1C / msField2C are the
// non-zero-guarded denominators).
// ---------------------------------------------------------------------------
int CMassiveAsset::SendReport(int nRecordField18, int bImpressionSlot,
                              CMassiveRecordImpression* pImpression)
{
    CMassiveClientCore* lpCore = CMassiveClientCore::Instance();
    CMassiveZoneManager* lpZone = lpCore->GetCurrentZone();
    CRequestImpressionUpdate* lpUpdate = lpZone ? lpZone->GetImpressionUpdate() : 0;

    if (!pImpression || !lpZone || !lpUpdate)
        return SetLastError(-591, reinterpret_cast<const char*>(0)); // &unk_820046A7

    // uTag54: `((cntlzw(a3) & 0x20) == 0) + 1` == (bImpressionSlot ? 2 : 1).
    const unsigned char uTag54 =
        static_cast<unsigned char>(bImpressionSlot ? 2 : 1);
    const unsigned int nTag21 = static_cast<unsigned int>(mnCrex); // this + 0x40
    const unsigned int nMediaBand = static_cast<unsigned int>(mnMediaType); // +0x44

    // The X360 leaves the pending-update pointer in r3 as the fall-through result
    // (the null-check idiom `(result = zone->update) != 0` reuses the return
    // register); a fired Add*Report overwrites it with the block's own result (0).
    int lnResult = static_cast<int>(reinterpret_cast<std::uintptr_t>(lpUpdate));

    if (nMediaBand < 0x400)
    {
        // Texture report (block type 46, float under tag 48).
        if (pImpression->GetField18())
        {
            lnResult = lpUpdate->AddTextureRepor(
                uTag54,
                static_cast<unsigned int>(nRecordField18),
                nTag21,
                static_cast<unsigned int>(pImpression->GetField34()),
                static_cast<unsigned long long>(pImpression->GetField40()),
                static_cast<unsigned long long>(pImpression->GetField48()),
                static_cast<unsigned short>(
                    static_cast<unsigned int>(pImpression->GetField18())
                        / pImpression->GetField1C()),
                pImpression->GetField20()
                    / static_cast<float>(static_cast<int>(pImpression->GetField24())),
                pImpression->GetField58());
        }
    }
    else if (nMediaBand < 0x800)
    {
        // Video report (byte-identical wire shape to the texture block).
        if (pImpression->GetField18())
        {
            lnResult = lpUpdate->AddVideoReport(
                uTag54,
                static_cast<unsigned int>(nRecordField18),
                nTag21,
                static_cast<unsigned int>(pImpression->GetField34()),
                static_cast<unsigned long long>(pImpression->GetField40()),
                static_cast<unsigned long long>(pImpression->GetField48()),
                static_cast<unsigned short>(
                    static_cast<unsigned int>(pImpression->GetField18())
                        / pImpression->GetField1C()),
                pImpression->GetField20()
                    / static_cast<float>(static_cast<int>(pImpression->GetField24())),
                pImpression->GetField58());
        }
    }
    else if (nMediaBand < 0x1000)
    {
        // Audio report (block type 43, float under tag 49, no tag-53 ratio).
        if (pImpression->GetField2C())
        {
            lnResult = lpUpdate->AddAudioReport(
                uTag54,
                static_cast<unsigned int>(nRecordField18),
                nTag21,
                static_cast<unsigned int>(pImpression->GetField34()),
                static_cast<unsigned long long>(pImpression->GetField40()),
                static_cast<unsigned long long>(pImpression->GetField48()),
                pImpression->GetField28()
                    / static_cast<float>(static_cast<int>(pImpression->GetField2C())),
                pImpression->GetField58());
        }
    }
    else if (nMediaBand < 0x2000)
    {
        // Model report (block type 41, integer ratio under tag 53, no float).
        if (pImpression->GetField18())
        {
            lnResult = lpUpdate->AddModelReport(
                uTag54,
                static_cast<unsigned int>(nRecordField18),
                nTag21,
                static_cast<unsigned int>(pImpression->GetField34()),
                static_cast<unsigned long long>(pImpression->GetField40()),
                static_cast<unsigned long long>(pImpression->GetField48()),
                static_cast<unsigned short>(
                    static_cast<unsigned int>(pImpression->GetField18())
                        / pImpression->GetField1C()),
                pImpression->GetField58());
        }
    }

    return lnResult;
}

} // namespace MassiveAdClient3
