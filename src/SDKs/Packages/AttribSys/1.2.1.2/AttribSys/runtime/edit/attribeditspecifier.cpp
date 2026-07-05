#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/edit/attribeditspecifier.h"

#include <cstring> // strstr, strncpy
#include <cstdlib> // atoi
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.h" // Attrib::GetDatabasePrivate
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"        // Attrib::StringToKey
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"                 // VecHashMap_Attrib_Class_TablePolicy_0_16

// ===========================================================================
// Attrib::EditSpecifier + its strict-weak-ordering comparator, reconstructed
// store-for-store from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//
//   EditSpecifierLess::operator() @ 0x82803DC0
//   EditSpecifier::Decode         @ 0x82805898
//   EditSpecifier::GetClass       @ 0x82808240
//
// Member layout/names are authoritative from the AttribSys PS3 DWARF
// (attriblivelink.cpp:110-134); see attribeditspecifier.h.
// ===========================================================================

namespace Attrib
{

// ---------------------------------------------------------------------------
// EditSpecifierLess::operator() @ 0x82803DC0
// A lexicographic, all-unsigned strict-weak ordering over
// (mClassKey, mCollectionKey, mAttribKey, mIndex): the three 64-bit keys use
// `cmpld` (unsigned) and the 32-bit tie-breaker uses the subfc/subfe unsigned
// less-than idiom.
// ---------------------------------------------------------------------------
bool EditSpecifierLess::operator()(const EditSpecifier& lfLhs,
                                   const EditSpecifier& lfRhs) const
{
    if (lfLhs.mClassKey != lfRhs.mClassKey)
    {
        return lfLhs.mClassKey < lfRhs.mClassKey;
    }
    if (lfLhs.mCollectionKey != lfRhs.mCollectionKey)
    {
        return lfLhs.mCollectionKey < lfRhs.mCollectionKey;
    }
    if (lfLhs.mAttribKey != lfRhs.mAttribKey)
    {
        return lfLhs.mAttribKey < lfRhs.mAttribKey;
    }
    return lfLhs.mIndex < lfRhs.mIndex;
}

// ===========================================================================
// Attrib::EditSpecifier::Decode @ 0x82805898
// ===========================================================================
// Parse a dotted edit-path "<class>.<collection>.<attrib>.<index><term>" into
// the four fields. Each of the first three fields is the text before the next
// '.', hashed through Attrib::StringToKey(text, length, seed) with the 64-bit
// seed staged inline at every AttribSys hash site (0xABCDEF0011223344), unless
// the source pointer is null or the token is empty (field 0). A missing '.' at
// any stage aborts the parse and returns null. The fourth field (index) runs to
// the first delimiter in "=:+-#" (or NUL), is copied into a bounded stack buffer
// and atoi'd into mIndex; if that token would not fit (length+1 > 0x10) the parse
// fails. On success the return value points at the delimiter that ended the token.
const char* Attrib::EditSpecifier::Decode(const char* lpcText)
{
    static const u64 KU_HASH_SEED = 0xABCDEF0011223344ULL;

    u8* lpThis = reinterpret_cast<u8*>(this);

    // --- mClassKey @ +0x00 ---------------------------------------------------
    const char* lpcDot = strstr(lpcText, ".");
    const char* lpcCollection = lpcDot + 1;
    if (lpcDot == NULL)
        return NULL;
    {
        const u32 luLen = static_cast<u32>(lpcDot - lpcText);
        u64 luKey = 0;
        if (lpcText != NULL && luLen != 0)
            luKey = Attrib::StringToKey(lpcText, luLen, KU_HASH_SEED);
        *reinterpret_cast<u64*>(lpThis + 0x00) = luKey;
    }

    // --- mCollectionKey @ +0x08 ----------------------------------------------
    lpcDot = strstr(lpcCollection, ".");
    const char* lpcAttrib = lpcDot + 1;
    if (lpcDot == NULL)
        return NULL;
    {
        const u32 luLen = static_cast<u32>(lpcDot - lpcCollection);
        u64 luKey = 0;
        if (lpcCollection != NULL && luLen != 0)
            luKey = Attrib::StringToKey(lpcCollection, luLen, KU_HASH_SEED);
        *reinterpret_cast<u64*>(lpThis + 0x08) = luKey;
    }

    // --- mAttribKey @ +0x10 --------------------------------------------------
    lpcDot = strstr(lpcAttrib, ".");
    const char* lpcIndex = lpcDot + 1;
    if (lpcDot == NULL)
        return NULL;
    {
        const u32 luLen = static_cast<u32>(lpcDot - lpcAttrib);
        u64 luKey = 0;
        if (lpcAttrib != NULL && luLen != 0)
            luKey = Attrib::StringToKey(lpcAttrib, luLen, KU_HASH_SEED);
        *reinterpret_cast<u64*>(lpThis + 0x10) = luKey;
    }

    // --- mIndex @ +0x18 ------------------------------------------------------
    // Scan forward until the current char is one of the index delimiters (or NUL),
    // mirroring the X360's nested set-walk exactly (including the dead `!= 6` guard).
    static const char KACDELIMITERS[] = "=:+-#";
    const char* i = lpcIndex;
    for (;; ++i)
    {
        const char lcCh = *i;
        const char* lpcSet = KACDELIMITERS;
        char lcSet = *lpcSet;
        while (lcCh != lcSet)
        {
            if (lcSet == 0)
                break;
            lcSet = *++lpcSet;
        }
        if (*lpcSet == lcCh && (lpcSet - KACDELIMITERS) != 6)
            break;
    }

    if (static_cast<u32>((i - lpcIndex) + 1) <= 0x10)
    {
        char lacBuffer[64];
        strncpy(lacBuffer, lpcIndex, static_cast<size_t>(i - lpcIndex));
        lacBuffer[i - lpcIndex] = 0;
        *reinterpret_cast<u32*>(lpThis + 0x18) = static_cast<u32>(atoi(lacBuffer));
        return i;
    }
    return NULL;
}

// ===========================================================================
// Attrib::EditSpecifier::GetClass @ 0x82808240
// ===========================================================================
// Resolve this EditSpecifier's mClassKey against the process attribute database's
// class-registry table (DatabasePrivate+8 == the VecHashMap<Key,Class,...,16u>);
// returns the owning Attrib::Class (or null if the key is not registered).
Attrib::Class* Attrib::EditSpecifier::GetClass() const
{
    u8* lpPrivates = reinterpret_cast<u8*>(Attrib::GetDatabasePrivate());
    VecHashMap_Attrib_Class_TablePolicy_0_16* lpTable =
        reinterpret_cast<VecHashMap_Attrib_Class_TablePolicy_0_16*>(lpPrivates + 8);
    return lpTable->Find(*reinterpret_cast<const u64*>(
        reinterpret_cast<const u8*>(this) + 0x00)); // mClassKey
}

} // namespace Attrib
