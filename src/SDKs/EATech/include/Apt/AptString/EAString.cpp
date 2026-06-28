// ===========================================================================
// EATech Apt -- EAStringC core out-of-line bodies.
//
// DECOMPILED from the PS3 EXTERNAL ELF (full GCC-mangled Apt symbol table) and
// cross-checked against X360 ARTIST. Addresses below are the PS3 External
// (_ZN9EAStringC...) offsets. This brings the foundational ref-counted string to
// life: construction/copy/destruction, the refcount machinery, the
// allocate/init/change-buffer path, the case-insensitive hash, length/c_str/
// compare/assign. The large text-manipulation suite (Append/Format/Insert/
// Replace/Trim*/Make*/Find*/UTF8_*/Left/Right/Mid/StartWith*/EndWith*/operator+)
// stays DECLARED-only in the header and is bodied on demand as an engine path
// needs it (leaf-first) -- the linker only needs bodies for what is actually
// called.
//
// TWO PC-PORT FLAGS (faithful-shape, leaf-localised):
//  1. ATOMIC REFCOUNT: the console stores the refcount in the HIGH 16 bits of the
//     first 32-bit word (big-endian m_uRefCount-first) and mutates it with an
//     lwarx/stwcx. atomic (+/-0x10000). PC is little-endian + single-threaded, so
//     the named m_uRefCount field IS the faithful equivalent and a plain ++/-- is
//     used. Marked // FLAG at each site.
//  2. POOL NOT YET WIRED: buffer alloc routes through gpNonGCPoolManager (the
//     non-GC Apt value pool), which AptInit has not built yet (it is null until
//     then -- see AptDefine.cpp). Until AptInit lands, the leaf falls back to the
//     C runtime heap (the established PC-IO-at-the-leaf pattern, as used by the
//     movie pool / debug-font pool). Once AptInit wires gpNonGCPoolManager the
//     fallback is dead. Marked // FLAG at each site.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptString/EAString.h"

#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"        // DOGMA_PoolManager::Allocate/Deallocate

#include <cstring>   // strlen/strcmp/memcpy/memmove/memset/memcmp
#include <cstdlib>   // malloc/free (the AptInit-not-yet-wired bring-up fallback)
#include <string.h>  // _stricmp (MSVC strcasecmp, for the case-insensitive compares)

// ---------------------------------------------------------------------------
// Statics.
//   s_EmptyInternalData : the shared empty string. 8-byte StringDataC header
//   (refcount/size/maxsize/hash, all zero) + 1 null terminator byte. Zero-init,
//   so c_str() on a default string returns a valid "" and every field reads 0.
//   sAptCallbacks       : the Apt user-function table, recorded by MemInitialize
//   (used by Format/vsFormat); null until then.
// ---------------------------------------------------------------------------
char              EAStringC::s_EmptyInternalData[8 + 1] = { 0 };
AptUserFunctions* EAStringC::sAptCallbacks              = 0;

// ---------------------------------------------------------------------------
// Refcount machinery.  PS3 External 0x7E9B20 / 0x7F49B8.
// ---------------------------------------------------------------------------
void EAStringC::IncreaseInternalRefCount()
{
    // FLAG: console = lwarx/stwcx. atomic +0x10000 on the first 32-bit word (the
    // high-half holds m_uRefCount on big-endian); PC = plain ++ on the named field.
    if (m_pData != reinterpret_cast<DebugDataC*>(&s_EmptyInternalData))
        ++m_pData->m_uRefCount;
}

void EAStringC::DecreaseInternalRefCount(StringDataC* const pData)
{
    if (pData == reinterpret_cast<StringDataC*>(&s_EmptyInternalData))
        return;
    // FLAG: console = lwarx/stwcx. atomic -0x10000, then test the high-half ==0;
    // PC = plain -- on the named field.
    if (--pData->m_uRefCount == 0)
    {
        // Block size = header(8) + maxsize + null(1) = m_uMaxSize + 9, matching
        // AllocateBuffer's (len+12)&~3 rounding.
        const size_t nAllocatedSize = static_cast<size_t>(pData->m_uMaxSize) + 9;
        if (gpNonGCPoolManager)
            gpNonGCPoolManager->Deallocate(pData, nAllocatedSize);
        else
            ::free(pData);   // FLAG: bring-up fallback until AptInit wires the pool
    }
}

