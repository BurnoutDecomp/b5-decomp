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

} // namespace MassiveAdClient3
