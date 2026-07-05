// ============================================================================
// b5-decomp/src/GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h
//
// Canonical (DWARF) home for the BundleLoaderModule's IO payload buffers. The three real
// buffers per the DecFIGS DWARF are InputBuffer_Update, InputBuffer_Record and OutputBuffer;
// each carries named EventQueue/PoolQueue/ResourceRequestQueue members. Every X360 accessor
// body (assert lines 78-82 / 112-113 / 142-152) matches the DWARF method table 1:1 and pins
// each member offset via each getter's `return this + OFFSET`.
//
// The three sibling .cpp files (CgsBundleLoaderModuleIO_InputBuffer.cpp,
// CgsBundleLoaderModuleIO_InputBuffer_Record.cpp, CgsBundleLoaderModuleIO_OutputBuffer.cpp,
// CgsBundleLoaderModuleIO_InputBuffer_Update.cpp) are the ONLY consumers of this header and
// are migrated in lockstep to the DWARF getter names. `InputBuffer` is retained as an alias
// of InputBuffer_Update so the historical GetEventQueue() accessor keeps resolving.
//
// Buffers (all : CgsModule::IOBuffer, 1-byte FlagSet status @+0; payload starts at +4):
//   InputBuffer_Update  (DWARF :66)  -- LoadBundleRequestQueue @+4, UnloadBundleRequestQueue @+0x9410
//   InputBuffer_Record  (DWARF :100) -- PoolReceiveQueue @+4
//   OutputBuffer        (DWARF :130) -- PoolSendQueue @+4, LoadBundleResponseQueue @+0x1014,
//                                       UnloadBundleResponseQueue @+0xA420, StreamRequestQueue @+0x1342C
//
// FLAG (foreign payload types): the queue element/body types (EventQueue<...,256>,
// PoolQueueTemplate<4096>, ResourceRequestQueue<256>) are modelled as correctly-PLACED opaque
// byte spans sized from the X360-attested inter-member offsets, so each getter's `return &member`
// reproduces the exact X360 `this + OFFSET`. mStreamRequestQueue lands at +0x1342C (== the
// previously committed mStream offset, so the object size is unchanged). We do NOT byte-match;
// only the return offsets are pinned. Adopt the named DWARF typedefs additively when their
// homes land.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer

namespace CgsResource
{
namespace BundleLoaderIO
{
    // DWARF :66 -- the bundle-loader per-frame input (Update side). Producer write-locks it and
    // fills the two request queues; the loader read-locks and drains them.
    struct InputBuffer_Update : public CgsModule::IOBuffer
    {
        // FLAG: foreign payload types (EventQueue<LoadBundleRequest,256> / <UnloadBundleRequest,256>).
        // Opaque, sized to the X360 inter-member offset (0x4 .. 0x9410 == 0x940C bytes).
        struct LoadBundleRequestQueue   { unsigned char maBytes[0x940C]; };   // DWARF :86, +0x0004
        struct UnloadBundleRequestQueue { unsigned char maBytes[4]; };        // DWARF :87, +0x9410 (tail; real size elsewhere)

        // Historical alias for the previously committed GetEventQueue accessor (== the load
        // bundle request queue at +4).
        typedef LoadBundleRequestQueue EventQueueStorage;

        void Construct();   // DWARF :71 (deferred)
        void Destruct();    // DWARF :75 (deferred)

        // @0x828E1D48 read-lock -> &mLoadBundleRequestQueue (this + 4).       (:78, committed)
        const EventQueueStorage*        GetEventQueue() const;
        // @0x828E1DF0 write-lock -> &mLoadBundleRequestQueue (this + 4).      (:79)
        LoadBundleRequestQueue*         GetLoadBundleRequestQueue();
        // @0x828E1E98 read-lock -> &mUnloadBundleRequestQueue (this + 0x9410). (:81)
        const UnloadBundleRequestQueue* GetUnloadBundleRequestQueue() const;
        // @0x828E1F40 write-lock -> &mUnloadBundleRequestQueue (this + 0x9410).(:82)
        UnloadBundleRequestQueue*       GetUnloadBundleRequestQueue();

        static void _AssertLayout();

    private:
        u8                       maStatusPad[3];             // +1..+3 (force +4 placement)
        LoadBundleRequestQueue   mLoadBundleRequestQueue;    // +0x0004  (DWARF :86)
        UnloadBundleRequestQueue mUnloadBundleRequestQueue;  // +0x9410  (DWARF :87)
    };

    // Historical name kept as an alias so committed consumers referencing
    // BundleLoaderIO::InputBuffer keep resolving (the DWARF class is InputBuffer_Update).
    typedef InputBuffer_Update InputBuffer;