// ---------------------------------------------------------------------------
// Buffer allocate / deallocate / init.  PS3 External 0x7EFDCC / 0x7EC724 / 0x7EFE2C.
// ---------------------------------------------------------------------------
void EAStringC::AllocateBuffer(const uint32_t uSize)
{
    // Round the total block (8 header + uSize chars + 1 null) up to a 4-byte
    // multiple; the usable max char count is the block minus header+null (9).
    const uint32_t nBlock   = (uSize + 12) & 0xFFFFFFFCu;
    const uint16_t uMaxSize = static_cast<uint16_t>(nBlock - 9);

    DebugDataC* result;
    if (gpNonGCPoolManager)
        result = static_cast<DebugDataC*>(gpNonGCPoolManager->Allocate(nBlock));
    else
        result = static_cast<DebugDataC*>(::malloc(nBlock));   // FLAG: bring-up fallback

    m_pData              = result;
    result->m_uRefCount  = 1;
    m_pData->m_uMaxSize  = uMaxSize;
}

void EAStringC::DeallocateBuffer(const void* const pBuffer, const uint32_t uSize)
{
    if (gpNonGCPoolManager)
        gpNonGCPoolManager->Deallocate(const_cast<void*>(pBuffer), uSize);
    else
        ::free(const_cast<void*>(pBuffer));   // FLAG: bring-up fallback
}

void EAStringC::InitFromBuffer(const char* const pStrText)
{
    if (*pStrText)
    {
        const uint32_t uLen = static_cast<uint32_t>(strlen(pStrText));
        AllocateBuffer(uLen);
        m_pData->m_uSize = static_cast<uint16_t>(uLen);
        m_pData->m_uHash = 0;
        memcpy(GetInternalBuffer(), pStrText, uLen + 1);
    }
    else
    {
        m_pData = reinterpret_cast<DebugDataC*>(&s_EmptyInternalData);
        IncreaseInternalRefCount();
    }
}

// The grow/copy-on-write workhorse behind ReserveSize/Append/Insert/etc.
// PS3 External 0x7F49FC. In-place when we exclusively own the block (refcount==1)
// and it already fits; otherwise reallocate (with 12.5% slack) and copy-out, or
// drop to the empty string when reserving zero.
void EAStringC::ChangeBuffer(const uint32_t uSizeToReserve, const uint32_t uOffsetCopy,
                             const uint32_t uSizeCopy, const CBPushZero ePushZero,
                             const uint32_t uInternalSize)
{
    DebugDataC* const old = m_pData;

    if (m_pData->m_uRefCount == 1 && m_pData->m_uMaxSize >= uSizeToReserve)
    {
        // In-place.
        if (uOffsetCopy)
        {
            char* const buf = GetInternalBuffer();
            memmove(buf, buf + uOffsetCopy, uSizeCopy);
        }
        m_pData->m_uSize = static_cast<uint16_t>(uInternalSize);
        m_pData->m_uHash = 0;
        if (ePushZero)
            GetInternalBuffer()[uInternalSize] = 0;
        return;
    }

    if (uSizeToReserve)
    {
        // Reallocate (copy-on-write or grow); the source bytes come from the old
        // block, captured before AllocateBuffer re-points m_pData.
        const char* const oldBuf = reinterpret_cast<const char*>(old) + sizeof(StringDataC);
        AllocateBuffer(uSizeToReserve + (uSizeToReserve >> 3));
        m_pData->m_uSize = static_cast<uint16_t>(uInternalSize);
        m_pData->m_uHash = 0;
        memcpy(GetInternalBuffer(), oldBuf + uOffsetCopy, uSizeCopy);
        if (ePushZero)
            GetInternalBuffer()[uInternalSize] = 0;
        DecreaseInternalRefCount(old);
    }
    else
    {
        m_pData = reinterpret_cast<DebugDataC*>(&s_EmptyInternalData);
        IncreaseInternalRefCount();
        DecreaseInternalRefCount(old);
    }
}

// ---------------------------------------------------------------------------
// Out-of-line constructors (the inline default/copy/const-char*/EmptyString
// ctors live in the header).  PS3 External 0x7F00D4 / 0x7EFFB4.
// ---------------------------------------------------------------------------
EAStringC::EAStringC(const uint32_t uReservedLength)
{
    if (uReservedLength)
    {
        AllocateBuffer(uReservedLength);
        m_pData->m_uSize     = 0;
        m_pData->m_uHash     = 0;
        GetInternalBuffer()[0] = 0;
    }
    else
    {
        m_pData = reinterpret_cast<DebugDataC*>(&s_EmptyInternalData);
        IncreaseInternalRefCount();
    }
}

EAStringC::EAStringC(const uint32_t cChar, const uint32_t uLength)
{
    if (uLength)
    {
        AllocateBuffer(uLength);
        memset(GetInternalBuffer(), static_cast<int>(cChar), uLength);
        m_pData->m_uSize           = static_cast<uint16_t>(uLength);
        m_pData->m_uHash           = 0;
        GetInternalBuffer()[uLength] = 0;
    }
    else
    {
        m_pData = reinterpret_cast<DebugDataC*>(&s_EmptyInternalData);
        IncreaseInternalRefCount();
    }
}

