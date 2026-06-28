#pragma once

// ===========================================================================
// EATech Apt -- EAStringC: the EA reference-counted string class used throughout
// the Apt engine (AptLoader::Load takes one; AptString wraps one; typedef
// AptNativeString = EAStringC). SHAPE verbatim from the Feb-2007 leak
// (SDKs/Packages/Apt/2.00.00/include/Apt/AptString/EAString.h).
//
// Layout: a single pointer m_pData to a DebugDataC (a StringDataC header --
// refcount/size/maxsize/hash -- immediately followed by the char buffer). Strings
// are shared by refcount; the empty string points at s_EmptyInternalData. The
// inline ctors/dtor/buffer accessors are the leak's; the large out-of-line method
// set (operator=/==/+, Append/Format, Find/Replace/Trim, UTF8_*, the refcount +
// buffer helpers) is DECLARED here and bodied from the X360/PS3 asm as the engine
// brings each path online (FLAG: bodies are a follow-on -- this header defines the
// type so AptLoader's signature, AptString, and the data format can reference it).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>
#include <cstdarg>   // va_list

struct AptUserFunctions;   // Apt.h -- the allocator/callback table (held by ptr only)

class EAStringC
{
public:
    EAStringC() : m_pData(reinterpret_cast<DebugDataC*>(&s_EmptyInternalData))
    {
        IncreaseInternalRefCount();
    }

    explicit EAStringC(const uint32_t uReservedLength);
    EAStringC(const uint32_t cChar, const uint32_t uLength);

    EAStringC(const char* const pStrText)
    {
        InitFromBuffer(pStrText);
    }

    EAStringC(const EAStringC& strText) : m_pData(strText.m_pData)
    {
        IncreaseInternalRefCount();
    }

    ~EAStringC()
    {
        DecreaseInternalRefCount(m_pData);
    }

private:
    // The shared string header: refcount + sizes + cached hash, followed in memory
    // by the char buffer (GetInternalBuffer = this + sizeof(StringDataC)).
    class alignas(4) StringDataC   // leak: __attribute__((aligned(4))) -> portable alignas (sizeof stays 8)
    {
    public:
        volatile uint16_t m_uRefCount;
        uint16_t          m_uSize;
        uint16_t          m_uMaxSize;
        uint16_t          m_uHash;
    };

    class DebugDataC : public StringDataC
    {
    public:
        char m_strText[256];
    };

    char* GetInternalBuffer() const
    {
        return reinterpret_cast<char*>(m_pData) + sizeof(StringDataC);
    }

public:
    struct StaticStringHelperT
    {
        EAStringC::StringDataC sd;
        char                   pbuf[256];
    };

    const char* GetBuffer() const { return GetInternalBuffer(); }
    operator const char*() const  { return GetBuffer(); }

    bool IsConstantString() { return m_pData->m_uSize > m_pData->m_uMaxSize; }

    uint32_t  operator[](const int32_t index) const;

    EAStringC& operator=(const EAStringC& strText);
    EAStringC& operator=(const StaticStringHelperT& strStruct);

    bool operator==(const EAStringC& strText) const;
    bool operator!=(const EAStringC& strText) const;
    bool operator<(const EAStringC& strText) const;
    bool operator<=(const EAStringC& strText) const;
    bool operator>(const EAStringC& strText) const;
    bool operator>=(const EAStringC& strText) const;

    bool operator==(const char* const pStrText) const;
    bool operator!=(const char* const pStrText) const;
    bool operator<(const char* const pStrText) const;
    bool operator<=(const char* const pStrText) const;
    bool operator>(const char* const pStrText) const;
    bool operator>=(const char* const pStrText) const;

    EAStringC  operator+(const EAStringC& strText) const;
    EAStringC& operator+=(const EAStringC& strText);
    EAStringC  operator+(const char* const pStrText) const;
    EAStringC& operator+=(const char* const pStrText);

    friend EAStringC operator+(const char* const pStrText, const EAStringC& strText);

    EAStringC& Duplicate(const EAStringC& strText);

    uint32_t GetLength() const;
    void     Clear();
    void     ClearConstantValue();

    void ReserveSize(const uint32_t uSize);
    bool IsEnoughSize(const uint32_t uSize) const;

    EAStringC& Append(const char* const pStrText, const uint32_t uSize);
    void       AppendFormat(const char* const pStrFormat, ...);
    void       Format(const char* const pStrFormat, ...);
    void       vsFormat(const char* const pStrFormat, va_list Args);

    bool    Equal(const char* const pStrText) const;
    bool    Equal(const EAStringC& strText) const;
    bool    EqualNoCase(const char* const pStrText) const;
    bool    EqualNoCase(const EAStringC& strText) const;
    int32_t Compare(const char* const pStrText) const;
    int32_t Compare(const EAStringC& strText) const;
    int32_t CompareNoCase(const char* const pStrText) const;
    int32_t CompareNoCase(const EAStringC& strText) const;
    bool    EqualHash(const EAStringC& strText) const;
    bool    EqualNoCaseHash(const EAStringC& strText) const;

    int32_t Find(const char* const pStrText, const int32_t iStart = 0) const;
    int32_t Find(const char cChar, const int32_t iStart = 0) const;
    int32_t FindOneOf(const char* const pStrText) const;
    int32_t ReverseFind(const char cChar) const;

