#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

#include <cstring>  // memcpy, memset, memcmp, strlen, strncpy

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3NetworkManager.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestManager.h"

// ===========================================================================
// MassiveAdClient3::CRequestObject -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both from the X360 asm (no leak / DecFIGS). Stores reproduced
// member-for-member; see MassiveAdClient3Request.h for the layout map. The class
// installs its own vftable (off_82184858) over the CMassiveBaseObject slot --
// modelled by the virtual destructor + virtual Parse, so no vftable store is
// written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// byte_8327F2A0 -- the client HMAC key string (see the header FLAG on the
// inferred 36-byte capacity). Zero-initialised .data, filled elsewhere.
char gacHMACKey[36];

// dword_8327F2C4 / dword_8327F2C8 -- the third-party ID / service strings.
char* gpcThirdPartyID = 0;
char* gpcThirdPartyService = 0;

// dword_8327F2CC / dword_8327F2D0 -- the client-wide session / player IDs.
// Zero-initialised .data, stamped in by the session-open path elsewhere.
unsigned int gnMassiveSessionID = 0;
unsigned int gnMassivePlayerID = 0;

// unk_8327F2D8 -- the 144-byte public-key block.
unsigned char gabPublicKey[144];

// ---------------------------------------------------------------------------
// MassiveNetOrderU64 / MassiveNetOrderFloat -- the Hex-Rays `STUB` byte-order
// hooks (see the header FLAG). Every X360 call site stores the returned
// register straight back over the input value, i.e. on this big-endian build
// the helpers are identities (host order == network order); reproduced as the
// identity pass-throughs the machine code performs.
// ---------------------------------------------------------------------------
unsigned long long MassiveNetOrderU64(unsigned long long nValue)
{
    return nValue;
}

float MassiveNetOrderFloat(float fValue)
{
    return fValue;
}

// ---------------------------------------------------------------------------
// CRequestObject::CRequestObject @ 0x82BCF1F8
//
//   CMassiveBaseObject::CMassiveBaseObject(this, a3);   (a3 = derived name)
//   +0x14 = a2; +0x18 = 1; vftable = off_82184858;
//   +0x1C..+0x30 = 0; std 0 @ +0x38; +0x40 = 0; +0x44 = 1; +0x48 = 50;
//
// (Hex-Rays folded the +0x38/+0x40..+0x48 stores into a bogus 0x3200000000
//  qword; the raw asm stores zero at +0x38 (std), zero at +0x40, one at +0x44
//  and 0x32 at +0x48 -- the asm wins.)
// ---------------------------------------------------------------------------
CRequestObject::CRequestObject(int nServerIndex, const char* pcName)
    : CMassiveBaseObject(pcName)  // bl CMassiveBaseObject::CMassiveBaseObject
    , mnServerIndex(nServerIndex) // stw a2,  0x14
    , mnStatus(1)                 // stw 1,   0x18
    , mpDataBuffer(0)             // stw 0,   0x1C
    , mnPosition(0)               // stw 0,   0x20
    , mnDataLength(0)             // stw 0,   0x24
    , mnBufferSize(0)             // stw 0,   0x28
    , mpBuilder(0)                // stw 0,   0x2C
    , mpSignature(0)              // stw 0,   0x30
    , mnField38(0)                // std 0,   0x38
    , mnField40(0)                // stw 0,   0x40
    , mbOwnsData(1)               // stw 1,   0x44
    , mnField48(50)               // stw 0x32,0x48
{
}

// ---------------------------------------------------------------------------
// CRequestObject::~CRequestObject @ 0x82BCF280
//
//   *this = off_82184858;                       (vftable rewrite -- implicit)
//   if (mpDataBuffer && mbOwnsData) { MassiveFree(mpDataBuffer); mpDataBuffer = 0; }
//   if (mpSignature)                { MassiveFree(mpSignature);  mpSignature  = 0; }
//   CMassiveBaseObject::~CMassiveBaseObject(this);
//
// The scalar deleting destructor @ 0x82BCFA18 is the compiler-emitted thunk
// around this dtor (conditional CMassiveBaseObject::operator delete), modelled
// by the virtual dtor + the base's static operator delete.
// ---------------------------------------------------------------------------
CRequestObject::~CRequestObject()
{
    if (mpDataBuffer && mbOwnsData)
    {
        MassiveFree(mpDataBuffer);
        mpDataBuffer = 0;
    }

    if (mpSignature)
    {
        MassiveFree(mpSignature);
        mpSignature = 0;
    }
}

