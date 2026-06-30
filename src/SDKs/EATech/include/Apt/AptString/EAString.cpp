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

#include <cstring>   // strlen/strcmp/memcpy/memmove/memset/memcmp/strstr/strchr
#include <cstdlib>   // malloc/free (the AptInit-not-yet-wired bring-up fallback)
#include <cctype>    // tolower/toupper (the UTF8_Make{Lower,Upper} case fold)
#include <cstdarg>   // va_start/va_end (the Format varargs collection)
#include <cstdio>    // vsnprintf (the Format/vsFormat printf core)
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

// ===========================================================================
// Text-manipulation suite (the Apt-driven path: substring / search / append /
// trim / UTF-8). Bodies DECOMPILED from X360 ARTIST and cross-checked against
// the demangled PS3 External ELF (the GCC-mangled _ZNK9EAStringC... symbols,
// which name the StringDataC fields directly). Addresses are noted per group.
//
// All counts/lengths are the named 16-bit StringDataC fields (m_uSize/m_uMaxSize)
// widened to int; the refcount machinery reuses the leaf helpers above (the same
// console-atomic-vs-PC-plain FLAG already documented at IncreaseInternalRefCount).
// The substring builders (Left/Mid/Right) follow the console shape exactly: take
// a private reference on the source block, ChangeBuffer the desired slice into a
// throw-away string, then hand its block to *this and release the temp.
// ===========================================================================

// ---------------------------------------------------------------------------
// UTF-8 cursor primitives (static).  X360 0x82AD8818 / 0x82AD8860 / 0x82AD8A88 /
// 0x82AD88D0 / 0x82AD8978.  These are the codec leaf the UTF8_* members ride on.
// ---------------------------------------------------------------------------
int32_t EAStringC::UTF8_GetCharacterSize(const char* const pBuffer)
{
    const unsigned int c = static_cast<unsigned char>(pBuffer[0]);
    if (c <= 0x7F)
        return 1;
    if ((c & 0xE0) == 0xC0)
        return 2;
    // 0xE0 lead -> 3 bytes, anything higher -> 4 bytes.
    return ((c & 0xF0) == 0xE0) ? 3 : 4;
}

int32_t EAStringC::UTF8_GetCharacterSize(const int32_t iCharacter)
{
    if (iCharacter < 0x80)    return 1;
    if (iCharacter < 0x800)   return 2;
    if (iCharacter < 0x10000) return 3;
    return 4;
}

int32_t EAStringC::UTF8_GetCharacter(const char* const pBuffer)
{
    const unsigned int c = static_cast<unsigned char>(pBuffer[0]);
    if (c <= 0x7F)
        return static_cast<int32_t>(c);
    if ((c & 0xE0) == 0xC0)
        return static_cast<int32_t>(((c << 6) & 0x7C0) | (static_cast<unsigned char>(pBuffer[1]) & 0x3F));
    const unsigned int b1 = static_cast<unsigned char>(pBuffer[1]);
    if ((c & 0xF0) == 0xE0)
        return static_cast<int32_t>((((((c << 6) & 0x3C0) | (b1 & 0x3F)) << 6)) |
                                    (static_cast<unsigned char>(pBuffer[2]) & 0x3F));
    return static_cast<int32_t>(
        (((((((c << 6) & 0x1C0) | (b1 & 0x3F)) << 6) | (static_cast<unsigned char>(pBuffer[2]) & 0x3F)) << 6)) |
        (static_cast<unsigned char>(pBuffer[3]) & 0x3F));
}

