// =====================================================================================
// CEMBMVQ -- embedded motion-vector / inter-block quantiser workspace for the WMV
// video-object encoder.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CEMBMVQ::CEMBMVQ            @0x82CCB538   (called by CWMVideoObjectEncoder::CWMVideoObjectEncoder)
//   CEMBMVQ::getNewInterVBuffer @0x82AACCB8
//
// CEMBMVQ owns one large, flat workspace object. Only the offsets the asm actually
// touches are attested; the object is modelled as a fixed byte block plus typed views
// onto the attested regions. Every displacement below is the true byte offset from
// `this` (Hex-Rays' _DWORD* /4 indexing has been undone).
//
// Attested layout (byte offsets from this):
//   +0x0168  aligned pointer  = (this + 0x0026) & ~0xF   (16-byte-aligned scratch base)
//   +0x03A8  512 x 16-byte free "inter-V block" cells         (stride 0x10)
//   +0x23A8  512 pointers, each initialised to &cell16[i]
//   +0x2CA8  u32 = 511  (top index of the 12-byte free list)
//   +0x2CAC  512 x 12-byte cells                                (stride 0x0C)
//   +0x44AC  512 pointers, each initialised to &cell12[i]
//   +0x4CAC  256-byte region (memset 0 on wrap in getNewInterVBuffer)
//   +0x4DAC  u32  free-list index into the 12-byte pointer table (starts 511)
//   +0x4DB0  u32  round-robin buffer counter (0..8), advanced each getNewInterVBuffer
//   +0x4DB4  16-byte pattern table A (memcpy of {2,2,1,1, 3,3,3,3, 0,0,0,0, 2,2,1,1})
//   +0x4DC4  16-byte pattern table B (memcpy of {1,1,2,2, 0,0,0,0, 3,3,3,3, 1,1,2,2})
//   +0x73F4  aligned pointer  = (this + 0x4DF2) & ~0xF   (base of the inter-V buffer
//            arena; buffers are handed out at base + 0x4C0 * index, stride 1216)
// =====================================================================================

#include <cstring>

#include "types.hpp"

// Free-list / pointer-table capacity (both tables hold 512 entries).
static const int KI_EMBMVQ_TABLE_COUNT = 512;

// Stride of the two cell arrays and the arena buffers.
static const u32 KU_EMBMVQ_CELL16_STRIDE = 0x10;   // +0x03A8 array element size
static const u32 KU_EMBMVQ_CELL12_STRIDE = 0x0C;   // +0x2CAC array element size
static const u32 KU_EMBMVQ_VBUFFER_STRIDE = 0x4C0; // 1216 bytes per inter-V buffer

// How many buffers are dispensed before the arena wraps (counter compared > 7).
static const int KI_EMBMVQ_VBUFFERS_PER_ARENA = 8;

class CEMBMVQ
{
public:
    CEMBMVQ(int a2, int a3, int a4, int a5);

    // Returns the next 1216-byte inter-V buffer from the arena, wrapping (and
    // rebuilding the 12-byte free-list) once eight have been handed out.
    void* getNewInterVBuffer();

private:
    // Byte-addressable views onto the flat workspace. The base is treated as a u8*
    // so every displacement below is a literal byte offset.
    u8* Base() { return reinterpret_cast<u8*>(this); }

    u32& U32At(u32 luByteOffset)
    {
        return *reinterpret_cast<u32*>(Base() + luByteOffset);
    }

    // The pointer tables store host pointers; on disk (X360) these were 32-bit, but
    // faithfulness here is store-for-store within the object, so a pointer view is used.
    void*& PtrAt(u32 luByteOffset)
    {
        return *reinterpret_cast<void**>(Base() + luByteOffset);
    }

    // The workspace is one flat block. Its full extent is not independently attested;
    // it must reach at least past the +0x73F4 arena-base pointer plus the arena the
    // ctor aligns from (+0x4DF2). Sized to comfortably cover every attested access.
    u8 maWorkspace[0x8000];

    // --- Attested named offsets (documentation; access goes through the byte views) ---
    // +0x0168 aligned scratch pointer
    // +0x2CA8 u32 top index (=511)
    // +0x4DAC u32 free-list index (=511)
    // +0x4DB0 u32 round-robin counter
    // +0x73F4 aligned arena base pointer
};

// Offsets used by both methods.
static const u32 KU_OFF_ALIGNED_SCRATCH   = 0x0168; // (this+0x26)&~0xF
static const u32 KU_OFF_CELL16_ARRAY      = 0x03A8;
static const u32 KU_OFF_CELL16_PTRTABLE   = 0x23A8;
static const u32 KU_OFF_CELL12_TOPINDEX   = 0x2CA8;
static const u32 KU_OFF_CELL12_ARRAY      = 0x2CAC;
static const u32 KU_OFF_CELL12_PTRTABLE   = 0x44AC;
static const u32 KU_OFF_WRAP_CLEAR_REGION = 0x4CAC; // memset 0, 256 bytes
static const u32 KU_OFF_FREELIST_INDEX    = 0x4DAC;
static const u32 KU_OFF_ROUNDROBIN_COUNT  = 0x4DB0;
static const u32 KU_OFF_PATTERN_TABLE_A   = 0x4DB4;
static const u32 KU_OFF_PATTERN_TABLE_B   = 0x4DC4;
static const u32 KU_OFF_ARENA_BASE_PTR    = 0x73F4; // (this+0x4DF2)&~0xF

