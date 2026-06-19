#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// CgsSound::Playback::Name name-interning.
//
// Reconstructed from the X360 asm:
//   MakeHash @ 0x82689A50   (the rolling hash + intern-and-return)
//   Store    @ 0x82689880   (the per-bucket BST insert / collision check)
// Both methods are STATIC: their leading register is the data they operate on
// (MakeHash's r3 is the char*, Store's r3 is the hash), never a `this`. FLAG.

namespace CgsSound
{
namespace Playback
{

// ---- HashTable static-pool definitions (CgsCommon.h:312-314) ----------------
// X360 globals: dword_82FFB958 / unk_82FFDF20 / dword_82FFB9D8. Zero-initialised
// (BSS), matching the X360 -- all bucket heads null, the whole pool zero, and the
// next-free index 0.
Name::HashNode* Name::HashTable::sapHashNode[32] = { 0 };
Name::HashNode  Name::HashTable::saNodes[2048]   = { };
u32             Name::HashTable::su32CurrentNode = 0;

// ---- Interning-table sizing constants ---------------------------------------
// FLAG: KU_HASH_NODES_AVAILABLE (2048) is the node-pool cap -- the X360 `cmplwi
// r8, 0x800` guard and the `== 2048` overflow test. The overflow message names
// the original tuning knob V_CGS_SOUND_PLAYBACK_HASH_NODES_AVAILABLE.
static const u32 KU_HASH_NODES_AVAILABLE = 2048;

// FLAG: KU_HASH_BUCKETS (32) is the bucket count. The bucket index is
// (hash >> 1) & 0x1F, i.e. & (KU_HASH_BUCKETS - 1). On X360 this is fused into
// the byte-offset form `rlwinm r10, r3, 1, 25, 29` == (hash << 1) & 0x7C, which
// is ((hash >> 1) & 0x1F) * sizeof(HashNode*).
static const u32 KU_HASH_BUCKETS = 32;

// ---- Rolling-hash constants (MakeHash @ 0x82689A50) -------------------------
// FLAG: derived from the `lis/ori` immediates (0x9B6:0x6E03, 0x9B6:0x6C41,
// 0x1DE1:0x3C89) and the `^ 0x80000001` odd-forcing step (xoris 0x8000; xori 1).
static const u32 KU_HASH_SEED       = 0x9B66E03;
static const u32 KU_HASH_MULTIPLIER = 0x9B66C41;
static const u32 KU_HASH_XOR        = 0x1DE13C89;
static const u32 KU_HASH_FORCE_ODD  = 0x80000001;

// =============================================================================
// CgsSound::Playback::Name::HashTable::Store  @ 0x82689880
//
// Intern (luHash, lkpacName) into the side-table. Per the X360, returns void:
// the caller (MakeHash) discards Store's r3 and re-loads its own hash. FLAG.
// =============================================================================
void Name::HashTable::Store(uintptr_t luHash, const char* lkpacName)
{
    if (su32CurrentNode < KU_HASH_NODES_AVAILABLE)
    {
        // Bucket head for this hash. Bucket index = (hash >> 1) & (BUCKETS - 1)
        // (X360 rlwinm fold -- see KU_HASH_BUCKETS note above). FLAG.
        HashNode** lppHashNode = &sapHashNode[(luHash >> 1) & (KU_HASH_BUCKETS - 1)];

        // Walk the per-bucket BST until we hit an empty slot or the same hash.
        while (*lppHashNode)
        {
            HashNode* lpHashNode = *lppHashNode;

            if (luHash == lpHashNode->mHash)
            {
                // Same hash: the interned strings MUST be identical, else this is
                // a genuine hash collision. The X360 compares with UNSIGNED chars
                // (lbz, NOT extsb) -- this differs from MakeHash, which hashes
                // SIGNED chars. FLAG.
                const u8* lpucIn     = reinterpret_cast<const u8*>(lkpacName);
                const u8* lpucStored = reinterpret_cast<const u8*>(lpHashNode->mkpacName);
                s32 liDelta;
                for (;;)
                {
                    liDelta = static_cast<s32>(*lpucIn) - static_cast<s32>(*lpucStored);
                    if (*lpucIn == 0)
                    {
                        break;
                    }
                    ++lpucIn;
                    ++lpucStored;
                    if (liDelta != 0)
                    {
                        break;
                    }
                }

                // Static-message assert (X360 baked file/line 124; CGS_ASSERT
                // supplies __FILE__/__LINE__ -- do NOT bake the literal line for
                // static-message asserts). FLAG.
                CGS_ASSERT(liDelta == 0, "HASH COLLISION");
                return;
            }

            // less = lower hash (mpLess @ +8), more = higher hash (mpMore @ +0xC).
            lppHashNode = (luHash < lpHashNode->mHash) ? &lpHashNode->mpLess
                                                       : &lpHashNode->mpMore;
        }

        // Empty slot: take a fresh node from the pool and link it in.
        HashNode* lpNewNode = &saNodes[su32CurrentNode];
        *lppHashNode = lpNewNode;
        ++su32CurrentNode;

        // HashNode::Clear() -- INLINED on the X360 (the four word stores). FLAG.
        lpNewNode->mHash     = 0;
        lpNewNode->mkpacName = 0;
        lpNewNode->mpLess    = 0;
        lpNewNode->mpMore    = 0;

        lpNewNode->mHash     = luHash;
        lpNewNode->mkpacName = lkpacName;
        return;
    }

    // Pool exhausted. Warn exactly ONCE -- only on the first call that finds the
    // count at the cap -- then bump past the cap so it never repeats.
    if (su32CurrentNode == KU_HASH_NODES_AVAILABLE)
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            // The X360 chains three operator<< calls: literal, int(2048), literal.
            // The integer goes through the s32 overload (X360 sub_821F0E50). The
            // two string literals are copied verbatim from the asm operands
            // (aRanOutOfAvaila / aInCgssoundPlay).
            *CgsDev::Log::gpDebugPrint
                << "Ran out of available nodes ("
                << static_cast<s32>(KU_HASH_NODES_AVAILABLE)
                << ") in CgsSound::Playback::Store().\n"
                   "Don't worry though this isn't critical, it's a debugging feature, and won't affect the game.\n"
                   "Try recompiling with V_CGS_SOUND_PLAYBACK_HASH_NODES_AVAILABLE set to a higher value if you care...";
        }