const char* EAStringC::UTF8_ReadCharacter(const char* const pBuffer, int32_t* const iCharacter)
{
    const unsigned int c = static_cast<unsigned char>(pBuffer[0]);
    if (c <= 0x7F)
    {
        *iCharacter = static_cast<int32_t>(c);
        return pBuffer + 1;
    }
    if ((c & 0xE0) == 0xC0)
    {
        *iCharacter = static_cast<int32_t>(((c << 6) & 0x7C0) | (static_cast<unsigned char>(pBuffer[1]) & 0x3F));
        return pBuffer + 2;
    }
    const unsigned int b1 = static_cast<unsigned char>(pBuffer[1]);
    if ((c & 0xF0) == 0xE0)
    {
        *iCharacter = static_cast<int32_t>((((((c << 6) & 0x3C0) | (b1 & 0x3F)) << 6)) |
                                           (static_cast<unsigned char>(pBuffer[2]) & 0x3F));
        return pBuffer + 3;
    }
    *iCharacter = static_cast<int32_t>(
        (((((((c << 6) & 0x1C0) | (b1 & 0x3F)) << 6) | (static_cast<unsigned char>(pBuffer[2]) & 0x3F)) << 6)) |
        (static_cast<unsigned char>(pBuffer[3]) & 0x3F));
    return pBuffer + 4;
}

void EAStringC::UTF8_SetCharacter(char* const pBuffer, const int32_t iCharacter)
{
    if (iCharacter < 0x80)
    {
        pBuffer[0] = static_cast<char>(iCharacter);
        return;
    }
    if (iCharacter < 0x800)
    {
        pBuffer[0] = static_cast<char>((iCharacter >> 6) | 0xC0);
        pBuffer[1] = static_cast<char>(0x80 | (iCharacter & 0x3F));
        return;
    }
    if (iCharacter < 0x10000)
    {
        pBuffer[0] = static_cast<char>((iCharacter >> 12) | 0xE0);
        pBuffer[1] = static_cast<char>(0x80 | ((iCharacter >> 6) & 0x3F));
        pBuffer[2] = static_cast<char>(0x80 | (iCharacter & 0x3F));
        return;
    }
    pBuffer[0] = static_cast<char>((iCharacter >> 18) | 0xF0);
    pBuffer[1] = static_cast<char>(0x80 | ((iCharacter >> 12) & 0x3F));
    pBuffer[2] = static_cast<char>(0x80 | ((iCharacter >> 6) & 0x3F));
    pBuffer[3] = static_cast<char>(0x80 | (iCharacter & 0x3F));
}

bool EAStringC::UTF8_IsValid(const int32_t iCharacter)
{
    return static_cast<uint32_t>(iCharacter) <= 0x10FFFFu;
}

// Encode a single code point into the (already-reserved) buffer and set the
// string's size to its byte length.  X360 0x82AD8978.
void EAStringC::UTF8_SetOneCharacter(const int32_t iCharacter)
{
    char* const buf = GetInternalBuffer();
    const int32_t nBytes = UTF8_GetCharacterSize(iCharacter);
    UTF8_SetCharacter(buf, iCharacter);
    buf[nBytes]        = 0;
    m_pData->m_uSize   = static_cast<uint16_t>(nBytes);
    m_pData->m_uHash   = 0;
}

// ---------------------------------------------------------------------------
// Substring builders.  X360 0x82AE8928 (Left) / 0x82AE8A20 (Mid) / Right (PS3
// External 0x7F67DC).  Each takes a private ref on the source block, copies the
// slice via ChangeBuffer into a temp, then transfers ownership to *this.
// ---------------------------------------------------------------------------
EAStringC EAStringC::Left(const int32_t iCount) const
{
    if (iCount <= 0)
        return EAStringC(EMPTY_STRING);
    if (static_cast<uint32_t>(iCount) >= m_pData->m_uSize)
        return *this;                           // whole string fits -> share the block

    EAStringC temp(*this);                      // private ref on the source block
    temp.ChangeBuffer(iCount, 0, iCount, CB_PUSH_ZERO, iCount);
    return temp;
}