// ---------------------------------------------------------------------------
// Length / buffer accessors.  PS3 External 0x7EE6C8 / 0x7E6AFC / 0x7E6DC0 /
// 0x7EE6D4 / 0x7DEB6C.
// ---------------------------------------------------------------------------
uint32_t    EAStringC::GetLength()    const { return m_pData->m_uSize; }
const char* EAStringC::c_str()        const { return GetInternalBuffer(); }
const char* EAStringC::ConstRawPtr()  const { return GetInternalBuffer(); }
int         EAStringC::Size()         const { return m_pData->m_uSize; }
bool        EAStringC::IsEmpty()      const { return m_pData == reinterpret_cast<DebugDataC*>(&s_EmptyInternalData); }

uint32_t EAStringC::GetInternalSize()     const { return m_pData->m_uSize; }
uint32_t EAStringC::GetInternalMaxSize()  const { return m_pData->m_uMaxSize; }
uint32_t EAStringC::GetInternalRefCount() const { return m_pData->m_uRefCount; }

void EAStringC::SetInternalSize(const uint32_t uSize)         { m_pData->m_uSize     = static_cast<uint16_t>(uSize); }
void EAStringC::SetInternalMaxSize(const uint32_t uMaxSize)   { m_pData->m_uMaxSize  = static_cast<uint16_t>(uMaxSize); }
void EAStringC::SetInternalRefCount(const uint32_t uRefCount) { m_pData->m_uRefCount = static_cast<uint16_t>(uRefCount); }

// ---------------------------------------------------------------------------
// Hash: a case-insensitive FNV-1a folded to 16 bits, with a non-zero sentinel
// (ZERO_HASH) so 0 always means "not yet computed".  PS3 External 0x7DF9EC (the
// static char* form), 0x7E69A4 (member, stores into m_uHash), 0x7F8CA0 (lazy),
// 0x7DEBBC (get), 0x7DEC14 (invalidate).
// ---------------------------------------------------------------------------
uint16_t EAStringC::CalculateHashValue(const char* const pText)
{
    unsigned char c = static_cast<unsigned char>(*pText);
    if (c == 0)
        return 40389;   // 0x9DC5 = low-16 of the FNV offset basis (empty-string hash)

    const char* p     = pText + 1;
    uint32_t    hash  = 0x811C9DC5u;   // FNV offset basis
    uint32_t    x     = 0;
    do
    {
        if (static_cast<unsigned>(c - 'A') <= 25u)   // ASCII A-Z -> lowercase
            c = static_cast<unsigned char>(c + 32);
        x    = hash ^ c;
        c    = static_cast<unsigned char>(*p++);
        hash = 0x01000193u * x;        // FNV prime (unused after the final char)
    }
    while (c);

    const uint16_t folded = static_cast<uint16_t>(403u * x);
    return folded ? folded : static_cast<uint16_t>(ZERO_HASH);
}

void EAStringC::CalculateHashValue() const
{
    m_pData->m_uHash = CalculateHashValue(GetInternalBuffer());
}

uint16_t EAStringC::UpdateHashValue() const
{
    if (!m_pData->m_uHash)
        m_pData->m_uHash = CalculateHashValue(GetInternalBuffer());
    return m_pData->m_uHash;
}

uint16_t EAStringC::GetHashValue()      const { return m_pData->m_uHash; }
void     EAStringC::InvalidateHashValue() const { m_pData->m_uHash = 0; }

// ---------------------------------------------------------------------------
// Capacity.  PS3 External 0x7F5414 / 0x7F54E0 / 0x7E87A0.
// ---------------------------------------------------------------------------
void EAStringC::ReserveSize(const uint32_t uSize)
{
    uint32_t uKeep = m_pData->m_uSize;
    if (uKeep > uSize)
        uKeep = uSize;
    ChangeBuffer(uSize, 0, uKeep, CB_PUSH_ZERO, uKeep);
}

void EAStringC::Reserve(const int32_t iSize) { ReserveSize(static_cast<uint32_t>(iSize)); }

bool EAStringC::IsEnoughSize(const uint32_t uSize) const { return m_pData->m_uMaxSize >= uSize; }

// ---------------------------------------------------------------------------
// Clear / Duplicate.  PS3 External 0x7F5728 / 0x7F5438.
// ---------------------------------------------------------------------------
void EAStringC::Clear()
{
    DecreaseInternalRefCount(m_pData);
    m_pData = reinterpret_cast<DebugDataC*>(&s_EmptyInternalData);
    IncreaseInternalRefCount();
}