    // DWARF :100 -- the loader's per-frame pool-receive input record.
    struct InputBuffer_Record : public CgsModule::IOBuffer
    {
        // FLAG: foreign payload type (PoolQueueTemplate<4096>). Opaque, placed at +4.
        struct PoolReceiveQueue { unsigned char maBytes[4]; };   // DWARF :117, +0x0004

        // Historical alias for the previously committed GetRecord accessor.
        typedef PoolReceiveQueue RecordStorage;

        void Construct();   // DWARF :105 (deferred)
        void Destruct();    // DWARF :109 (deferred)

        // @0x828E1FE8 read-lock -> &mPoolReceiveQueue (this + 4). (:112)
        const PoolReceiveQueue* GetPoolReceiveQueue() const;
        // @0x828E2090 write-lock -> &mPoolReceiveQueue (this + 4). (:113)
        PoolReceiveQueue*       GetPoolReceiveQueue();
        // Historical accessors (same +4 member) kept for committed consumers.
        const RecordStorage*    GetRecord() const;   // @0x828E1FE8 read-lock
        RecordStorage*          GetRecord();          // @0x828E2090 write-lock

        static void _AssertLayout();

    private:
        u8               maStatusPad[3];      // +1..+3 (force +4 placement)
        PoolReceiveQueue mPoolReceiveQueue;   // +0x0004  (DWARF :117)
    };

    // DWARF :130 -- the loader's output buffer. Loader write-locks it and fills the pool-send +
    // response queues + stream-request queue; the consumer read-locks and drains them.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // FLAG: foreign payload types (PoolQueueTemplate<4096> / EventQueue<...Response,256> /
        // ResourceRequestQueue<256>). Opaque, each sized to the X360 inter-member offset so every
        // getter's return address == this + attested-offset.
        struct PoolSendQueue             { unsigned char maBytes[0x1010]; };   // DWARF :156, +0x0004 .. +0x1014
        struct LoadBundleResponseQueue   { unsigned char maBytes[0x940C]; };   // DWARF :157, +0x1014 .. +0xA420
        struct UnloadBundleResponseQueue { unsigned char maBytes[0x900C]; };   // DWARF :158, +0xA420 .. +0x1342C
        struct StreamRequestQueue        { unsigned char maBytes[4]; };        // DWARF :160, +0x1342C (tail; real size elsewhere)

        // Historical aliases for the previously committed GetPool/GetStream accessors.
        typedef PoolSendQueue      PoolStorage;
        typedef StreamRequestQueue StreamStorage;

        void Construct();   // DWARF :135 (deferred)
        void Destruct();    // DWARF :139 (deferred)

        // @(:142) read-lock  -> &mPoolSendQueue (this + 4).                       (:142, deferred)
        const PoolSendQueue*             GetPoolSendQueue() const;
        // @0x828E21E0 write-lock -> &mPoolSendQueue (this + 4).                   (:143, committed)
        PoolSendQueue*                   GetPoolSendQueue();
        // @0x828E2288 read-lock  -> &mLoadBundleResponseQueue (this + 0x1014).    (:145)
        const LoadBundleResponseQueue*   GetLoadBundleResponseQueue() const;
        // @0x828E2330 write-lock -> &mLoadBundleResponseQueue (this + 0x1014).    (:146)
        LoadBundleResponseQueue*         GetLoadBundleResponseQueue();
        // @0x828E23D8 read-lock  -> &mUnloadBundleResponseQueue (this + 0xA420).  (:148)
        const UnloadBundleResponseQueue* GetUnloadBundleResponseQueue() const;
        // @0x828E2480 write-lock -> &mUnloadBundleResponseQueue (this + 0xA420).  (:149)
        UnloadBundleResponseQueue*       GetUnloadBundleResponseQueue();
        // @0x828E2528 read-lock  -> &mStreamRequestQueue (this + 0x1342C).        (:151, committed)
        const StreamRequestQueue*        GetStreamRequestQueue() const;
        // @(:152) write-lock -> &mStreamRequestQueue.                            (:152, deferred)
        StreamRequestQueue*              GetStreamRequestQueue();

        // Historical accessors (same members) kept for committed consumers.
        PoolStorage*                     GetPool();          // @0x828E21E0 write-lock -> mPoolSendQueue
        const StreamStorage*             GetStream() const;  // @0x828E2528 read-lock  -> mStreamRequestQueue

        static void _AssertLayout();

    private:
        u8                        maStatusPad[3];              // +1..+3 (force +4 placement)
        PoolSendQueue             mPoolSendQueue;              // +0x0004  (DWARF :156)
        LoadBundleResponseQueue   mLoadBundleResponseQueue;    // +0x1014  (DWARF :157)
        UnloadBundleResponseQueue mUnloadBundleResponseQueue;  // +0xA420  (DWARF :158)
        StreamRequestQueue        mStreamRequestQueue;         // +0x1342C (DWARF :160)
    };
}
}