EAStringC EAStringC::Right(const int32_t iCount) const
{
    if (iCount <= 0)
        return EAStringC(EMPTY_STRING);
    const int32_t iOffset = static_cast<int32_t>(m_pData->m_uSize) - iCount;
    if (iOffset <= 0)
        return *this;                           // whole string fits -> share the block

    EAStringC temp(*this);
    temp.ChangeBuffer(iCount, static_cast<uint32_t>(iOffset), iCount, CB_PUSH_ZERO, iCount);
    return temp;
}

EAStringC EAStringC::Mid(const int32_t iFirst) const
{
    if (iFirst <= 0)
        return *this;                           // from the start -> share the block
    const int32_t iCount = static_cast<int32_t>(m_pData->m_uSize) - iFirst;
    if (iCount <= 0)
        return EAStringC(EMPTY_STRING);

    EAStringC temp(*this);
    temp.ChangeBuffer(iCount, static_cast<uint32_t>(iFirst), iCount, CB_PUSH_ZERO, iCount);
    return temp;
}

EAStringC EAStringC::Mid(const int32_t iFirst, const int32_t iCount) const
{
    int32_t iStart = iFirst;
    int32_t iLen   = iCount;
    if (iFirst < 0)
    {
        iLen += iFirst;                          // a negative start eats into the count
        iStart = 0;
        if (iLen <= 0)
            return EAStringC(EMPTY_STRING);
    }
    else if (iCount <= 0)
    {
        return EAStringC(EMPTY_STRING);
    }

    const int32_t iAvail = static_cast<int32_t>(m_pData->m_uSize) - iStart;
    if (iAvail <= 0)
        return EAStringC(EMPTY_STRING);
    if (iLen > iAvail)
        iLen = iAvail;

    EAStringC temp(*this);
    temp.ChangeBuffer(iLen, static_cast<uint32_t>(iFirst), iLen, CB_PUSH_ZERO, iLen);
    return temp;
}

// ---------------------------------------------------------------------------
// Search.  Find: PS3 External 0x7EEAFC (strstr-based).  LastIndexOf: X360
// 0x82AD8BF8 (reverse scan).
// ---------------------------------------------------------------------------
int32_t EAStringC::Find(const char* const pStrText, const int32_t iStart) const
{
    if (static_cast<int32_t>(m_pData->m_uSize) > iStart)
    {
        const char* const base = GetInternalBuffer();
        const int32_t      from = (iStart >= 0) ? iStart : 0;
        const char* const hit  = strstr(base + from, pStrText);
        if (hit)
            return static_cast<int32_t>(hit - base);
    }
    return -1;
}

int32_t EAStringC::LastIndexOf(const char* const pStrText, const int32_t iStart) const
{
    const int32_t      iNeedle = static_cast<int32_t>(strlen(pStrText));
    const char* const  base    = GetInternalBuffer();

    // Highest index where the needle can still fit (size - (needleLen-1) - 1),
    // clamped to the caller's start cap.
    int32_t iPos = static_cast<int32_t>(m_pData->m_uSize) - (iNeedle - 1) - 1;
    if (iStart < iPos)
        iPos = iStart;
    if (iPos < 0)
        return -1;

    for (;;)
    {
        const char* p = pStrText;
        const char* q = base + iPos;
        while (*p && *q == *p)
        {
            ++q;
            ++p;
        }
        if (*p == 0)        // matched the whole needle
            return iPos;
        if (--iPos < 0)
            return -1;
    }
}

