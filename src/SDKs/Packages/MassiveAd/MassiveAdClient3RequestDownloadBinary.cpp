#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestDownloadBinary.h"

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"
// CalculateMD5Hash(data, len) is homed with the sibling CRequestLocateService
// (the MassiveAd crypto slice); include that header for its declaration rather
// than re-declaring the shared helper.
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestLocateService.h"

#include <cstdint>
#include <cstring>

// ===========================================================================
// MassiveAdClient3::CRequestDownloadBinary -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (no leak / DecFIGS).
//
// The vftable install the X360 ctor / vector deleting destructor perform
// (off_82187AD0) is modelled by the class virtuals, so no vftable store is
// written by hand. The verbose-trace call sites the X360 renders as Hex-Rays
// `STUB(level, mpcName, fmt, ...)` are the MassiveLog hook (r3 = level, r4 = the
// base name at +0x0C, r5 = format, then the args).
//
// The X360 reuses the inherited CRequestObject::mpSignature slot (+0x30) to hold
// the expected 32-byte MD5 digest: CreateRequest MassiveMalloc's it there
// (`stw r3, 0x30`) so the base ~CRequestObject frees it, and Parse memcmp's the
// received digest against it. The +0x48 base field (mnField48) carries the
// expected byte size.
// ===========================================================================

namespace MassiveAdClient3
{

// Size of the expected-MD5 digest block copied/compared by this request.
static const int KI_ExpectedMD5Bytes = 32;

// ---------------------------------------------------------------------------
// CRequestDownloadBinary::CRequestDownloadBinary @ 0x82BDD280
//
//   bl CRequestObject::CRequestObject(this, 0x54=84, "RequestDownloadBinary")
//   stw 0, 0x50(this)                    -> mpRequestURL = 0
//   stw off_82187AD0, 0(this)            -> install vftable (modelled by vtable)
// ---------------------------------------------------------------------------
CRequestDownloadBinary::CRequestDownloadBinary()
    : CRequestObject(84, "RequestDownloadBinary")
    , mpRequestURL(0)
{
}

// ---------------------------------------------------------------------------
// CRequestDownloadBinary::~CRequestDownloadBinary @ 0x82BDD2D8
//
//   *this = off_82187AD0                 -> rewrite vftable (modelled by vtable)
//   if (mpRequestURL) { MassiveFree(mpRequestURL); mpRequestURL = 0; }
//   chain ~CRequestObject                -> automatic
//
// (The `vector deleting destructor' @ 0x82BDD518 is the compiler thunk over this
// dtor; it is not written by hand.)
// ---------------------------------------------------------------------------
CRequestDownloadBinary::~CRequestDownloadBinary()
{
    if (mpRequestURL)
    {
        MassiveFree(mpRequestURL); // off_82F91C18(mpRequestURL) -- the free hook
        mpRequestURL = 0;
    }
}

// ---------------------------------------------------------------------------
// CRequestDownloadBinary::SetRequestURL @ 0x82BDD338
//
//   len = strlen(pcURL) + 1;
//   mpRequestURL = MassiveMalloc(len);   -> stw 0x50
//   if (mpRequestURL) return strncpy(mpRequestURL, pcURL, len);
//   return SetLastError(-99, "Could not allocate buffer to store request URL");
//
// The failure branch returns the SetLastError code in r3 (Hex-Rays typed the
// register as char*); the caller (CreateRequest) ignores the return, the load-
// bearing effect is the mnLastError store SetLastError performs.
// ---------------------------------------------------------------------------
char* CRequestDownloadBinary::SetRequestURL(const char* pcURL)
{
    int lnLength = static_cast<int>(strlen(pcURL)) + 1;

    mpRequestURL = static_cast<char*>(MassiveMalloc(lnLength));
    if (mpRequestURL)
        return strncpy(mpRequestURL, pcURL, lnLength);

    return reinterpret_cast<char*>(static_cast<std::intptr_t>(
        SetLastError(-99, "Could not allocate buffer to store request URL")));
}

// ---------------------------------------------------------------------------
// CRequestDownloadBinary::CreateRequest @ 0x82BDD3A8
//
//   if (!pBuilder || !pcURL || !pExpectedMD5) return -100;
//   SetRequestURL(pcURL);
//   mpSignature = MassiveMalloc(32);     -> stw 0x30 (reuse base signature slot)
//   if (!mpSignature) return -99;
//   memcpy(mpSignature, pExpectedMD5, 32);
//   mnField48 = nExpectedSize;           -> stw 0x48
//   CRequestObject::CreateRequest(pBuilder, 256, nExpectedSize);
//   SetStatus(2);
//   MassiveLog(5, GetName(), "Created request for %s", pcURL);
//   return 0;
// ---------------------------------------------------------------------------
int CRequestDownloadBinary::CreateRequest(CRequestBuilder* pBuilder, const char* pcURL,
                                          const void* pExpectedMD5, int nExpectedSize)
{
    if (!pBuilder || !pcURL || !pExpectedMD5)
        return -100; // li r3, -0x64

    SetRequestURL(pcURL);

    // Reuse the inherited mpSignature slot (+0x30) for the expected MD5, so the
    // base ~CRequestObject frees it on teardown.
    mpSignature = static_cast<unsigned char*>(MassiveMalloc(KI_ExpectedMD5Bytes));
    if (!mpSignature)
        return -99; // li r3, -0x63

    memcpy(mpSignature, pExpectedMD5, KI_ExpectedMD5Bytes);
    mnField48 = nExpectedSize; // stw r29, 0x48

    CRequestObject::CreateRequest(pBuilder, 256, nExpectedSize); // li r5, 0x100
    SetStatus(2);                                                // li r4, 2
    MassiveLog(5, GetName(), "Created request for %s", pcURL);
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestDownloadBinary::Parse @ 0x82BDD470
//
//   actual   = mnDataLength;   // +0x24
//   expected = mnField48;      // +0x48
//   if (expected != actual) {
//     MassiveLog(2, GetName(), "Expected size(%d) does not match actual size(%d)",
//                expected, actual);
//     return 0;
//   }
//   md5 = CalculateMD5Hash(mpDataBuffer, actual);   // +0x1C
//   if (memcmp(mpSignature, md5, 32)) {             // +0x30
//     MassiveLog(2, GetName(), "Expected md5 does not match actual md5");
//     return 0;
//   }
//   MassiveLog(5, GetName(), "Response successfully read and parsed.");
//   return 1;
// ---------------------------------------------------------------------------
int CRequestDownloadBinary::Parse()
{
    int lnActualSize   = mnDataLength; // lwz r7, 0x24
    int lnExpectedSize = mnField48;    // lwz r6, 0x48

    if (lnExpectedSize != lnActualSize)
    {
        MassiveLog(2, GetName(), "Expected size(%d) does not match actual size(%d)",
                   lnExpectedSize, lnActualSize);
        return 0;
    }

    char* pcActualMD5 = CalculateMD5Hash(mpDataBuffer, lnActualSize); // lwz r3, 0x1C
    if (memcmp(mpSignature, pcActualMD5, KI_ExpectedMD5Bytes))        // lwz r3, 0x30
    {
        MassiveLog(2, GetName(), "Expected md5 does not match actual md5");
        return 0;
    }

    MassiveLog(5, GetName(), "Response successfully read and parsed.");
    return 1;
}

} // namespace MassiveAdClient3
