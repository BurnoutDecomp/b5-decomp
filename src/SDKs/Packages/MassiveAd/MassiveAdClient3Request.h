#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestObject -- the MassiveAd client's protocol request
// (vendor middleware). One CRequestObject owns the raw wire buffer for a single
// client<->server transaction: the derived request classes (CRequestOpenSession,
// CRequestEnterZone, ...) build the outgoing block through the Write*/Finish
// helpers, the transport layers (CRequestManager / CNetworkManager /
// CTransactionHTTP) drive its status lifecycle, and the derived Parse()
// overrides consume the response through the Read* helpers.
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CRequestObject::CRequestObject          @ 0x82BCF1F8
//     MassiveAdClient3::CRequestObject::~CRequestObject         @ 0x82BCF280
//     MassiveAdClient3::CRequestObject::`scalar deleting destructor' @ 0x82BCFA18
//     MassiveAdClient3::CRequestObject::TakeDataOwnership       @ 0x82BCF2F8
//     MassiveAdClient3::CRequestObject::SetThirdPartyID         @ 0x82BCF320
//     MassiveAdClient3::CRequestObject::SetThirdPartyService    @ 0x82BCF3E8
//     MassiveAdClient3::CRequestObject::ClearThirdPartyService  @ 0x82BCF460
//     MassiveAdClient3::CRequestObject::SetPublicKey            @ 0x82BCF4B0
//     MassiveAdClient3::CRequestObject::GetServerIndex          @ 0x82BCF4F0
//     MassiveAdClient3::CRequestObject::IsPending               @ 0x82BCF500
//     MassiveAdClient3::CRequestObject::IsActive                @ 0x82BCF510
//     MassiveAdClient3::CRequestObject::IsComplete              @ 0x82BCF528
//     MassiveAdClient3::CRequestObject::VerifyHMACSignature     @ 0x82BCF538
//     MassiveAdClient3::CRequestObject::ReadU8                  @ 0x82BCF5D0
//     MassiveAdClient3::CRequestObject::ReadU16                 @ 0x82BCF618
//     MassiveAdClient3::CRequestObject::ReadU32                 @ 0x82BCF6A0
//     MassiveAdClient3::CRequestObject::ReadFloat               @ 0x82BCF7B0
//     MassiveAdClient3::CRequestObject::ReadU32Array            @ 0x82BCF840
//     MassiveAdClient3::CRequestObject::ReadByteArray           @ 0x82BCF908
//     MassiveAdClient3::CRequestObject::SkipField               @ 0x82BCF9C0
//     MassiveAdClient3::CRequestObject::ResetDataBuffer         @ 0x82BCFA68
//     MassiveAdClient3::CRequestObject::AllocateDataBuffer      @ 0x82BCFAD8
//     MassiveAdClient3::CRequestObject::ReAllocateDataBuffer    @ 0x82BCFB98
//     MassiveAdClient3::CRequestObject::AppendToRequestBuffer   @ 0x82BCFC50
//     MassiveAdClient3::CRequestObject::PrependToRequestBuffer  @ 0x82BCFD10
//     MassiveAdClient3::CRequestObject::RemoveFromRequestBuffer @ 0x82BCFDF0
//     MassiveAdClient3::CRequestObject::Submit                  @ 0x82BCFF70
//     MassiveAdClient3::CRequestObject::Error                   @ 0x82BD0008
//     MassiveAdClient3::CRequestObject::Cancel                  @ 0x82BD0070
//     MassiveAdClient3::CRequestObject::Suspend                 @ 0x82BD00E8
//     MassiveAdClient3::CRequestObject::Resume                  @ 0x82BD00F0
//     MassiveAdClient3::CRequestObject::WriteU8                 @ 0x82BD00F8
//     MassiveAdClient3::CRequestObject::WriteU16                @ 0x82BD0138
//     MassiveAdClient3::CRequestObject::WriteU32                @ 0x82BD0178
//     MassiveAdClient3::CRequestObject::WriteU64                @ 0x82BD01B8
//     MassiveAdClient3::CRequestObject::WriteFloat              @ 0x82BD0220
//     MassiveAdClient3::CRequestObject::ReadRemoveVerifyProtocolVersion @ 0x82BD0280
//     MassiveAdClient3::CRequestObject::ReadString              @ 0x82BD0330
//     MassiveAdClient3::CRequestObject::CreateRequest           @ 0x82BD03E0
//     MassiveAdClient3::CRequestObject::Complete                @ 0x82BD0468
//     MassiveAdClient3::CRequestObject::FinishBaseBlock         @ 0x82BD04E8
//     MassiveAdClient3::CRequestObject::WriteString             @ 0x82BD0648
// (SetStatus and ReadRemoveSignature are CRequestObject functions attributed to
//  a separate ledger slice; SetStatus is declared below because this TU's
//  bodies call it, ReadRemoveSignature is not declared -- its signature is not
//  attested by this TU's asm.)
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestObject class / its methods) are PRESERVED VERBATIM
// -- external middleware API, not project-owned code.
//
// Layout (members BY NAME over the 0x14-byte CMassiveBaseObject base; X360-
// absolute offsets are reproduced by member name, not asserted on the 64-bit
// host where the base and the pointers are wider; the +0x34 hole is the X360's
// natural 8-byte alignment padding ahead of the int64 at +0x38, reproduced here
// by the same long long member):
//   +0x00  vftable        (off_82184858; slot 0 = scalar deleting destructor,
//                          slot 1 = the virtual Parse() Complete dispatches)
//   +0x14  mnServerIndex  (ctor a2; GetServerIndex returns its low nibble)
//   +0x18  mnStatus       (1 at ctor; nibble bands: &0xF0 pending, ==0x10
//                          active, &0xF000 complete/error, &0xF00000 cancelled)
//   +0x1C  mpDataBuffer   (MassiveMalloc'd wire buffer, or 0)
//   +0x20  mnPosition     (read/write cursor into the buffer)
//   +0x24  mnDataLength   (live payload bytes)
//   +0x28  mnBufferSize   (allocated capacity; <= 0 marks the buffer invalid)
//   +0x2C  mpBuilder      (owning CRequestBuilder; completion/error callbacks)
//   +0x30  mpSignature    (owned 20-byte response HMAC block, or 0)
//   +0x38  mnField38      (int64, zeroed by the ctor; untouched in this TU)
//   +0x40  mnField40      (0 at ctor; untouched in this TU)
//   +0x44  mbOwnsData     (1 at ctor; the dtor frees mpDataBuffer only while
//                          set, TakeDataOwnership clears it)
//   +0x48  mnField48      (50 at ctor; CreateRequest overwrites it with its a4)
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

