#pragma once

// ===========================================================================
// XAUDIO::CEffectManager -- the XAudio effect-factory manager in
// BURNOUT_X360_ARTIST.XEX. It owns two id-indexed factory tables (a fixed
// "builtin" table sized at construction and a dynamically-grown "registered"
// table) and, given a per-effect request whose first byte is the effect type
// id, dispatches to the matching factory to query an effect's size or create
// it. This header is the canonical OWNING home for:
//
//     XAUDIO::CEffectManager::CEffectManager             @ 0x82C2F480
//     XAUDIO::CEffectManager::QueryInterface             @ 0x82C2F078
//     XAUDIO::CEffectManager::QueryEffectSize            @ 0x82C2F090
//     XAUDIO::CEffectManager::CreateEffect               @ 0x82C2F118
//     XAUDIO::CEffectManager::RegisterEffects            @ 0x82C2F1A0
//     XAUDIO::CEffectManager::UnregisterEffects          @ 0x82C2F3A8
//     XAUDIO::CEffectManager::GetObjectAdditionalSize    @ 0x82C2F420
//     XAUDIO::CEffectManager::`vector deleting destructor' @ 0x82C2F598
//
// There is no reference source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 asm. `XAUDIO` is an external middleware
// boundary, so its API identifiers are preserved verbatim per the naming
// convention; reconstructed members use the project Hungarian.
//
// LAYOUT (X360 32-bit byte offsets, grounded in the asm load/store
// displacements; pointer members widened to x64 -- the exact byte offsets are
// an X360 compiler artifact, semantic parity is by named member):
//   +0x00  vptr             -- polymorphic; the ctor seats CEffectManager's own
//                              vtable (off_821B553C), the destructor reseats it
//                              then the shared XAudio base image (off_82108FF4).
//   +0x04  miRefCount       -- intrusive ref count; ctor seeds 1 (not otherwise
//                              read by this TU).
//   +0x08  muBuiltinCount   -- slot count of the builtin table (== highest
//                              builtin effect id + 1; the table is indexed
//                              directly by id, so it may contain holes).
//   +0x0C  mpBuiltinEffects -- builtin factory table (SEffectFactory[muBuiltin-
//                              Count]); allocated at construction through the
//                              supplied allocator. Only seated when non-empty.
//   +0x10  muRegisteredCount -- slot count of the registered table.
//   +0x14  mpRegisteredEffects -- dynamically grown registered factory table
//                              (SEffectFactory[muRegisteredCount]); allocated /
//                              grown by RegisterEffects through the XAudio XMem
//                              manager, released by the destructor.
//
// The ctor initialises only the vptr, miRefCount, muBuiltinCount and (when
// non-empty) mpBuiltinEffects; muRegisteredCount / mpRegisteredEffects are left
// to the zero-initialised object image the factory (XAudioCreateEffectManager)
// allocates -- the asm performs no store to them, so none is fabricated here.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// The per-effect request descriptor handed to QueryEffectSize / CreateEffect.
// Its first byte is the effect type id (`lbz 0(a2)`); the remainder is opaque
// to the manager and forwarded verbatim to the selected factory. FLAG: only the
// leading id byte is exercised by this TU.
struct SEffectRequest
{
    u8 muEffectId; // +0x00
};

// The factory functions a registration entry supplies. QueryEffectSize
// tail-calls the query-size function with (request, arg); CreateEffect
// tail-calls the create function with (request, arg1, arg2). Argument types
// beyond the request are opaque here (forwarded register-for-register).
typedef s32 (*TEffectQuerySizeFn)(const SEffectRequest* apRequest, void* apArg);
typedef s32 (*TEffectCreateFn)(const SEffectRequest* apRequest,
                               void* apArg1, void* apArg2);

// One resolved factory slot (8 bytes) in a manager table. Populated from a
// registration entry: the query-size function at +0x00 and the create function
// at +0x04. A slot is "free" when both words are zero.
struct SEffectFactory
{
    TEffectQuerySizeFn mpfnQuerySize; // +0x00  (called by QueryEffectSize)
    TEffectCreateFn    mpfnCreate;    // +0x04  (called by CreateEffect)
};

// One effect-registration entry (12-byte stride in the X360 build): the effect
// id byte (which RegisterEffects rewrites with the assigned high id), the
// query-size function at +0x04 and the create function at +0x08.
struct SEffectRegistration
{
    u8                 muEffectId;    // +0x00
    u8                 mPad01[3];     // +0x01  alignment (X360)
    TEffectQuerySizeFn mpfnQuerySize; // +0x04
    TEffectCreateFn    mpfnCreate;    // +0x08
};