EAStringC& EAStringC::Duplicate(const EAStringC& strText)
{
    const uint16_t uSize = strText.m_pData->m_uSize;
    ReserveSize(uSize);
    char* const buf = GetInternalBuffer();
    memcpy(buf, strText.GetInternalBuffer(), uSize);
    buf[uSize]        = 0;
    m_pData->m_uSize  = uSize;
    m_pData->m_uHash  = strText.m_pData->m_uHash;
    return *this;
}

// ---------------------------------------------------------------------------
// Assignment.  PS3 External 0x7F54E8 / 0x7E2AC4.
// ---------------------------------------------------------------------------
EAStringC& EAStringC::operator=(const EAStringC& strText)
{
    // Inc source before dec self -> self-assignment safe.
    const_cast<EAStringC&>(strText).IncreaseInternalRefCount();
    DecreaseInternalRefCount(m_pData);
    m_pData = strText.m_pData;
    return *this;
}

EAStringC& EAStringC::operator=(const StaticStringHelperT& strStruct)
{
    // A compile-time constant string: point straight at the static StringDataC
    // (no refcount traffic). The buffer follows sd in StaticStringHelperT.
    m_pData = reinterpret_cast<DebugDataC*>(const_cast<StringDataC*>(&strStruct.sd));
    return *this;
}

// ---------------------------------------------------------------------------
// Comparison / indexing.  PS3 External 0x7EE6E0 / 0x7E6B28 / 0x7EEA5C / 0x7FC7A4 /
// 0x7E6B0C / 0x7E6BDC / 0x7EEA38 / 0x7E6C54 / 0x7E6C84.
// ---------------------------------------------------------------------------
bool EAStringC::operator==(const EAStringC& strText) const
{
    if (m_pData->m_uSize != strText.m_pData->m_uSize)
        return false;
    if (m_pData == strText.m_pData)
        return true;
    return memcmp(GetInternalBuffer(), strText.GetInternalBuffer(), m_pData->m_uSize) == 0;
}

bool EAStringC::operator==(const char* const pStrText) const
{
    return strcmp(GetInternalBuffer(), pStrText) == 0;
}

bool EAStringC::operator!=(const EAStringC& strText) const { return !(*this == strText); }
bool EAStringC::operator!=(const char* const pStrText) const
{
    return strcmp(GetInternalBuffer(), pStrText) != 0;
}

uint32_t EAStringC::operator[](const int32_t index) const
{
    return static_cast<uint32_t>(static_cast<int32_t>(GetInternalBuffer()[index]));
}

bool EAStringC::Equal(const char* const pStrText) const { return strcmp(GetInternalBuffer(), pStrText) == 0; }
bool EAStringC::Equal(const EAStringC& strText)   const { return *this == strText; }

int32_t EAStringC::Compare(const char* const pStrText) const { return strcmp(GetInternalBuffer(), pStrText); }
int32_t EAStringC::Compare(const EAStringC& strText) const
{
    if (m_pData == strText.m_pData)
        return 0;
    return strcmp(GetInternalBuffer(), strText.GetInternalBuffer());
}

// Case-insensitive compares (console strcasecmp -> MSVC _stricmp).  PS3 External
// 0x7E6C18 / 0x7EEA88 / 0x7E6CD4 / 0x7E6D04 / 0x7E7F88. The NoCase hash form is
// the AptNativeHash probe: same buffer -> equal; different cached (case-folded)
// hash -> not equal; otherwise confirm with _stricmp.
bool EAStringC::EqualNoCase(const char* const pStrText) const
{
    return _stricmp(GetInternalBuffer(), pStrText) == 0;
}

bool EAStringC::EqualNoCase(const EAStringC& strText) const
{
    if (m_pData->m_uSize != strText.m_pData->m_uSize)
        return false;
    if (m_pData == strText.m_pData)
        return true;
    return _stricmp(GetInternalBuffer(), strText.GetInternalBuffer()) == 0;
}

int32_t EAStringC::CompareNoCase(const char* const pStrText) const
{
    return _stricmp(GetInternalBuffer(), pStrText);
}

int32_t EAStringC::CompareNoCase(const EAStringC& strText) const
{
    if (m_pData == strText.m_pData)
        return 0;
    return _stricmp(GetInternalBuffer(), strText.GetInternalBuffer());
}

bool EAStringC::EqualNoCaseHash(const EAStringC& strText) const
{
    if (m_pData == strText.m_pData)
        return true;
    if (m_pData->m_uHash != strText.m_pData->m_uHash)
        return false;
    return _stricmp(GetInternalBuffer(), strText.GetInternalBuffer()) == 0;
}

// ---------------------------------------------------------------------------
// Memory bring-up.  PS3 External 0x7DEB84 / 0x7DEB90.
// ---------------------------------------------------------------------------
void EAStringC::MemInitialize(AptUserFunctions* const callbacks) { sAptCallbacks = callbacks; }
void EAStringC::MemUninitialize()                                { sAptCallbacks = 0; }