namespace MassiveAdClient3
{

class CRequestBuilder;

// ---------------------------------------------------------------------------
// MassiveAd HMAC helper (separate TU).
//
// CalculateSHA1HMac(data, dataLength, key, keyLength) returns a freshly
// MassiveMalloc'd 20-byte SHA1-HMAC digest (freed by the caller through the
// MassiveAd heap hook), or null on failure. The X360 symbol demangles WITHOUT
// the MassiveAdClient3 namespace, but it is declared inside the namespace here
// so the whole vendor package stays self-contained (link-name fidelity is not a
// gate for this reconstruction). Body lives in the MassiveAd crypto TU.
// ---------------------------------------------------------------------------
void* CalculateSHA1HMac(const void* pData, int nDataLength,
                        const char* pcKey, int nKeyLength);

// ---------------------------------------------------------------------------
// MassiveAd wire byte-order hooks (FLAGGED).
//
// The X360 routes every 64-bit and float wire value through a helper Hex-Rays
// renders as the placeholder `STUB` (WriteU64/WriteFloat/FinishBaseBlock before
// appending, ReadFloat after extracting). The call sites prove the value passes
// through in-register (the result is stored straight back over the input), so on
// this big-endian build the helper is an identity host<->network byte-order
// shim (the classic hton64/htonf family folded to a no-op body). FLAG: the
// NAMES and the byte-order INTENT are supplied; the identity behaviour is the
// attested fact. Defined (as identities) in MassiveAdClient3Request.cpp.
// ---------------------------------------------------------------------------
unsigned long long MassiveNetOrderU64(unsigned long long nValue);
float              MassiveNetOrderFloat(float fValue);

// ---------------------------------------------------------------------------
// Protocol .data shared with the rest of the MassiveAd client (this TU's slice
// of the 0x8327F2A0.. block). Defined in MassiveAdClient3Request.cpp.
// ---------------------------------------------------------------------------

// byte_8327F2A0 -- the NUL-terminated HMAC key string (strlen'd by both
// VerifyHMACSignature and FinishBaseBlock). Written elsewhere in the client
// (the session-key path); FLAG: the 36-byte capacity is inferred from the .data
// gap to the next slot (0x8327F2A0..0x8327F2C4), not from an attested store.
extern char gacHMACKey[36];

// dword_8327F2C4 / dword_8327F2C8 -- the third-party ID / service strings
// (owned MassiveMalloc'd copies installed by the Set* helpers below, or 0).
extern char* gpcThirdPartyID;
extern char* gpcThirdPartyService;

// dword_8327F2CC / dword_8327F2D0 -- the client-wide MassiveAd session / player
// IDs, stamped into every outgoing request block (e.g. CRequestExitZone::
// WriteExitZoneRequest writes gnMassivePlayerID under wire tag 42 and
// gnMassiveSessionID under tag 43). Written elsewhere in the client (the
// session-open path); read here by the request writers.
extern unsigned int gnMassiveSessionID; // dword_8327F2CC
extern unsigned int gnMassivePlayerID;  // dword_8327F2D0

// unk_8327F2D8 -- the 144-byte public-key block SetPublicKey memcpy's in.
extern unsigned char gabPublicKey[144];

// ---------------------------------------------------------------------------
// CRequestObject -- base class of every MassiveAd protocol request.
// ---------------------------------------------------------------------------
class CRequestObject : public CMassiveBaseObject
{
    // CRequestManager::PrepareForShutdown inspects a queued request's stored
    // index/type word (mnServerIndex at +0x14) directly to decide whether to
    // cancel it during shutdown. Granting friendship keeps that a named-member
    // read rather than an offset hack; the manager owns the queue of requests.
    friend class CRequestManager;

public:
    // @ 0x82BCF1F8. Chains the base ctor with the derived class's name, installs
    // this class's vftable (off_82184858 -- modelled by the virtuals), stores the
    // server index, and defaults the whole request state: status 1, no buffer,
    // no builder, no signature, owns-data set, mnField48 = 50.
    CRequestObject(int nServerIndex, const char* pcName);