    int32_t Delete(const int32_t iIndex, const int32_t iCount = 1);
    int32_t Remove(const char cRemove);
    int32_t Insert(const int32_t iIndex, const char* const pStrText);
    int32_t Insert(const int32_t iIndex, const char cChar);
    int32_t Replace(const char* const pStrOld, const char* const pStrNew);
    int32_t Replace(const char cOld, const char cNew);

    EAStringC Left(const int32_t iCount) const;
    EAStringC Right(const int32_t iCount) const;
    EAStringC Mid(const int32_t iFirst) const;
    EAStringC Mid(const int32_t iFirst, const int32_t iCount) const;

    EAStringC& MakeLower();
    EAStringC& MakeUpper();
    EAStringC& MakeReverse();

    EAStringC& TrimLeft(const char* const pStrText);
    EAStringC& TrimRight(const char* const pStrText);
    EAStringC& Trim(const char* const pStrText);
    bool       StartWith(const char* const pStrText) const;
    bool       StartWithIgnoreCase(const char* const pStrText) const;
    bool       EndWith(const char* const pStrText) const;
    bool       EndWithIgnoreCase(const char* const pStrText) const;
    bool       StartWithRemove(const char* const pStrText);
    bool       StartWithRemoveIgnoreCase(const char* const pStrText);
    bool       EndWithRemove(const char* const pStrText);
    bool       EndWithRemoveIgnoreCase(const char* const pStrText);

    const char* UTF8_GetBuffer(const int32_t iIndex);
    int32_t     UTF8_CharAt(const int32_t iIndex) const;
    int32_t     UTF8_Size() const;
    EAStringC   UTF8_Mid(const int32_t iFirst) const;
    EAStringC   UTF8_Mid(const int32_t iFirst, const int32_t iCount) const;
    EAStringC&  UTF8_Append(const char* const pStrText, const int32_t iSize);
    int32_t     UTF8_Find(const char* const pStrText, const int32_t iStart = 0) const;
    EAStringC&  UTF8_MakeLower();
    EAStringC&  UTF8_MakeUpper();
    EAStringC&  UTF8_Initialize(const int32_t iCharacter);

    static int32_t     UTF8_GetCharacterSize(const char* const pBuffer);
    static int32_t     UTF8_GetCharacterSize(const int32_t iCharacter);
    static const char* UTF8_GetBuffer(const char* const pBuffer, const int32_t iIndex);
    static int32_t     UTF8_GetSize(const char* const pBuffer);
    static int32_t     UTF8_GetCharacter(const char* const pBuffer);
    static void        UTF8_SetCharacter(char* const pBuffer, const int32_t iCharacter);
    void               UTF8_SetOneCharacter(const int32_t iCharacter);
    static const char* UTF8_ReadCharacter(const char* const pBuffer, int32_t* const iCharacter);
    static bool        UTF8_IsValid(const int32_t iCharacter);

    bool        IsEmpty() const;
    const char* c_str() const;
    const char* ConstRawPtr() const;
    int         Size() const;
    void        Reserve(int32_t iSize);

    static void MemInitialize(AptUserFunctions* const callbacks);
    static void MemUninitialize();

    void Invalidate();
    void Validate();
    void Validate(const EAStringC& strText);
    bool IsValid() const;

    uint16_t        GetHashValue() const;
    uint16_t        UpdateHashValue() const;
    void            CalculateHashValue() const;
    static uint16_t CalculateHashValue(const char* const pText);

    uint32_t GetInternalSize() const;
    uint32_t GetInternalMaxSize() const;
    uint32_t GetInternalRefCount() const;

private:
    enum EmptyString { EMPTY_STRING };

    explicit EAStringC(const EmptyString) : m_pData(reinterpret_cast<DebugDataC*>(&s_EmptyInternalData))
    {
        IncreaseInternalRefCount();
    }

    void SetInternalSize(const uint32_t uSize);
    void SetInternalMaxSize(const uint32_t uMaxSize);
    void SetInternalRefCount(const uint32_t uRefCount);

    void        IncreaseInternalRefCount();
    static void DecreaseInternalRefCount(StringDataC* const pData);

    void InvalidateHashValue() const;

    enum CBPushZero { CB_NO_PUSH_ZERO, CB_PUSH_ZERO };

    void ChangeBuffer(const uint32_t uSizeToReserve, const uint32_t uOffsetCopy, const uint32_t uSizeCopy,
                      const CBPushZero ePushZero, const uint32_t uInternalSize);
    void InitFromBuffer(const char* const pStrText);

    void        AllocateBuffer(const uint32_t uSize);
    static void DeallocateBuffer(const void* const pBuffer, const uint32_t uSize);

    // The open-addressing AptNativeHash stores EAStringC keys directly in its
    // bucket array and needs the raw slot representation (unused = null m_pData,
    // tombstone = s_EmptyInternalData, occupied) plus raw occupy/release -- the
    // same internal access the console code uses. Friended rather than widening
    // the public API.
    friend struct AptNativeHash;

    DebugDataC* m_pData;

    static char              s_EmptyInternalData[8 + 1];
    static AptUserFunctions* sAptCallbacks;

    enum { ZERO_HASH = 0x4567 };
};

EAStringC operator+(const char* const pStrText, const EAStringC& strText);

typedef EAStringC AptNativeString;