// ---------------------------------------------------------------------------
// CRequestObject::CreateRequest @ 0x82BD03E0
// ---------------------------------------------------------------------------
int CRequestObject::CreateRequest(CRequestBuilder* pBuilder, int nBufferSize, int nField48)
{
    if (!pBuilder)
        return -1100;

    mpBuilder = pBuilder;   // stw r3, 0x2C
    mnField48 = nField48;   // stw r6, 0x48
    pBuilder->AddToRequestCollection(this);

    if (AllocateDataBuffer(nBufferSize) != 0)
        return SetLastError(-99, "Could not allocate raw data buffer.");

    return 0;
}

// ---------------------------------------------------------------------------
// CRequestObject::Submit @ 0x82BCFF70
// ---------------------------------------------------------------------------
int CRequestObject::Submit()
{
    int lnStatus = mnStatus;
    if (lnStatus != 2)
        return SetLastError(-1098,
                            "Could not be submitted to RequestManager. Bad request status(%d).",
                            lnStatus);

    SetStatus(mpBuilder->GetSubmitStatus());  // SetStatus(*(mpBuilder + 0x24))

    CRequestManager* lpManager = spRequestManager;  // off_8327F368
    if (lpManager)
    {
        int lnResult = lpManager->SubmitRequest(this);
        if (lnResult == 0)
            return lnResult;
    }

    SetStatus(0x2000);
    return SetLastError(-1097, "Could not be submitted to RequestManager.");
}

// ---------------------------------------------------------------------------
// CRequestObject::Complete @ 0x82BD0468
// ---------------------------------------------------------------------------
int CRequestObject::Complete()
{
    SetStatus(0x1000);

    int lnResult = Parse();  // (*(*this + 4))(this) -- virtual slot 1
    if (!lnResult)
        return Error(5);

    if ((mnStatus & 0xF00000) == 0)
        return mpBuilder->OnRequestComplete(this);  // builder vtable slot 1

    return lnResult;
}

// ---------------------------------------------------------------------------
// CRequestObject::Error @ 0x82BD0008
// ---------------------------------------------------------------------------
int CRequestObject::Error(int nErrorCode)
{
    int lnResult = SetStatus(0x2000);

    if ((mnStatus & 0xF00000) == 0)
        return mpBuilder->OnRequestError(this, nErrorCode);  // builder vtable slot 2

    return lnResult;
}

// ---------------------------------------------------------------------------
// CRequestObject::Cancel @ 0x82BD0070
// ---------------------------------------------------------------------------
void CRequestObject::Cancel()
{
    SetStatus(0x103000);

    CRequestManager* lpManager = spRequestManager;  // off_8327F368
    if (!lpManager)
        return;

    if (lpManager->RemoveRequest(this) != -697)
        return;

    CNetworkManager* lpNetwork = spNetworkManager;
    if (!lpNetwork)
        return;

    if (this != lpNetwork->GetCurrentRequest())  // *(spNetworkManager + 0x70)
        return;

    lpNetwork->RemoveCurrentRequest();
}

// ---------------------------------------------------------------------------
// CRequestObject::Suspend @ 0x82BD00E8 / Resume @ 0x82BD00F0
// (both are bare tail-branches into SetStatus)
// ---------------------------------------------------------------------------
int CRequestObject::Suspend()
{
    return SetStatus(0x20);
}

int CRequestObject::Resume()
{
    return SetStatus(0x10);
}

// ---------------------------------------------------------------------------
// CRequestObject::GetServerIndex @ 0x82BCF4F0 / IsPending @ 0x82BCF500 /
// IsActive @ 0x82BCF510 / IsComplete @ 0x82BCF528
// ---------------------------------------------------------------------------
int CRequestObject::GetServerIndex()
{
    return mnServerIndex & 0xF;  // clrlwi r3, +0x14, 28
}

int CRequestObject::IsPending()
{
    return mnStatus & 0xF0;  // rlwinm +0x18, 0,24,27
}

int CRequestObject::IsActive()
{
    return mnStatus == 0x10;  // cntlzw/extrwi boolean
}

int CRequestObject::IsComplete()
{
    return mnStatus & 0xF000;  // rlwinm +0x18, 0,16,19
}