    // @ 0x82BCF280 (chained by the scalar deleting destructor @ 0x82BCFA18).
    // Rewrites the vftable, frees the wire buffer through the MassiveAd heap
    // hook iff the request still owns it (mbOwnsData), frees the signature
    // block, then chains the base dtor.
    virtual ~CRequestObject();

    // Vftable slot 1: the response parser Complete() dispatches virtually on
    // itself. Every concrete request (CRequestOpenSession, CRequestEnterZone,
    // ...) overrides it in its own TU; the base slot is the pure stub (no
    // CRequestObject::Parse body exists in the X360 ledger). Returns non-zero on
    // a successfully parsed response.
    virtual int Parse() = 0;

    // Vftable slot 2 (+0x08): the per-request-type server endpoint / URL getter.
    // Attested as a slot-2 virtual by CMassiveAsset::HandleError / HandleResponse
    // (both call `(*(*request + 8))(request)` and log the returned string), and
    // overridden by CRequestImpressionUpdate::GetRequestURL. Non-pure here (each
    // concrete request supplies its own; the base body lives in its own ledger
    // slice) so the existing request subclasses that do not name an endpoint stay
    // instantiable. Reproduced from the asm, not the pseudocode (which renders the
    // virtual as a direct call).
    virtual const char* GetRequestURL();

    // ----- lifecycle ------------------------------------------------------

