#pragma once

// ===========================================================================
// EATech Apt -- AptActionQueueC: the interpreter's circular action deque.
//
// While APT interprets a frame it queues up the clip-event handlers and the
// ActionScript function calls that the frame schedules, then drains them in
// order (front-to-back) once the display-list pass is complete. The queue is a
// fixed-capacity ring buffer of 20-byte "pool data" slots; each slot is either
// an *action* (a clip-event to run against an AptCIH) or a *function* (an
// ActionScript closure call). Slots hold ref-counted AptValue pointers, so the
// queue AddRefs on enqueue and Releases on clear; the GC mark walk
// (RegisterReferences) registers the held pointers so they survive collection.
//
// NO DWARF / Feb-2007 source exists for this class (X360-only; not in the PS3
// DecFIGS dump nor the EATech public Apt.h, and no wiki entry). All shape below
// is derived STRICTLY from the X360 BURNOUT_X360_ARTIST.XEX disassembly:
//     AptActionQueueC::AptActionQueueC        @ 0x82AE6780  (ctor)
//     AptActionQueueC::ClearActions           @ 0x82ADF1D8
//     AptActionQueueC::AddActionBack          @ 0x82AD94B0
//     AptActionQueueC::AddActionFront         @ 0x82AD9548
//     AptActionQueueC::AddFunctionBack        @ 0x82AD95F8
//     AptActionQueueC::AddFunctionFront       @ 0x82AD96A8
//     AptActionQueueC::RemoveActionFor        @ 0x82ADF2A0
//     AptActionQueueC::RegisterReferences     @ 0x82AD97B0
//     AptActionQueueC::GetDequeLocation       @ 0x82AD9758
//     AptActionQueueC::DecrementDequeLocation @ 0x82AD5E68
//     AptActionQueueC::GetNextItem            @ 0x82ADC5E0
//     AptActionQueueC::IsLastItem             @ 0x82AD5E08
//     AptActionQueueC::IsLastItemOrBeyond     @ 0x82AD5E20
//     AptActionQueueC::SetCurItem             @ 0x82AD5E60
//
// LAYOUT (the queue control block; X360 dword offsets reproduced from the ctor
// and the pointer-arithmetic in every accessor). Per the project x64
// semantic-parity rule the queue is modelled by NAMED MEMBERS, not byte offsets
// (PC pointers are 8 bytes, so the PC layout legitimately differs); the X360
// offsets are recorded in comments only:
//     +0x00 mpBegin     ring buffer base (first slot)          (a1[0] / *a1)
//     +0x04 mpFront     front slot pointer (next to dequeue)   (a1[1])
//     +0x08 mpBack      back slot pointer (one past last)      (a1[2])
//     +0x0C mpCurItem   "current item" cursor (SetCurItem)     (a1[3])
//     +0x10 mnCapacity  slot capacity (== a2 of the ctor)      (a1[4])
//
// The ring is allocated with capacity+1 slots (front == back means empty), from
// the shared Apt fixed-size DOGMA pool (gpAptPseudoDataPool == X360 off_8324D808,
// the same pool the other Apt display-list nodes use); the allocation stores its
// own slot-count in the dword preceding mpBegin and is freed by the pool.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API
// exception): AptActionQueueC, AddActionBack, ClearActions, ...
// ===========================================================================

#include <cstddef>   // offsetof (_AssertLayout)

#include "types.hpp"

class AptValue;
struct AptCIH;

// ---------------------------------------------------------------------------
// AptAnimationPoolData -- one 20-byte (5-dword) queue slot. The slot is a small
// X360-only tagged union; field meaning depends on mnType. The member NAMES are
// the X360's own (the GC debug strings the binary embeds are
// "AptAnimationPoolData::action.pCIH", "AptAnimationPoolData::function.pContext"
// and "AptAnimationPoolData::function.pFuncDef"). No DWARF/Feb home exists, so
// the shape is reconstructed from the X360 stores; modelled with named members,
// the union mirroring the two slot kinds the X360 builds and tears down.
// ---------------------------------------------------------------------------
struct AptAnimationPoolData
{
    enum E_AptActionType
    {
        E_ACTION_TYPE_EMPTY    = 0,   // slot cleared / available
        E_ACTION_TYPE_ACTION   = 1,   // a clip-event action to run on a CIH
        E_ACTION_TYPE_FUNCTION = 2,   // an ActionScript function call
    };

    // x64 slot shape (the ring stride is PROVEN 0x20 -- every deque accessor does
    // `shl reg,5` @0x140835BC0/0x140839630/0x14083A650; B4Extern's AptActionPool
    // corroborates field-for-field): the +0x04 int sits OUTSIDE the union (B4
    // 'input'), the union starts 8-aligned at +0x08 with {int,ptr,ptr} action /
    // {ptr,ptr,int} function views. (The earlier shape nested the +0x04 int inside
    // the union, compiling to a 40-byte slot -- a live ring-stride corruption.)
    s32 mnType;    // +0x00  E_AptActionType tag
    s32 mnInput;   // +0x04  B4 'input' -- the action's interpreter-context handle (a4)
                   //        / the function's forwarded arg-count handle (a5)