// ---------------------------------------------------------------------------
// In-place edit.  Delete: X360 0x82AE8830.  Append: X360 0x82AE86F8.
// ---------------------------------------------------------------------------
int32_t EAStringC::Delete(const int32_t iIndex, const int32_t iCount)
{
    int32_t iFrom = iIndex;
    int32_t iTo   = iIndex + iCount;
    if (iCount <= 0 || iTo <= 0)
    {
        // Nothing (or everything before 0) to keep -> empty string.
        DecreaseInternalRefCount(m_pData);
        m_pData = reinterpret_cast<DebugDataC*>(&s_EmptyInternalData);
        IncreaseInternalRefCount();
        return 0;
    }

    if (iIndex < 0)
        iFrom = 0;
    const int32_t iSize = static_cast<int32_t>(m_pData->m_uSize);
    if (iTo >= iSize)
        iTo = iSize;

    if (iFrom == 0)
    {
        // Cut the head: keep the tail [iTo .. size).
        const int32_t iKept = iSize - iTo;
        ChangeBuffer(iKept, static_cast<uint32_t>(iTo), iKept, CB_PUSH_ZERO, iKept);
        return iKept;
    }

    if (iTo != iSize)
    {
        // Cut a middle span: keep the head [0 .. iFrom) then splice the tail in.
        const int32_t iTail = iSize - iTo;
        const int32_t iKept = iTail + iFrom;
        char* const   src   = GetInternalBuffer() + iTo;
        ChangeBuffer(iKept, 0, iFrom, CB_NO_PUSH_ZERO, iKept);
        memcpy(GetInternalBuffer() + iFrom, src, static_cast<size_t>(iTail) + 1);
        return iKept;
    }

    // Cut the tail: keep the head [0 .. iFrom).
    ChangeBuffer(iFrom, 0, iFrom, CB_PUSH_ZERO, iFrom);
    return iFrom;
}