    // @ 0x82BD03E0. Wires the request to its owning builder, records the a4
    // dword into mnField48, registers with the builder's request collection,
    // and allocates the raw wire buffer. Returns -1100 on a null builder, the
    // -99 SetLastError result on a failed allocation, else 0.
    int CreateRequest(CRequestBuilder* pBuilder, int nBufferSize, int nField48);

    // @ 0x82BCFF70. Submits the request to the CRequestManager singleton. Only
    // status 2 may submit (-1098 otherwise); the pre-submit status is fetched
    // from the builder (+0x24). A missing manager or a non-zero SubmitRequest
    // marks the request failed (status 0x2000, error -1097).
    int Submit();

    // @ 0x82BD0468. Marks the request complete (status 0x1000), runs the virtual
    // Parse(), routes a parse failure into Error(5), and -- unless the request
    // was cancelled (status & 0xF00000) -- notifies the builder via its slot-1
    // callback.
    int Complete();

    // @ 0x82BD0008. Marks the request failed (status 0x2000) and -- unless the
    // request was cancelled -- notifies the builder via its slot-2 callback with
    // the error code.
    int Error(int nErrorCode);

    // @ 0x82BD0070. Marks the request cancelled (status 0x103000) and removes it
    // from the CRequestManager singleton; when the manager answers -697 (not
    // queued) and this request is the network manager's in-flight one, it is
    // removed there instead. (The X360 r3 return is the incidental result of
    // whichever call ran last; modelled as void.)
    void Cancel();

    // @ 0x82BD00E8. Tail-calls SetStatus(0x20).
    int Suspend();

    // @ 0x82BD00F0. Tail-calls SetStatus(0x10).
    int Resume();

    // Status setter shared by the lifecycle paths above. NOT part of this TU's
    // ledger slice (separate dossier); declared here because Submit/Complete/
    // Error/Cancel/Suspend/Resume all bl into it.
    int SetStatus(int nStatus);

    // ----- status queries -------------------------------------------------

    // @ 0x82BCF4F0. Low nibble of the ctor's server-index word.
    int GetServerIndex();

    // Additive accessor (FLAG: not its own X360 function). The request-owning
    // subsystems dispatch on the FULL server-index/type word at +0x14 (e.g.
    // CMassiveZoneManager::HandleResponse/HandleError switch on it: EnterZone=0x33,
    // ExitZone=0x43, ImpressionUpdate=0x65 -- the same values the ctors pass, 51/
    // 67/101). GetServerIndex masks to the low nibble, so this exposes the whole
    // field BY NAME for the request-type switch.
    int GetServerType() const { return mnServerIndex; }

    // Additive accessors (FLAG: not their own X360 functions). CMassiveAsset::
    // HandleResponse reads the request's live payload length (mnDataLength at
    // +0x24) to reject an empty response and to feed CMassiveClientCore::
    // AddBenchmarkData, and reads the +0x38 field (mnField38, an i64) as the
    // second AddBenchmarkData argument. Both are attested reads in that TU's asm;
    // exposed here BY NAME so the asset does not reach into the protected members.
    int       GetDataLength() const { return mnDataLength; }
    long long GetField38() const { return mnField38; }

    // Additive accessors (FLAG: not their own X360 functions). CNetworkManager
    // drives a request through the wire directly on the X360: Send/Receive read the
    // raw wire buffer (mpDataBuffer at +0x1C) and its live length (mnDataLength),
    // and the state machine compares the exact status word (mnStatus at +0x18) to
    // the sending band 0x200 / receiving band 0x300. All are attested reads in the
    // CNetworkManager asm; exposed here BY NAME so the transport does not reach into
    // the protected members.
    int            GetStatus() const     { return mnStatus; }
    unsigned char* GetDataBuffer() const { return mpDataBuffer; }

    // @ 0x82BCF500. Non-zero while the status sits in the pending band (&0xF0).
    int IsPending();

    // @ 0x82BCF510. True (0/1) iff the status is exactly 0x10 (active).
    int IsActive();

    // @ 0x82BCF528. Non-zero once the status carries a completion nibble
    // (&0xF000: completed 0x1000 or failed 0x2000).
    int IsComplete();

    // ----- raw wire buffer ------------------------------------------------

