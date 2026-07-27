#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"

#include <string.h>   // strlen (the NUL-terminated convenience form)

// Attrib::StringToKey -- the AttribSys text hash, reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//   Attrib::StringToKey(const char*, u32, u64)  @ 0x82802940
//   Attrib::StringToKey(const char*)            @ 0x82805828 (strlen + seed wrapper)
//
// The X360 body @0x82802940 is Bob Jenkins' 1997 lookup8 64-bit hash ("hash64"):
// the 24-byte block loop feeds three 64-bit lanes seeded {level, level,
// 0x9E3779B97F4A7C13} through mix64 (the characteristic shift ladder
// 43/9/8/38/23/5/35/49/11/12/18/22 is verbatim in the asm), the tail switch
// folds the remaining 0..23 bytes in at the lookup8 byte positions, and the
// c-lane is returned. Every call site stages the same 64-bit seed
// 0xABCDEF00_11223344 (e.g. CgsAttribSys::AttribSysCollectionKey::GetHashKey
// @0x82805C20 builds it with lis/ori/insrdi into r5), which is how the
// AttributeKey.h declaration documents it.
//
// Hash-identity attestations (data-driven, this reconstruction verified
// against all of them):
//   hash64('boostparamsasset')          == 0xDA21657C48943FAC (WORLDVAULT class)
//   hash64('schema.vlt')/('schema.bin') == the schema DepN dependency ids
//   hash64('Attrib::Database')          == the schema's database ExportID
//   hash64('Attrib::DatabaseLoadData')  == gDatabaseType   0x0B38846845E9C175
//   hash64('Attrib::ClassLoadData')     == gClassType      0x2A7895AC4A876152
//   hash64('Attrib::CollectionLoadData')== gCollectionType 0xAD303B8F42B3307E
//   hash64('EA::Reflection::Float')     == the schema Definition mType fields
//
// The 32-bit ::Attribute::Key consumers use the low doubleword truncation of
// the same hash (CgsResource::ID::HashString @0x828D84A8 ends clrldi r3,32).
namespace Attrib
{
namespace
{
    // lookup8 mix64 (Bob Jenkins). The X360 loop body @0x829029xx is this ladder
    // unrolled over r-register triples.
    void Mix64(u64& a, u64& b, u64& c)
    {
        a -= b; a -= c; a ^= (c >> 43);
        b -= c; b -= a; b ^= (a << 9);
        c -= a; c -= b; c ^= (b >> 8);
        a -= b; a -= c; a ^= (c >> 38);
        b -= c; b -= a; b ^= (a << 23);
        c -= a; c -= b; c ^= (b >> 5);
        a -= b; a -= c; a ^= (c >> 35);
        b -= c; b -= a; b ^= (a << 49);
        c -= a; c -= b; c ^= (b >> 11);
        a -= b; a -= c; a ^= (c >> 12);
        b -= c; b -= a; b ^= (a << 18);
        c -= a; c -= b; c ^= (b >> 22);
    }

    // lookup8's little-endian 8-byte gather (the X360 asm assembles each lane
    // byte-by-byte in this order regardless of machine endianness).
    u64 Gather8(const unsigned char* lpBytes)
    {
        return static_cast<u64>(lpBytes[0]) |
               (static_cast<u64>(lpBytes[1]) << 8) |
               (static_cast<u64>(lpBytes[2]) << 16) |
               (static_cast<u64>(lpBytes[3]) << 24) |
               (static_cast<u64>(lpBytes[4]) << 32) |
               (static_cast<u64>(lpBytes[5]) << 40) |
               (static_cast<u64>(lpBytes[6]) << 48) |
               (static_cast<u64>(lpBytes[7]) << 56);
    }
}

// @ 0x82802940 -- the full 64-bit hash. Named per the PS3 DWARF spelling
// (attribdatabase.cpp:590 StringToKey(const char*, unsigned int)); the X360 ABI
// takes the 64-bit seed as the third argument, staged 0xABCDEF0011223344 at
// every site.
u64 StringToKey64(const char* lpcText, u32 luLength, u64 luSeed)
{
    const unsigned char* lpBytes = reinterpret_cast<const unsigned char*>(lpcText);
    u64 a = luSeed;
    u64 b = luSeed;
    u64 c = 0x9E3779B97F4A7C13ull;

    u32 luRemaining = luLength;
    while (luRemaining >= 24)
    {
        a += Gather8(lpBytes);
        b += Gather8(lpBytes + 8);
        c += Gather8(lpBytes + 16);
        Mix64(a, b, c);
        lpBytes += 24;
        luRemaining -= 24;
    }

    // Tail: fold the original length into c, then the trailing 0..23 bytes at
    // the lookup8 positions (bytes 16..22 land in c shifted one byte up).
    c += luLength;
    switch (luRemaining)
    {
        case 23: c += static_cast<u64>(lpBytes[22]) << 56; // fall through
        case 22: c += static_cast<u64>(lpBytes[21]) << 48; // fall through
        case 21: c += static_cast<u64>(lpBytes[20]) << 40; // fall through
        case 20: c += static_cast<u64>(lpBytes[19]) << 32; // fall through
        case 19: c += static_cast<u64>(lpBytes[18]) << 24; // fall through
        case 18: c += static_cast<u64>(lpBytes[17]) << 16; // fall through
        case 17: c += static_cast<u64>(lpBytes[16]) << 8;  // fall through
        case 16: b += static_cast<u64>(lpBytes[15]) << 56; // fall through
        case 15: b += static_cast<u64>(lpBytes[14]) << 48; // fall through
        case 14: b += static_cast<u64>(lpBytes[13]) << 40; // fall through
        case 13: b += static_cast<u64>(lpBytes[12]) << 32; // fall through
        case 12: b += static_cast<u64>(lpBytes[11]) << 24; // fall through
        case 11: b += static_cast<u64>(lpBytes[10]) << 16; // fall through
        case 10: b += static_cast<u64>(lpBytes[9]) << 8;   // fall through
        case 9:  b += static_cast<u64>(lpBytes[8]);        // fall through
        case 8:  a += static_cast<u64>(lpBytes[7]) << 56;  // fall through
        case 7:  a += static_cast<u64>(lpBytes[6]) << 48;  // fall through
        case 6:  a += static_cast<u64>(lpBytes[5]) << 40;  // fall through
        case 5:  a += static_cast<u64>(lpBytes[4]) << 32;  // fall through
        case 4:  a += static_cast<u64>(lpBytes[3]) << 24;  // fall through
        case 3:  a += static_cast<u64>(lpBytes[2]) << 16;  // fall through
        case 2:  a += static_cast<u64>(lpBytes[1]) << 8;   // fall through
        case 1:  a += static_cast<u64>(lpBytes[0]);        // fall through
        case 0:  break;
    }
    Mix64(a, b, c);
    return c;
}

// The 32-bit ::Attribute::Key forms declared in AttributeKey.h: the same hash,
// truncated to the low doubleword exactly as the 32-bit consumers do.
::Attribute::Key StringToKey(const char* lpcText, u32 luLength, u64 luSeed)
{
    return static_cast<::Attribute::Key>(StringToKey64(lpcText, luLength, luSeed));
}

// @ 0x82805828 -- NUL-terminated convenience form (strlen + the baked seed).
::Attribute::Key StringToKey(const char* lpcText)
{
    return StringToKey(lpcText,
                       static_cast<u32>(strlen(lpcText)),
                       KU_ATTRIB_STRING_TO_KEY_SEED);
}
}