// ---------------------------------------------------------------------------
// CRequestObject::AllocateDataBuffer @ 0x82BCFAD8
// ---------------------------------------------------------------------------
int CRequestObject::AllocateDataBuffer(int nSize)
{
    if (nSize <= 0)
        return SetLastError(-99, "Can't allocate buffer of 0 or negative size.");

    if (mpDataBuffer && mnBufferSize > 0)
        return SetLastError(-99,
                            "Buffer already exists.  Can't allocate new, use ReAllocateDataBuffer.");

    unsigned char* lpBuffer = static_cast<unsigned char*>(MassiveMalloc(nSize));
    mpDataBuffer = lpBuffer;  // stw r3, 0x1C (stored before the null test)
    mnPosition   = 0;         // stw 0,  0x20
    mnDataLength = 0;         // stw 0,  0x24

    if (lpBuffer)
    {
        mnBufferSize = nSize;  // stw r30, 0x28
        return 0;
    }

    mpDataBuffer = 0;  // stw 0, 0x1C
    mnBufferSize = 0;  // stw 0, 0x28
    return SetLastError(-99, "Could not allocate raw request buffer");
}

// ---------------------------------------------------------------------------
// CRequestObject::ReAllocateDataBuffer @ 0x82BCFB98
// ---------------------------------------------------------------------------
int CRequestObject::ReAllocateDataBuffer(int nNewSize)
{
    if (nNewSize <= 0)
        return SetLastError(-99,
                            "Can't reallocate buffer where new size is 0 or negative size.");

    if (!mpDataBuffer || mnBufferSize <= 0)
        return AllocateDataBuffer(nNewSize);  // r4 passes through untouched

    int lnGrownSize = nNewSize + 64;  // addi r29, r4, 0x40
    unsigned char* lpBuffer = static_cast<unsigned char*>(MassiveMalloc(lnGrownSize));
    if (!lpBuffer)
        return SetLastError(-99,
                            "Could not reallocate buffer, leaving old buffer untouched.");

    std::memcpy(lpBuffer, mpDataBuffer, mnDataLength);
    MassiveFree(mpDataBuffer);
    mpDataBuffer = lpBuffer;      // stw r30, 0x1C
    mnBufferSize = lnGrownSize;   // stw r29, 0x28
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestObject::ResetDataBuffer @ 0x82BCFA68
// ---------------------------------------------------------------------------
int CRequestObject::ResetDataBuffer()
{
    if (!mpDataBuffer || mnBufferSize <= 0)
        return SetLastError(-99, "ResetDataBuffer called on invalid buffer.");

    std::memset(mpDataBuffer, 0, mnBufferSize);
    mnPosition   = 0;  // stw 0, 0x20
    mnDataLength = 0;  // stw 0, 0x24
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestObject::AppendToRequestBuffer @ 0x82BCFC50
// ---------------------------------------------------------------------------
int CRequestObject::AppendToRequestBuffer(const void* pData, int nLength)
{
    if (!mpDataBuffer || mnBufferSize <= 0)
    {
        SetLastError(-99, "Can't append to an invalid buffer.");
        return -1;
    }

    if (!pData || nLength <= 0)
    {
        SetLastError(-99, "Data being passed to append is null or zero/negative length");
        return -1;
    }

    if (mnDataLength + nLength > mnBufferSize &&
        ReAllocateDataBuffer(mnDataLength + nLength) != 0)
        return -1;

    std::memcpy(mpDataBuffer + mnPosition, pData, nLength);
    mnDataLength += nLength;    // stw @ 0x24
    mnPosition = mnDataLength;  // stw @ 0x20 (same value)
    return nLength;
}

// ---------------------------------------------------------------------------
// CRequestObject::PrependToRequestBuffer @ 0x82BCFD10
// ---------------------------------------------------------------------------
int CRequestObject::PrependToRequestBuffer(const void* pData, int nLength)
{
    if (!mpDataBuffer || mnBufferSize <= 0)
    {
        SetLastError(-99, "Can't prepend to an invalid buffer.");
        return -1;
    }

    if (!pData || nLength <= 0)
    {
        SetLastError(-99, "Data being passed to prepend is null or zero/negative length");
        return -1;
    }

    if (mnDataLength + nLength > mnBufferSize &&
        ReAllocateDataBuffer(mnDataLength + nLength) != 0)
        return -99;  // li r3, -0x63 (this path differs from Append's -1)

    // Shift the live payload up by nLength, one byte at a time, high to low.
    for (int lnIndex = mnDataLength - 1; lnIndex >= 0; --lnIndex)
        mpDataBuffer[lnIndex + nLength] = mpDataBuffer[lnIndex];

    std::memcpy(mpDataBuffer, pData, nLength);
    mnDataLength += nLength;    // stw @ 0x24
    mnPosition = mnDataLength;  // stw @ 0x20
    return nLength;
}

// ---------------------------------------------------------------------------
// CRequestObject::RemoveFromRequestBuffer @ 0x82BCFDF0
// ---------------------------------------------------------------------------
int CRequestObject::RemoveFromRequestBuffer(int nOffset, int nLength)
{
    if (!mpDataBuffer || mnBufferSize <= 0)
    {
        SetLastError(-99, "Can't remove data from an invalid buffer.");
        return -1;
    }

    if (nOffset + nLength > mnDataLength)
    {
        SetLastError(-99, "Length of data to remove will overrun buffer. Not removing.");
        return -1;
    }

    if (nLength == 0)
    {
        SetLastError(-99, "Length of data to remove is zero.");
        return -1;
    }

    // Compact the tail down over the removed range, one byte at a time.
    if (nOffset + nLength < mnDataLength)
    {
        int lnIndex = nOffset;
        do
        {
            mpDataBuffer[lnIndex] = mpDataBuffer[lnIndex + nLength];
            ++lnIndex;
        } while (lnIndex + nLength < mnDataLength);
    }

    mnDataLength -= nLength;  // stw @ 0x24
    if (mnPosition > nOffset)
        mnPosition -= nLength;  // stw @ 0x20

    return nLength;
}

// ---------------------------------------------------------------------------
// CRequestObject::TakeDataOwnership @ 0x82BCF2F8
// ---------------------------------------------------------------------------
unsigned char* CRequestObject::TakeDataOwnership()
{
    if (!mbOwnsData)
        return 0;

    unsigned char* lpBuffer = mpDataBuffer;  // lwz 0x1C
    mbOwnsData = 0;                          // stw 0, 0x44
    return lpBuffer;
}

// ---------------------------------------------------------------------------
// CRequestObject::ReadU8 @ 0x82BCF5D0
// ---------------------------------------------------------------------------
int CRequestObject::ReadU8()
{
    if (!mpDataBuffer)
        return 0;
    if (mnBufferSize <= 0)
        return 0;
    if (mnDataLength - mnPosition < 1)
        return 0;

    int lnValue = mpDataBuffer[mnPosition];  // lbzx
    mnPosition += 1;
    return lnValue;
}

// ---------------------------------------------------------------------------
// CRequestObject::ReadU16 @ 0x82BCF618
// ---------------------------------------------------------------------------
int CRequestObject::ReadU16()
{
    if (!mpDataBuffer)
        return 0;
    if (mnBufferSize <= 0)
        return 0;
    if (mnDataLength - mnPosition < 2)
        return 0;

    unsigned short lnValue = 0;  // sth 0 into the stack slot first
    std::memcpy(&lnValue, mpDataBuffer + mnPosition, 2);
    mnPosition += 2;
    return lnValue;  // lhz (zero-extended)
}

// ---------------------------------------------------------------------------
// CRequestObject::ReadU32 @ 0x82BCF6A0
// ---------------------------------------------------------------------------
unsigned int CRequestObject::ReadU32()
{
    if (!mpDataBuffer)
        return 0;
    if (mnBufferSize <= 0)
        return 0;
    if (mnDataLength - mnPosition < 4)
        return 0;

    unsigned int lnValue = 0;  // stw 0 into the stack slot first
    std::memcpy(&lnValue, mpDataBuffer + mnPosition, 4);
    mnPosition += 4;
    return lnValue;
}

// ---------------------------------------------------------------------------
// CRequestObject::ReadFloat @ 0x82BCF7B0
//
// (Hex-Rays lost the f1 return; the asm returns flt_82001CC0 (0.0f) on the
//  bounds-failure path and the STUB-hooked extracted value otherwise.)
// ---------------------------------------------------------------------------
float CRequestObject::ReadFloat()
{
    if (!mpDataBuffer || mnBufferSize <= 0 || mnDataLength - mnPosition < 4)
        return 0.0f;  // lfs f1, flt_82001CC0

    float lfValue = 0.0f;  // stfs flt_82001CC0 into the stack slot first
    std::memcpy(&lfValue, mpDataBuffer + mnPosition, 4);
    mnPosition += 4;
    return MassiveNetOrderFloat(lfValue);  // bl STUB (f1 pass-through)
}

// ---------------------------------------------------------------------------
// CRequestObject::ReadString @ 0x82BD0330
// ---------------------------------------------------------------------------
char* CRequestObject::ReadString()
{
    unsigned short lnLength = static_cast<unsigned short>(ReadU16());  // clrlwi 16

    if (!mpDataBuffer || mnBufferSize <= 0 || mnDataLength - mnPosition < lnLength)
        return 0;

    char* lpString = static_cast<char*>(MassiveMalloc(lnLength + 1));
    if (!lpString)
        return 0;

    std::memcpy(lpString, mpDataBuffer + mnPosition, lnLength);
    std::memcpy(lpString + lnLength, "", 1);  // &unk_82071450 -- the shared NUL
    mnPosition += lnLength;
    return lpString;
}

// ---------------------------------------------------------------------------
// CRequestObject::ReadU32Array @ 0x82BCF840
//
// FLAG (faithful X360 quirk): the remaining-bytes check compares against the
// element COUNT, not count*4 bytes -- reproduced verbatim.
// ---------------------------------------------------------------------------
void CRequestObject::ReadU32Array(unsigned int** ppValues, unsigned short* pnCount)
{
    if (!pnCount)
        return;

    int lnCount = ReadU16();
    *pnCount = static_cast<unsigned short>(lnCount);  // sth r3
    if ((lnCount & 0xFFFF) == 0)                       // clrlwi. 16
    {
        *ppValues = 0;
        return;
    }

    if (!mpDataBuffer)
        return;
    if (mnBufferSize <= 0)
        return;
    if (mnDataLength - mnPosition < (lnCount & 0xFFFF))
        return;

    *ppValues = static_cast<unsigned int*>(MassiveMalloc(4 * (lnCount & 0xFFFF)));
    if (!*ppValues)
        return;

    if (*pnCount)
    {
        unsigned int lnIndex = 0;
        do
        {
            unsigned int lnValue = ReadU32();
            (*ppValues)[lnIndex] = lnValue;
            lnIndex = static_cast<unsigned short>(lnIndex + 1);  // clrlwi 16
        } while (lnIndex < *pnCount);
    }
}

// ---------------------------------------------------------------------------
// CRequestObject::ReadByteArray @ 0x82BCF908
// ---------------------------------------------------------------------------
void CRequestObject::ReadByteArray(unsigned char** ppData, unsigned short* pnLength)
{
    if (!pnLength)
        return;

    int lnLength = ReadU16();
    *pnLength = static_cast<unsigned short>(lnLength);  // sth r11
    if ((lnLength & 0xFFFF) == 0)                        // clrlwi. 16
    {
        *ppData = 0;
        return;
    }

    if (!mpDataBuffer)
        return;
    if (mnBufferSize <= 0)
        return;
    if (mnDataLength - mnPosition < (lnLength & 0xFFFF))
        return;

    *ppData = static_cast<unsigned char*>(MassiveMalloc(lnLength & 0xFFFF));
    if (!*ppData)
        return;

    std::memcpy(*ppData, mpDataBuffer + mnPosition, *pnLength);
    mnPosition += *pnLength;  // lhz + add + stw
}

// ---------------------------------------------------------------------------
// CRequestObject::SkipField @ 0x82BCF9C0
// ---------------------------------------------------------------------------
int CRequestObject::SkipField(unsigned char bFieldType)
{
    if (bFieldType < 0xC8)
        return 0;

    mnPosition += ReadU32();  // ReadU32 consumes the size dword, then skip it
    return 1;
}

// ---------------------------------------------------------------------------
// CRequestObject::ReadRemoveVerifyProtocolVersion @ 0x82BD0280
//
// The asm snapshots the cursor BEFORE ReadU8 (lwz r4, 0x20 ahead of the call)
// and removes the version byte at that pre-read offset.
// ---------------------------------------------------------------------------
int CRequestObject::ReadRemoveVerifyProtocolVersion()
{
    int lnVersionOffset = mnPosition;
    unsigned char lbVersion = static_cast<unsigned char>(ReadU8());

    RemoveFromRequestBuffer(lnVersionOffset, 1);

    return lbVersion == 4;  // cntlzw/extrwi boolean
}

// ---------------------------------------------------------------------------
// CRequestObject::VerifyHMACSignature @ 0x82BCF538
// ---------------------------------------------------------------------------
int CRequestObject::VerifyHMACSignature()
{
    int lnKeyLength = static_cast<int>(std::strlen(gacHMACKey));
    void* lpDigest = CalculateSHA1HMac(mpDataBuffer, mnDataLength, gacHMACKey, lnKeyLength);
    if (!lpDigest)
        return 0;

    int lbMatches = std::memcmp(mpSignature, lpDigest, 20) == 0;
    MassiveFree(lpDigest);
    return lbMatches;
}

// ---------------------------------------------------------------------------
// CRequestObject::WriteU8 @ 0x82BD00F8 / WriteU16 @ 0x82BD0138 /
// WriteU32 @ 0x82BD0178 -- fixed-width scalar writes through a stack slot.
// ---------------------------------------------------------------------------
int CRequestObject::WriteU8(char bValue, int bPrepend)
{
    char lbValue = bValue;  // stb r4 into the stack slot
    if (bPrepend)
        return PrependToRequestBuffer(&lbValue, 1);
    return AppendToRequestBuffer(&lbValue, 1);
}

int CRequestObject::WriteU16(short nValue, int bPrepend)
{
    short lnValue = nValue;  // sth r4 into the stack slot
    if (bPrepend)
        return PrependToRequestBuffer(&lnValue, 2);
    return AppendToRequestBuffer(&lnValue, 2);
}

int CRequestObject::WriteU32(int nValue, int bPrepend)
{
    int lnValue = nValue;  // stw r4 into the stack slot
    if (bPrepend)
        return PrependToRequestBuffer(&lnValue, 4);
    return AppendToRequestBuffer(&lnValue, 4);
}

// ---------------------------------------------------------------------------
// CRequestObject::WriteU64 @ 0x82BD01B8 -- the value passes through the 64-bit
// byte-order hook (bl STUB with the value in r3, result stored back) before the
// 8-byte write.
// ---------------------------------------------------------------------------
int CRequestObject::WriteU64(unsigned long long nValue, int bPrepend)
{
    unsigned long long lnValue = MassiveNetOrderU64(nValue);
    if (bPrepend)
        return PrependToRequestBuffer(&lnValue, 8);
    return AppendToRequestBuffer(&lnValue, 8);
}

// ---------------------------------------------------------------------------
// CRequestObject::WriteFloat @ 0x82BD0220 -- the value passes through the float
// byte-order hook (bl STUB with the value in f1, result stored back) before the
// 4-byte write.
// ---------------------------------------------------------------------------
int CRequestObject::WriteFloat(float fValue, int bPrepend)
{
    float lfValue = MassiveNetOrderFloat(fValue);
    if (bPrepend)
        return PrependToRequestBuffer(&lfValue, 4);
    return AppendToRequestBuffer(&lfValue, 4);
}

// ---------------------------------------------------------------------------
// CRequestObject::WriteString @ 0x82BD0648
// ---------------------------------------------------------------------------
int CRequestObject::WriteString(const char* pcString, int bPrepend)
{
    unsigned short lnLength = 0;
    if (pcString)
        lnLength = static_cast<unsigned short>(std::strlen(pcString));  // clrlwi 16

    if (bPrepend)
    {
        PrependToRequestBuffer(pcString, lnLength);
        unsigned short lnPrefix = lnLength;  // sth into the stack slot
        return PrependToRequestBuffer(&lnPrefix, 2);
    }

    unsigned short lnPrefix = lnLength;  // sth into the stack slot
    AppendToRequestBuffer(&lnPrefix, 2);
    return AppendToRequestBuffer(pcString, lnLength);
}

// ---------------------------------------------------------------------------
// CRequestObject::FinishBaseBlock @ 0x82BD04E8
//
//   if (bWriteTimestamp) { append tag 59; append NetOrder(clock);
//                          Instance()->GetTime() again, result discarded }
//   if (bSign)             append tag 30;
//   length = mnDataLength (+22 when signing); prepend length dword;
//   prepend the request-type byte;
//   if (bSign)           { digest = SHA1-HMAC(payload, key);
//                          append u16 20; append digest[20]; free digest }
//   prepend the protocol-version byte 4;
//
// (The asm's tail `if (mpDataBuffer) store mnBufferSize into the dead stack
//  slot` is a dead store to a local -- dropped.)
// ---------------------------------------------------------------------------
int CRequestObject::FinishBaseBlock(char bRequestType, int bWriteTimestamp, int bSign)
{
    if (bWriteTimestamp)
    {
        unsigned char lbTimeTag = 59;  // ';'
        AppendToRequestBuffer(&lbTimeTag, 1);

        long long lnTime = CMassiveClientCore::Instance()->GetTime();
        unsigned long long lnWireTime =
            MassiveNetOrderU64(static_cast<unsigned long long>(lnTime));  // bl STUB
        AppendToRequestBuffer(&lnWireTime, 8);

        // The X360 queries the clock a second time and discards the result
        // (a stripped benchmark/log read); the calls are reproduced.
        CMassiveClientCore::Instance()->GetTime();
    }

    if (bSign)
    {
        unsigned char lbSignTag = 30;
        AppendToRequestBuffer(&lbSignTag, 1);
    }

    int lnLength = mnDataLength;  // lwz 0x24
    if (bSign)
        lnLength += 22;  // addi 0x16 (tag byte + u16 length + 20-byte digest + version)
    PrependToRequestBuffer(&lnLength, 4);

    char lbRequestType = bRequestType;  // stb r29 into the stack slot
    PrependToRequestBuffer(&lbRequestType, 1);

    if (bSign)
    {
        int lnKeyLength = static_cast<int>(std::strlen(gacHMACKey));
        void* lpDigest =
            CalculateSHA1HMac(mpDataBuffer, mnDataLength, gacHMACKey, lnKeyLength);

        unsigned short lnDigestLength = 20;  // sth 0x14
        AppendToRequestBuffer(&lnDigestLength, 2);
        AppendToRequestBuffer(lpDigest, 20);  // appended before the null test (asm order)

        if (lpDigest)
            MassiveFree(lpDigest);
    }

    unsigned char lbVersion = 4;
    return PrependToRequestBuffer(&lbVersion, 1);
}

// ---------------------------------------------------------------------------
// CRequestObject::SetThirdPartyID @ 0x82BCF320
// ---------------------------------------------------------------------------
char* CRequestObject::SetThirdPartyID(const char* pcID)
{
    if (!pcID)
        return 0;

    std::size_t luLength = std::strlen(pcID);
    if (!luLength)
        return 0;

    char* lpCopy = static_cast<char*>(MassiveMalloc(luLength + 1));
    gpcThirdPartyID = lpCopy;  // stored before the null test
    if (!lpCopy)
        return 0;

    return std::strncpy(lpCopy, pcID, luLength + 1);
}

// ---------------------------------------------------------------------------
// CRequestObject::SetThirdPartyService @ 0x82BCF3E8
// ---------------------------------------------------------------------------
char* CRequestObject::SetThirdPartyService(const char* pcService)
{
    if (!pcService)
        return 0;

    std::size_t luLength = std::strlen(pcService);
    if (!luLength)
        return 0;

    char* lpCopy = static_cast<char*>(MassiveMalloc(luLength + 1));
    gpcThirdPartyService = lpCopy;  // stored before the null test
    if (!lpCopy)
        return 0;

    return std::strncpy(lpCopy, pcService, luLength + 1);
}

// ---------------------------------------------------------------------------
// CRequestObject::ClearThirdPartyService @ 0x82BCF460
// ---------------------------------------------------------------------------
void CRequestObject::ClearThirdPartyService()
{
    if (gpcThirdPartyService)
    {
        MassiveFree(gpcThirdPartyService);
        gpcThirdPartyService = 0;
    }
}

// ---------------------------------------------------------------------------
// CRequestObject::SetPublicKey @ 0x82BCF4B0
// ---------------------------------------------------------------------------
int CRequestObject::SetPublicKey(const void* pKey)
{
    if (!pKey)
        return 0;

    std::memcpy(gabPublicKey, pKey, 144);
    return 1;
}

} // namespace MassiveAdClient3