EAStringC& EAStringC::Append(const char* const pStrText, const uint32_t uSize)
{
    if (uSize)
    {
        // Append at most uSize chars, stopping early at a NUL.
        uint32_t uLen = 0;
        const char* p = pStrText;
        while (uLen < uSize && *p)
        {
            ++p;
            ++uLen;
        }
        if (uLen)
        {
            const uint32_t uOld = m_pData->m_uSize;
            ChangeBuffer(uOld + uLen, 0, uOld, CB_PUSH_ZERO, uOld + uLen);
            memcpy(GetInternalBuffer() + uOld, pStrText, uLen);
            GetInternalBuffer()[uOld + uLen] = 0;
        }
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Trim / suffix-remove.  TrimRight: X360 0x82AE8C58.  EndWithRemove: X360
// 0x82AE8D00.  EndWithRemoveIgnoreCase: PS3 External 0x7F7208.
// ---------------------------------------------------------------------------
EAStringC& EAStringC::TrimRight(const char* const pStrText)
{
    const uint32_t uSize = m_pData->m_uSize;
    uint32_t uTrim = 0;
    const char* p = GetInternalBuffer() + uSize - 1;
    while (uTrim < uSize)
    {
        if (!strchr(pStrText, static_cast<unsigned char>(*p)))
            break;
        --p;
        ++uTrim;
    }
    *this = Left(static_cast<int32_t>(uSize - uTrim));
    return *this;
}

bool EAStringC::EndWithRemove(const char* const pStrText)
{
    const uint32_t uSize   = m_pData->m_uSize;
    const uint32_t uNeedle = static_cast<uint32_t>(strlen(pStrText));
    if (uSize < uNeedle)
        return false;
    if (memcmp(GetInternalBuffer() + uSize - uNeedle, pStrText, uNeedle) != 0)
        return false;
    *this = Left(static_cast<int32_t>(uSize - uNeedle));
    return true;
}

bool EAStringC::EndWithRemoveIgnoreCase(const char* const pStrText)
{
    const uint32_t uSize   = m_pData->m_uSize;
    const uint32_t uNeedle = static_cast<uint32_t>(strlen(pStrText));
    if (uSize < uNeedle || _stricmp(GetInternalBuffer() + uSize - uNeedle, pStrText) != 0)
        return false;
    *this = Left(static_cast<int32_t>(uSize - uNeedle));
    return true;
}

// ---------------------------------------------------------------------------
// Concatenation.  operator+= : PS3 External _ZN9EAStringCpLEPKc / pLERKS_.
// free operator+(const char*, const EAStringC&) : X360 0x82AE85D0.
// ---------------------------------------------------------------------------
EAStringC& EAStringC::operator+=(const char* const pStrText)
{
    const uint32_t uOld = m_pData->m_uSize;
    if (!uOld)
    {
        *this = EAStringC(pStrText);
        return *this;
    }
    const uint32_t uLen = static_cast<uint32_t>(strlen(pStrText));
    if (!uLen)
        return *this;
    ChangeBuffer(uOld + uLen, 0, uOld, CB_NO_PUSH_ZERO, uOld + uLen);
    memcpy(GetInternalBuffer() + uOld, pStrText, uLen + 1);
    return *this;
}

EAStringC& EAStringC::operator+=(const EAStringC& strText)
{
    const uint32_t uOld = m_pData->m_uSize;
    if (!uOld)
    {
        *this = strText;
        return *this;
    }
    const uint32_t uAdd = strText.m_pData->m_uSize;
    if (!uAdd)
        return *this;
    ChangeBuffer(uOld + uAdd, 0, uOld, CB_NO_PUSH_ZERO, uOld + uAdd);
    memcpy(GetInternalBuffer() + uOld, strText.GetInternalBuffer(), uAdd + 1);
    return *this;
}

EAStringC operator+(const char* const pStrText, const EAStringC& strText)
{
    const uint32_t uTail = strText.m_pData->m_uSize;
    if (!uTail)
        return EAStringC(pStrText);             // RHS empty -> just the prefix

    const uint32_t uHead = static_cast<uint32_t>(strlen(pStrText));
    if (!uHead)
        return strText;                         // prefix empty -> share the RHS block

    EAStringC result(uHead + uTail);            // reserve head+tail
    char* const buf = result.GetInternalBuffer();
    memcpy(buf, pStrText, uHead);
    memcpy(buf + uHead, strText.GetInternalBuffer(), uTail);
    buf[uHead + uTail]            = 0;
    result.m_pData->m_uSize       = static_cast<uint16_t>(uHead + uTail);
    result.m_pData->m_uHash       = 0;
    return result;
}

// ---------------------------------------------------------------------------
// UTF-8 string operations.  X360 0x82ADCFE8 / 0x82ADD040 / 0x82ADD088 /
// 0x82AE8E70 / 0x82AE8F78 / 0x82AE8FF0 / 0x82AE9068 / 0x82AE90F0 (+ the PS3
// static UTF8_GetBuffer(char*,int)).
// ---------------------------------------------------------------------------

// Static: advance pBuffer by iIndex code points; null if the string ends first.
const char* EAStringC::UTF8_GetBuffer(const char* const pBuffer, const int32_t iIndex)
{
    if (iIndex <= 0)
        return pBuffer;

    const char* p = pBuffer;
    int32_t     n = 0;
    for (;;)
    {
        int32_t cp;
        p = UTF8_ReadCharacter(p, &cp);
        ++n;
        if (cp == 0)
            return 0;        // hit the terminator before reaching iIndex
        if (n == iIndex)
            return p;
    }
}

// Member: the same skip, anchored at this string's buffer.
const char* EAStringC::UTF8_GetBuffer(const int32_t iIndex)
{
    return UTF8_GetBuffer(GetInternalBuffer(), iIndex);
}

// Code-point count of this string.
int32_t EAStringC::UTF8_Size() const
{
    const char* p = GetInternalBuffer();
    int32_t     n = 0;
    for (;;)
    {
        int32_t cp;
        p = UTF8_ReadCharacter(p, &cp);
        if (cp == 0)
            break;
        ++n;
    }
    return n;
}

int32_t EAStringC::UTF8_CharAt(const int32_t iIndex) const
{
    const char* const p = UTF8_GetBuffer(GetInternalBuffer(), iIndex);
    return p ? UTF8_GetCharacter(p) : 0;
}

// Find a substring, returning a code-point (not byte) index.  The byte offset
// from Find is converted back to a character count by re-walking the prefix.
int32_t EAStringC::UTF8_Find(const char* const pStrText, const int32_t iStart) const
{
    const char* const base   = GetInternalBuffer();
    const char* const pStart = UTF8_GetBuffer(base, iStart);
    if (!pStart)
        return -1;

    const int32_t iByte = Find(pStrText, static_cast<int32_t>(pStart - base));
    if (iByte < 0)
        return -1;

    // Re-walk from the start offset to convert the byte hit into a char index.
    int32_t     iChar = iStart;
    const char* p     = pStart;
    while (p - base < iByte)
    {
        p += UTF8_GetCharacterSize(p);
        ++iChar;
    }
    return iChar;
}

EAStringC EAStringC::UTF8_Mid(const int32_t iFirst) const
{
    int32_t iFrom = iFirst;
    if (iFrom < 0)
        iFrom = 0;
    const char* const base   = GetInternalBuffer();
    const char* const pStart = UTF8_GetBuffer(base, iFrom);
    if (!pStart)
        return EAStringC(EMPTY_STRING);
    return Mid(static_cast<int32_t>(pStart - base));
}

EAStringC EAStringC::UTF8_Mid(const int32_t iFirst, const int32_t iCount) const
{
    int32_t iFrom = iFirst;
    int32_t iLen  = iCount;
    if (iFirst < 0)
    {
        iLen += iFirst;
        iFrom = 0;
        if (iLen <= 0)
            return EAStringC(EMPTY_STRING);
    }
    else if (iCount <= 0)
    {
        return EAStringC(EMPTY_STRING);
    }

    const char* const base   = GetInternalBuffer();
    const char* const pStart = UTF8_GetBuffer(base, iFrom);
    if (!pStart)
        return EAStringC(EMPTY_STRING);

    const char* const pEnd = UTF8_GetBuffer(pStart, iLen);
    if (pEnd)
        return Mid(static_cast<int32_t>(pStart - base),
                   static_cast<int32_t>(pEnd - pStart));
    return Mid(static_cast<int32_t>(pStart - base));   // ran off the end -> to the tail
}

EAStringC& EAStringC::UTF8_Append(const char* const pStrText, const int32_t iSize)
{
    // Walk iSize code points (stopping early at a NUL) to find the byte length.
    const char* p = pStrText;
    int32_t     n = 0;
    if (iSize > 0)
    {
        for (;;)
        {
            int32_t cp;
            p = UTF8_ReadCharacter(p, &cp);
            if (cp == 0)
                break;
            if (++n >= iSize)
                break;
        }
    }
    return Append(pStrText, static_cast<uint32_t>(p - pStrText));
}

EAStringC& EAStringC::UTF8_Initialize(const int32_t iCharacter)
{
    // Reset to empty, reserve room for one encoded code point, then write it.
    DecreaseInternalRefCount(m_pData);
    m_pData = reinterpret_cast<DebugDataC*>(&s_EmptyInternalData);

    const uint32_t uBytes = static_cast<uint32_t>(UTF8_GetCharacterSize(iCharacter));
    uint32_t uKeep = m_pData->m_uMaxSize;       // empty block's max size (0)
    if (uKeep > uBytes)
        uKeep = uBytes;
    ChangeBuffer(uBytes, 0, uKeep, CB_PUSH_ZERO, uKeep);
    UTF8_SetOneCharacter(iCharacter);
    return *this;
}

EAStringC& EAStringC::UTF8_MakeLower()
{
    // Force a private, fully-sized copy, then lower each code point in place.
    const uint32_t uSize = m_pData->m_uSize;
    ChangeBuffer(uSize, 0, uSize, CB_PUSH_ZERO, uSize);

    char* dst = GetInternalBuffer();
    const char* src = dst;
    for (;;)
    {
        int32_t cp;
        const char* next = UTF8_ReadCharacter(src, &cp);
        if (cp == 0)
            break;
        UTF8_SetCharacter(dst, tolower(cp));
        dst += UTF8_GetCharacterSize(dst);
        src  = next;
    }
    return *this;
}

EAStringC& EAStringC::UTF8_MakeUpper()
{
    const uint32_t uSize = m_pData->m_uSize;
    ChangeBuffer(uSize, 0, uSize, CB_PUSH_ZERO, uSize);

    char* dst = GetInternalBuffer();
    const char* src = dst;
    for (;;)
    {
        int32_t cp;
        const char* next = UTF8_ReadCharacter(src, &cp);
        if (cp == 0)
            break;
        // The console only upper-cases the ASCII range (<= 0x7F).
        if (cp <= 0x7F)
            cp = toupper(cp);
        UTF8_SetCharacter(dst, cp);
        dst += UTF8_GetCharacterSize(dst);
        src  = next;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Format / vsFormat : the printf-style formatter.  X360 Format @0x82AEDE48
// (tail-calls vsFormat) / vsFormat @0x82AE8788.
//
// vsFormat (the workhorse) reserves an estimate of 4*strlen(format) chars, then
// vsnprintf's into the string's own buffer (DstBuf = GetInternalBuffer(),
// MaxCount = m_uMaxSize); if the formatted text did not fit (vsnprintf returned
// negative -- the console's vsnprintf signals truncation with a negative result)
// it DOUBLES the reserve and retries. On success it null-terminates at the
// returned length and stamps m_uSize = length, m_uHash = 0 -- exactly the X360
// `stbx 0; sth len,2; sth 0,6` tail. ChangeBuffer here is the (reserve, 0,0,
// CB_NO_PUSH_ZERO, 0) form the asm emits (r4=size, r5..r8=0).
//
// Format collects the C varargs into a va_list and forwards to vsFormat -- the
// X360 Format only marshals the register/stack args into the &a9 arg pointer it
// hands to vsFormat (`return EAStringC::vsFormat(this, fmt, &a9)`).
// ---------------------------------------------------------------------------
void EAStringC::vsFormat(const char* const pStrFormat, va_list Args)
{
    // Initial reserve: 4 * strlen(format) (X360 walks to the NUL then `slwi ,2`).
    uint32_t uReserve = 4u * static_cast<uint32_t>(strlen(pStrFormat));

    int iResult;
    for (;;)
    {
        ChangeBuffer(uReserve, 0, 0, CB_NO_PUSH_ZERO, 0);
        char* const pBuffer = GetInternalBuffer();           // *m_pData + 8
        // MaxCount = m_uMaxSize (the usable char capacity of the reserved block).
        iResult = vsnprintf(pBuffer, m_pData->m_uMaxSize, pStrFormat, Args);
        if (iResult >= 0)
            break;
        uReserve *= 2;                                       // didn't fit -> grow + retry
    }

    GetInternalBuffer()[iResult] = 0;
    m_pData->m_uSize = static_cast<uint16_t>(iResult);
    m_pData->m_uHash = 0;
}

void EAStringC::Format(const char* const pStrFormat, ...)
{
    va_list Args;
    va_start(Args, pStrFormat);
    vsFormat(pStrFormat, Args);
    va_end(Args);
}

// ---------------------------------------------------------------------------
// Burnout_X360_Artist_0040_0 (ICF-folded) @0x82AD85D8 -- the case-sensitive
// EAStringC equality the linker folded to this synthetic symbol. The X360 body
// loads both m_pData, rejects on a m_uSize mismatch, accepts on identical
// m_pData, then byte-compares the two buffers over m_uSize bytes -- byte-for-byte
// EAStringC::operator==(const EAStringC&). Reconstructed as that call so the
// folded symbol resolves to the identical behaviour.
// ---------------------------------------------------------------------------
bool Burnout_X360_Artist_0040_0(EAStringC* a, EAStringC* b)
{
    return *a == *b;
}