        ++su32CurrentNode;
    }
}

// =============================================================================
// CgsSound::Playback::Name::MakeHash  @ 0x82689A50
//
// Compute the interning hash of a C string, force it odd, intern it, and return
// it widened to uintptr_t.
// =============================================================================
uintptr_t Name::MakeHash(const char* lkpacName)
{
    if (!lkpacName)
    {
        return 0;
    }

    // First char as a SIGNED char (X360 extsb). FLAG: signed -- contrast with
    // Store's unsigned strcmp.
    s32 liChar = static_cast<s8>(*lkpacName);
    if (liChar == 0)
    {
        return 0;
    }

    // CRITICAL: compute the rolling hash in a 32-bit local so the multiply (X360
    // mullw = low 32 bits) and xor stay 32-bit. uintptr_t is 64-bit on the PC
    // target -- doing the math in uintptr_t would change the result. The 32-bit
    // value is widened to uintptr_t only on return. FLAG.
    u32 luHash = KU_HASH_SEED;
    const char* lpkcC = lkpacName;
    do
    {
        ++lpkcC;
        luHash = (KU_HASH_MULTIPLIER * (luHash + static_cast<u32>(liChar))) ^ KU_HASH_XOR;
        liChar = static_cast<s8>(*lpkcC);
    }
    while (*lpkcC != 0);

    // Force the hash odd (low bit set) so it can never collide with a reserved /
    // null ident. X360: xoris 0x8000 ; xori 1  == ^ 0x80000001.
    if ((luHash & 1) == 0)
    {
        luHash ^= KU_HASH_FORCE_ODD;
    }

    // Static-message assert (X360 baked file/line 244/0xF4; CGS_ASSERT supplies
    // __FILE__/__LINE__ -- do NOT bake the literal line). FLAG.
    CGS_ASSERT((luHash & 1) != 0, "lHash & 1");

    HashTable::Store(luHash, lkpacName);

    return luHash;
}

}
}