    // @ 0x82BCFAD8. Allocates the wire buffer (rejects sizes <= 0 and double
    // allocation), zeroes cursor + length. Returns 0 on success, else the -99
    // SetLastError result.
    int AllocateDataBuffer(int nSize);

    // @ 0x82BCFB98. Grows the buffer to nNewSize + 64 bytes (copying the live
    // payload and freeing the old block); falls back to AllocateDataBuffer when
    // no buffer exists yet. Returns 0 on success, else the -99 SetLastError
    // result.
    int ReAllocateDataBuffer(int nNewSize);

    // @ 0x82BCFA68. Zero-fills the whole buffer and rewinds cursor + length.
    // Returns 0 on success, else the -99 SetLastError result.
    int ResetDataBuffer();

    // @ 0x82BCFC50. Copies nLength bytes at the cursor (growing on overflow) and
    // advances length + cursor. Returns nLength, or -1 on any failure.
    int AppendToRequestBuffer(const void* pData, int nLength);

    // @ 0x82BCFD10. Shifts the live payload up by nLength (byte-by-byte, high to
    // low; growing on overflow) and copies the new bytes to the front. Returns
    // nLength, -99 on a failed grow, or -1 on invalid arguments.
    int PrependToRequestBuffer(const void* pData, int nLength);

    // @ 0x82BCFDF0. Removes nLength bytes at nOffset (byte-by-byte compaction),
    // shrinking the length and pulling the cursor back when it sat past the
    // removed range. Returns nLength, or -1 on any failure.
    int RemoveFromRequestBuffer(int nOffset, int nLength);

    // @ 0x82BCF2F8. Hands the wire buffer to the caller: clears mbOwnsData (so
    // the dtor no longer frees it) and returns the buffer, or 0 when ownership
    // was already taken.
    unsigned char* TakeDataOwnership();

    // ----- response readers (all cursor-relative, all bounds-checked) ------

    // @ 0x82BCF5D0. Next byte (0 when the buffer is invalid/exhausted).
    int ReadU8();

    // @ 0x82BCF618. Next 2 bytes (0 when invalid/exhausted).
    int ReadU16();

    // @ 0x82BCF6A0. Next 4 bytes (0 when invalid/exhausted).
    unsigned int ReadU32();

    // @ 0x82BCF7B0. Next 4 bytes through the float byte-order hook (0.0f when
    // invalid/exhausted).
    float ReadFloat();

    // Next 8 bytes through the 64-bit byte-order hook (0 when invalid/exhausted);
    // the read mirror of WriteU64. NOT part of this header's original ledger
    // slice, but CRequestObject is its home -- declared here because
    // CRequestLocateService::Parse reads the server-time field with it (attested
    // by a named `bl ...CRequestObject::ReadU64` in that TU's asm). Body lives in
    // the base reader slice.
    unsigned long long ReadU64();

    // @ 0x82BD0330. Reads a u16 length then MassiveMalloc's a NUL-terminated
    // copy of that many bytes. Returns the owned string, or 0 on any failure.
    char* ReadString();

    // @ 0x82BCF840. Reads a u16 count into *pnCount then MassiveMalloc's and
    // fills *ppValues with count u32s (null on a zero count; untouched on a
    // bounds failure -- the X360 checks the remaining BYTES against the COUNT,
    // not count*4, reproduced verbatim). The X360 r3 return is incidental;
    // modelled as void.
    void ReadU32Array(unsigned int** ppValues, unsigned short* pnCount);

    // @ 0x82BCF908. Reads a u16 length into *pnLength then MassiveMalloc's and
    // fills *ppData with that many raw bytes (null on a zero length; untouched
    // on a bounds failure). The X360 r3 return is incidental; modelled as void.
    void ReadByteArray(unsigned char** ppData, unsigned short* pnLength);

    // @ 0x82BCF9C0. Field types >= 200 carry a u32 payload size: consume the
    // size dword and skip that many payload bytes (returns 1). Anything below
    // 200 is left for the caller (returns 0).
    int SkipField(unsigned char bFieldType);