    union
    {
        // ---- action view (mnType == E_ACTION_TYPE_ACTION) -----------------
        struct
        {
            s32       miDepth;     // +0x08  instance depth snapshot at queue time (B4 nFrame)
            // The queued "event id" is really the ADDRESS of the command record's
            // action-stream pointer slot (console: queueFrameActions passes cmd+4;
            // RunActions dereferences it TWICE -- `**(v3+12)`).
            const void* mpEventStreamSlot;   // +0x10 (B4 pBlock)
            AptValue* mpCIH;       // +0x18  the CIH (GC ref; B4 pCIH)
        } action;

        // ---- function view (mnType == E_ACTION_TYPE_FUNCTION) -------------
        struct
        {
            AptValue* mpContext;   // +0x08  call "this"/context value (GC ref)
            AptValue* mpFuncDef;   // +0x10  the function-definition value (GC ref)
            s32       miReturnReg; // +0x18  result register / handle (B4 nParams)
        } function;
    };

    // Layout pinned against the x64 XB1 export (never called; member body gives
    // complete-class offsetof context).
    static void _AssertLayout()
    {
        static_assert(offsetof(AptAnimationPoolData, mnInput) == 0x04, "B4: 'input' lives OUTSIDE the union");
        static_assert(sizeof(AptAnimationPoolData) == 0x20, "x64 ring stride: `shl reg,5` in every deque accessor");
    }
};

// ---------------------------------------------------------------------------
// AptActionQueueC -- the circular action deque control block.
// ---------------------------------------------------------------------------
class AptActionQueueC
{
public:
    // @ 0x82AE6780 -- allocate the ring (capacity+1 slots; the +1 distinguishes
    // full from empty) from the shared Apt DOGMA pool, point all three cursors at
    // the first slot, and clear it. The capacity*20 byte size is clamped if the
    // requested count would overflow. Returns *this (X360 fastcall).
    explicit AptActionQueueC(u32 nCapacity);

    // @ 0x82ADF1D8 -- drain the queue: walk every live slot front-to-back,
    // Release the GC pointers it holds (the CIH for an action; the context +
    // func-def for a function), mark it empty, and reset front == back == begin.
    void ClearActions();

    // @ 0x82AD94B0 -- enqueue an action at the back. No-op if the ring is full
    // (advancing back would collide with front). AddRefs the CIH. Returns the CIH
    // (X360 fastcall return value).
    // pEventStreamSlot = the command record's action-stream pointer slot address
    // (console s32 event id; pointer-width on x64 -- see AptAnimationPoolData).
    AptValue* AddActionBack(const void* pEventStreamSlot, AptCIH* pCIH, s32 iContext);

    // @ 0x82AD9548 -- enqueue an action at the front (push onto the head). Mirrors
    // AddActionBack but decrements front. AddRefs the CIH.
    AptValue* AddActionFront(const void* pEventStreamSlot, AptCIH* pCIH, s32 iContext);

    // @ 0x82AD95F8 -- enqueue a function call at the back. No-op if the ring is
    // full. AddRefs both the context and the func-def values.
    AptValue* AddFunctionBack(AptValue* pContext, AptValue* pFuncDef,
                              s32 iReturnReg, s32 iArgCount);

    // @ 0x82AD96A8 -- enqueue a function call at the front. AddRefs both held
    // values.
    AptValue* AddFunctionFront(AptValue* pContext, AptValue* pFuncDef,
                               s32 iReturnReg, s32 iArgCount);

    // @ 0x82ADF2A0 -- remove the first live action queued for a given CIH context
    // (matching action slots whose mpCIH == pContext, excluding the current
    // item), Releasing its CIH and closing the gap with a memmove. Returns the
    // context (X360 fastcall return value).
    AptValue* RemoveActionFor(AptValue* pContext);

    // @ 0x82AD97B0 -- GC mark walk: register every held AptValue pointer with the
    // collector so the queued actions/functions survive a collection.
    void RegisterReferences();

    // @ 0x82AD9758 -- address of the logical slot `nIndex` positions ahead of the
    // front, wrapping around the ring (handles a negative wrapped index).
    AptAnimationPoolData* GetDequeLocation(s32 nIndex);

    // @ 0x82AD5E68 -- the slot one position before `pSlot`, wrapping to the last
    // slot when stepping below mpBegin.
    AptAnimationPoolData* DecrementDequeLocation(AptAnimationPoolData* pSlot);

    // @ 0x82ADC5E0 -- the slot one position after `pSlot`, wrapping to mpBegin
    // when stepping past the last slot.
    AptAnimationPoolData* GetNextItem(AptAnimationPoolData* pSlot);

    // @ 0x82AD5E08 -- true iff `pSlot` is exactly the back of the queue.
    bool IsLastItem(AptAnimationPoolData* pSlot) const;

    // @ 0x82AD5E20 -- true iff `pSlot` is at or past the back, accounting for ring
    // wrap-around (front > back).
    bool IsLastItemOrBeyond(AptAnimationPoolData* pSlot) const;

    // @ 0x82AD5E60 -- set the "current item" cursor (mpCurItem). Returns *this
    // (X360 fastcall return value).
    AptActionQueueC* SetCurItem(AptAnimationPoolData* pSlot);

    AptAnimationPoolData* mpBegin;     // +0x00
    AptAnimationPoolData* mpFront;     // +0x04
    AptAnimationPoolData* mpBack;      // +0x08
    AptAnimationPoolData* mpCurItem;   // +0x0C
    u32                   mnCapacity;  // +0x10
};