// A registration table: a byte count and the entry array (`lbz 0(t)` /
// `lwz 4(t)`). Passed (by pointer-to-pointer) to the ctor and
// GetObjectAdditionalSize, and (directly) to RegisterEffects.
struct SEffectRegistrationTable
{
    u8                   muCount;   // +0x00
    u8                   mPad01[3]; // +0x01  alignment (X360)
    SEffectRegistration* mpEntries; // +0x04
};

// The id list handed to UnregisterEffects: a byte count and a pointer to an
// array of effect id bytes (`lbzx` reads each id).
struct SEffectIdList
{
    u8        muCount;   // +0x00
    u8        mPad01[3]; // +0x01  alignment (X360)
    const u8* mpIds;     // +0x04
};

// FLAGGED: the allocator object the constructor allocates the builtin table
// through. It is reached only through vtable slot [5] (offset +0x14), an
// Allocate(size)->ptr call -- exactly the shape XAUDIO::CFrameBuffer models as
// IFrameBufferAllocator. Its concrete type is not homed; this captures just
// that one virtual call so the dispatch shape is faithful.
class IEffectManagerAllocator
{
public:
    struct VTable
    {
        void* mpSlot00; // [0]
        void* mpSlot04; // [1]
        void* mpSlot08; // [2]
        void* mpSlot0C; // [3]
        void* mpSlot10; // [4]
        // [5] @+0x14 -- Allocate(this, byteSize) -> raw pointer.
        void* (*mpAllocate)(IEffectManagerAllocator* apSelf, u32 uSize);
    };

    const VTable* mpVTable; // +0x00
};

class CEffectManager
{
public:
    // @ 0x82C2F480 -- seat miRefCount = 1, size the builtin table from the
    // registration table's highest id, allocate it through `apAllocator`, and
    // populate builtin[id] = { querySize, create } for every entry.
    CEffectManager(SEffectRegistrationTable* const* appTable,
                   IEffectManagerAllocator* apAllocator);

    // @ 0x82C2F598 -- destructor (the X360 `vector deleting destructor' entry;
    // the compiler folded the scalar/vector deleting thunks onto it and the asm
    // frees no `this`, only reseating both vtables and returning this): release
    // the dynamically-grown registered table through the XAudio XMem manager.
    virtual ~CEffectManager();

    // @ 0x82C2F078 -- hand back the object's effect-table sub-interface (the
    // object viewed from byte +8) through `*appInterface`, or null when the
    // object pointer is null; returns the object.
    void* QueryInterface(void** appInterface);

    // @ 0x82C2F090 -- resolve the factory for `apRequest`'s effect id and
    // tail-call its query-size function; returns 0x80004001 (E_NOTIMPL) when no
    // factory is registered for the id.
    s32 QueryEffectSize(const SEffectRequest* apRequest, void* apArg);

    // @ 0x82C2F118 -- resolve the factory for `apRequest`'s effect id and
    // tail-call its create function (forwarding apArg1 / apArg2); returns
    // 0x80004001 (E_NOTIMPL) when no factory is registered for the id.
    s32 CreateEffect(const SEffectRequest* apRequest, void* apArg1, void* apArg2);

    // @ 0x82C2F1A0 -- register a batch of effects: fill free holes in the
    // registered table first, then grow it (through the XAudio XMem manager) for
    // the remainder, assigning each a high id (>= 128) written back into the
    // registration entry. Returns 0 (S_OK), or 0x8007000E (E_OUTOFMEMORY) when
    // the table would exceed 128 slots or the allocation fails.
    s32 RegisterEffects(const SEffectRegistrationTable* apTable);

    // @ 0x82C2F3A8 -- clear the registered-table slot for each listed high id
    // (only when the slot is fully occupied). Returns 0.
    s32 UnregisterEffects(const SEffectIdList* apIds);

    // @ 0x82C2F420 -- static: the extra bytes an effect manager built from
    // `*appTable` needs for its builtin table = 8 * (highest id + 1); 0 when the
    // table pointer or the table it points to is null.
    static s32 GetObjectAdditionalSize(const SEffectRegistrationTable* const* appTable);

    // ----- layout (declared in X360 offset order; see the header banner) -----
    u32             miRefCount;          // +0x04
    u32             muBuiltinCount;      // +0x08
    SEffectFactory* mpBuiltinEffects;    // +0x0C
    u32             muRegisteredCount;   // +0x10
    SEffectFactory* mpRegisteredEffects; // +0x14

private:
    // The identical max-id scan the ctor and GetObjectAdditionalSize inline:
    // returns the slot count (highest effect id + 1, or 0 for an empty table)
    // needed to index a factory table directly by id.
    static u32 ComputeSlotCount(const SEffectRegistrationTable* apTable);
};

} // namespace XAUDIO
