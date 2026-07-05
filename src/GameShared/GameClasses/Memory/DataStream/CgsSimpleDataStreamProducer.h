#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h"

// CgsMemory::SimpleDataStreamProducer
// ---------------------------------------------------------------------------
// A convenience front-end that drives a DataStreamCommandPoster: it owns the
// command poster (mCommandPoster) plus cached copies of the command/result
// geometry, a result iterator, and the streaming bookkeeping. Construct() sizes
// and constructs the embedded command poster over the caller-supplied command
// buffer, then caches the geometry the producer needs to replay results.
//
// X360 home (reconstructed in this pass):
//   Construct @ 0x8286A3B0
//
// This is semantic-parity C++ (not byte-matching): members are reconstructed
// from the DWARF declaration and accessed by name. The X360 store offsets prove
// which value lands in which logical field; field NAMES follow the DWARF.
namespace CgsMemory
{
    // CgsSimpleDataStreamProducer.h:47 — read cursor over a producer's results.
    struct SimpleDataStreamResultIterator
    {
        // Construct() wires the iterator's parent back pointer directly.
        // (Additive: layout/sizeof unchanged.)
        friend struct SimpleDataStreamProducer;

        // Declared-only (not reconstructed in this TU pass).
        const void* GetFirst();
        const void* GetNext();
        const void* GetCurrent();

    private:
        s32                        miResultIndex;  // CgsSimpleDataStreamProducer.h:60
        SimpleDataStreamProducer*  mpParent;       // CgsSimpleDataStreamProducer.h:61
    };

    struct SimpleDataStreamProducer
    {
        // The iterator's GetCurrent reads the producer's private geometry
        // (mbIsStreaming/miNumAddedCommands/mpResultBuffer/miAlignedResultSize).
        // (Additive: layout/sizeof unchanged.)
        friend struct SimpleDataStreamResultIterator;

        // CgsSimpleDataStreamProducer.h:79 — geometry snapshot shared with the
        // streaming side. Not touched by Construct(); homed for completeness.
        struct SharedData
        {
            s32   miMaxCommands;       // h:81
            s32   miCommandSize;       // h:82
            s32   miMaxResults;        // h:83
            s32   miResultSize;        // h:84
            s32   miAlignedResultSize; // h:85
            void* mpResultBuffer;      // h:86
            DataStreamCommandPoster* mpPoster;  // h:87
        private:
            u32   muPad[1];            // h:89
        };

        // --- API (CgsSimpleDataStreamProducer.h:101-147) ---------------------
        // Reconstructed in this pass.
        void Construct(s32 liMaxCommands, s32 liCommandSize, void* lpCommandBuffer,
                       s32 liMaxResults, s32 liResultSize, void* lpResultBuffer);

        // Reconstructed in this pass (X360 0x8280FFF0). Static helper: the asm
        // takes all six arguments in r3-r8 with no `this` pointer (r3 is used as
        // the numeric liMaxCommands operand), so this is a static class method.
        static void GetRequiredBufferSizes(s32 liMaxCommands, s32 liCommandSize,
                                           s32 liMaxResults, s32 liResultSize,
                                           u32* lpuOutCommandBufferSize,
                                           u32* lpuOutResultBufferSize);
        void Begin();
        void End();
        s32  AddCommand(void* lpCommand);
        s32  AddCommands(void* lpCommands, s32 liCommandCount);
        s32  AllocateCommand(void** lppOutBuffer);
        s32  AllocateCommands(s32 liCommandCount, void** lppOutBuffer);
        SimpleDataStreamResultIterator GetResultIterator() const;
        s32  GetNumCommands() const;

    private:
        // --- members (CgsSimpleDataStreamProducer.h:152-163) -----------------
        SharedData                     mShared;            // h:152
        s32                            miMaxCommands;      // h:154
        s32                            miCommandSize;      // h:155
        s32                            miMaxResults;       // h:156
        s32                            miResultSize;       // h:157
        s32                            miAlignedResultSize;// h:158
        void*                          mpResultBuffer;     // h:159
        SimpleDataStreamResultIterator mResultIterator;    // h:160
        DataStreamCommandPoster        mCommandPoster;     // h:161
        bool                           mbIsStreaming;      // h:162
        s32                            miNumAddedCommands; // h:163
    };
}