    // @ 0x82BD0280. Reads the protocol-version byte, removes it from the buffer
    // (at the pre-read cursor), and returns whether it is version 4.
    int ReadRemoveVerifyProtocolVersion();

    // @ 0x82BCF538. Recomputes the SHA1-HMAC over the live payload with the
    // client HMAC key and compares it against the 20-byte received signature
    // (mpSignature). Returns 1 on a match, 0 on a mismatch or failed digest.
    int VerifyHMACSignature();

    // ----- request writers (bPrepend routes front/back) --------------------

    // @ 0x82BD00F8 / 0x82BD0138 / 0x82BD0178. Fixed-width scalar writes.
    int WriteU8(char bValue, int bPrepend);
    int WriteU16(short nValue, int bPrepend);
    int WriteU32(int nValue, int bPrepend);

    // @ 0x82BD01B8 / 0x82BD0220. 64-bit / float writes through the byte-order
    // hooks.
    int WriteU64(unsigned long long nValue, int bPrepend);
    int WriteFloat(float fValue, int bPrepend);

    // @ 0x82BD0648. Length-prefixed string write: a u16 strlen then the bytes
    // (prepend order: bytes first, then the length in front of them).
    int WriteString(const char* pcString, int bPrepend);

    // @ 0x82BD04E8. Seals the request block: optional timestamp field (tag 59 +
    // the client clock through the 64-bit byte-order hook), optional signature
    // tag 30, then prepends the total length dword (+22 when signed) and the
    // request-type byte, appends the 20-byte SHA1-HMAC (u16 length 20 + digest)
    // when signing, and finally prepends the protocol-version byte 4.
    int FinishBaseBlock(char bRequestType, int bWriteTimestamp, int bSign);

    // ----- client-wide protocol state (static; .data slice above) ----------

    // @ 0x82BCF320. Installs an owned copy of pcID into gpcThirdPartyID.
    // Returns the copy, or 0 on a null/empty string or failed allocation.
    static char* SetThirdPartyID(const char* pcID);

    // @ 0x82BCF3E8. Installs an owned copy of pcService into
    // gpcThirdPartyService. Returns the copy, or 0 as above.
    static char* SetThirdPartyService(const char* pcService);

    // @ 0x82BCF460. Frees gpcThirdPartyService through the MassiveAd heap hook
    // and nulls it. (The X360 r3 return is incidental; modelled as void.)
    static void ClearThirdPartyService();

    // Symmetric clear for gpcThirdPartyID (separate ledger TU; body lives in the
    // MassiveAd third-party-state slice). Declared here -- CRequestObject is its
    // home -- because CRequestManager::Shutdown bl's into it. (The X360 r3 return
    // is incidental; modelled as void.)
    static void ClearThirdPartyID();

    // @ 0x82BCF4B0. Copies the 144-byte public-key block into gabPublicKey.
    // Returns 1, or 0 on a null source.
    static int SetPublicKey(const void* pKey);

protected:
    // The derived request TUs (CRequestOpenSession::Parse and friends) walk the
    // same buffer state directly on the X360, so the members are protected
    // rather than private.
    int            mnServerIndex; // +0x14 (ctor a2; GetServerIndex low nibble)
    int            mnStatus;      // +0x18 (1 at ctor)
    unsigned char* mpDataBuffer;  // +0x1C (MassiveMalloc'd wire buffer, or 0)
    int            mnPosition;    // +0x20 (read/write cursor)
    int            mnDataLength;  // +0x24 (live payload bytes)
    int            mnBufferSize;  // +0x28 (allocated capacity)
    CRequestBuilder* mpBuilder;   // +0x2C (owning builder, or 0)
    unsigned char* mpSignature;   // +0x30 (owned 20-byte response HMAC, or 0)
    long long      mnField38;     // +0x38 (0 at ctor; untouched in this TU)
    int            mnField40;     // +0x40 (0 at ctor; untouched in this TU)
    int            mbOwnsData;    // +0x44 (1 at ctor; cleared by TakeDataOwnership)
    int            mnField48;     // +0x48 (50 at ctor; CreateRequest's a4)
};

} // namespace MassiveAdClient3