// CEMBMVQ::CEMBMVQ @0x82CCB538.
CEMBMVQ::CEMBMVQ(int /*a2*/, int /*a3*/, int /*a4*/, int /*a5*/)
{
    u8* lpBase = Base();

    // Loop 1: initialise the 512-entry pointer table at +0x23A8 to reference the
    // 512 x 16-byte cells starting at +0x03A8 (stride 0x10).
    {
        void** lppTable = reinterpret_cast<void**>(lpBase + KU_OFF_CELL16_PTRTABLE);
        u8* lpCell = lpBase + KU_OFF_CELL16_ARRAY;
        for (int liEntry = 0; liEntry < KI_EMBMVQ_TABLE_COUNT; ++liEntry)
        {
            *lppTable++ = lpCell;
            lpCell += KU_EMBMVQ_CELL16_STRIDE;
        }
    }

    // Top index of the 12-byte free list.
    U32At(KU_OFF_CELL12_TOPINDEX) = 511;

    // Loop 2: initialise the 512-entry pointer table at +0x44AC to reference the
    // 512 x 12-byte cells starting at +0x2CAC (stride 0x0C).
    {
        void** lppTable = reinterpret_cast<void**>(lpBase + KU_OFF_CELL12_PTRTABLE);
        u8* lpCell = lpBase + KU_OFF_CELL12_ARRAY;
        for (int liEntry = 0; liEntry < KI_EMBMVQ_TABLE_COUNT; ++liEntry)
        {
            *lppTable++ = lpCell;
            lpCell += KU_EMBMVQ_CELL12_STRIDE;
        }
    }

    // Free-list index (top of the 12-byte pointer table).
    U32At(KU_OFF_FREELIST_INDEX) = 511;

    // Two 16-byte-aligned scratch pointers derived from raw interior addresses.
    // (X360 stored these as 32-bit words; on the 64-bit host they are kept as full
    // pointers so the arena base round-trips through getNewInterVBuffer.)
    PtrAt(KU_OFF_ALIGNED_SCRATCH) = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(lpBase + 0x0026) & ~static_cast<uintptr_t>(0xF));
    PtrAt(KU_OFF_ARENA_BASE_PTR) = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(lpBase + 0x4DF2) & ~static_cast<uintptr_t>(0xF));

    // Pattern table A @ +0x4DB4 -- 16 bytes.
    {
        const u8 laPatternA[16] = { 2, 2, 1, 1, 3, 3, 3, 3, 0, 0, 0, 0, 2, 2, 1, 1 };
        memcpy(lpBase + KU_OFF_PATTERN_TABLE_A, laPatternA, sizeof(laPatternA));
    }

    // Pattern table B @ +0x4DC4 -- 16 bytes.
    {
        const u8 laPatternB[16] = { 1, 1, 2, 2, 0, 0, 0, 0, 3, 3, 3, 3, 1, 1, 2, 2 };
        memcpy(lpBase + KU_OFF_PATTERN_TABLE_B, laPatternB, sizeof(laPatternB));
    }
}

// CEMBMVQ::getNewInterVBuffer @0x82AACCB8.
void* CEMBMVQ::getNewInterVBuffer()
{
    u8* lpBase = Base();

    int liCounter = static_cast<int>(U32At(KU_OFF_ROUNDROBIN_COUNT));
    u8* lpArena = reinterpret_cast<u8*>(PtrAt(KU_OFF_ARENA_BASE_PTR));
    void* lpResult = lpArena + KU_EMBMVQ_VBUFFER_STRIDE * static_cast<u32>(liCounter);

    if (liCounter > (KI_EMBMVQ_VBUFFERS_PER_ARENA - 1))
    {
        // The arena is full: rebuild the tail of the 12-byte pointer table, clear the
        // 256-byte region, reset both counters and hand out buffer 0.
        int liIndex = static_cast<int>(U32At(KU_OFF_FREELIST_INDEX));
        if (liIndex < KI_EMBMVQ_TABLE_COUNT)
        {
            // Rewire pointer-table entries [liIndex .. 512) to reference their cells:
            //   table  = &cell12Ptr[liIndex]   (base +0x44AC, index 4395 words + liIndex)
            //   cell   = &cell12[liIndex]       (base +0x2CAC, (953 + liIndex) * 3 words)
            void** lppTable =
                reinterpret_cast<void**>(lpBase + KU_OFF_CELL12_PTRTABLE) + liIndex;
            u8* lpCell = lpBase + KU_OFF_CELL12_ARRAY +
                         KU_EMBMVQ_CELL12_STRIDE * static_cast<u32>(liIndex);
            for (int liRemaining = KI_EMBMVQ_TABLE_COUNT - liIndex;
                 liRemaining != 0; --liRemaining)
            {
                *lppTable++ = lpCell;
                lpCell += KU_EMBMVQ_CELL12_STRIDE;
            }
        }

        U32At(KU_OFF_FREELIST_INDEX) = 511;
        memset(lpBase + KU_OFF_WRAP_CLEAR_REGION, 0, 256);
        U32At(KU_OFF_ROUNDROBIN_COUNT) = 0;
        lpResult = reinterpret_cast<u8*>(PtrAt(KU_OFF_ARENA_BASE_PTR));
    }

    ++U32At(KU_OFF_ROUNDROBIN_COUNT);
    return lpResult;
}
