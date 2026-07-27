// ============================================================================
// GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.cpp
//
// Out-of-line symbols for the AttribSys module IO payload:
//   * AttribSysIO::Inp        @ X360 0x828042D8 (read-locked accessor)
//   * AttribSysIO::InputBuff  @ X360 0x82664188 (write-locked accessor)
//   * AttribSysRequestInterface<512>::RegisterVault  @ X360 0x82256428 (3-arg; explicit
//        member specialization -- exactly the X360-attested <512> symbol, without pulling
//        in the not-yet-reconstructed Construct/Clear/GetRequestQueue members).
//   * AttribSysRequestInterface<2048>::RegisterSchema/RegisterVault/UnregisterVault
//        @ X360 0x826730B0 / 0x8229D6C8 / 0x826731E8 (generic bodies in the Impl header,
//        emitted here via explicit member instantiation).
// ============================================================================

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSharedIOImpl.h" // generic template method bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsAttribSys
{
namespace AttribSysIO
{
    // @ 0x828042D8 -- assert the input buffer is locked for reading (IOBuffer status bit 4),
    // then hand back the embedded request queue (this+2068 == &mVaultRequestInterface, whose
    // first member mRequestQueue is the drained VariableEventQueue). Return type is the committed
    // AttribSysEventQueue* so ProcessInputs' generic GetFirstEvent/GetNextEvent path compiles.
    const AttribSysEventQueue* Inp(const InputBuffer* lpInputBuffer)
    {
        CGS_ASSERT(lpInputBuffer->IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const AttribSysEventQueue*>(
            &lpInputBuffer->mVaultRequestInterface.mRequestQueue);
    }

    // @ 0x82664188 -- assert the input buffer is locked for writing (IOBuffer status bit 3),
    // then hand back the embedded request queue (this+2068 == &mVaultRequestInterface). The
    // non-const writer variant used by the request builders taking the exclusive write lock.
    AttribSysEventQueue* InputBuff(InputBuffer* lpInputBuffer)
    {
        CGS_ASSERT(lpInputBuffer->IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<AttribSysEventQueue*>(
            &lpInputBuffer->mVaultRequestInterface.mRequestQueue);
    }

    // @ 0x82256428 -- AttribSysRequestInterface<512>::RegisterVault (DWARF
    // CgsAttribSysSharedIO.h:273). Assert the resource handle's memory pointer and the receiver
    // queue are non-NULL, then enqueue a type-0 RegisterVaultRequest into the interface's request
    // queue. Provided as an explicit member specialization so exactly the X360-attested <512>
    // symbol is emitted (an explicit `template struct ...<512>;` instantiation would also pull in
    // the not-yet-reconstructed Construct/Clear/etc. members, so it is deliberately omitted).
    //
    // X360 store map (payload @ var_60): stw r28@+0 (queue); std r26=r5@+4 (the 8-byte handle
    // rides ONE 64-bit GPR); stw r25=r6@+12 (miEventId -- a REAL parameter, not register reuse:
    // the GameData legs pass their event-slot index and the module's type-3 reply echoes this
    // field); stw r24=r7@+16 (vault type).
    template <>
    bool AttribSysRequestInterface<512>::RegisterVault(
        BaseEventReceiverQueue* lpUserReceiverQueue,
        ResourceHandle lVaultResourceHandle,
        s32 liEventId,
        EAttribSysVaultType leVaultType)
    {
        CGS_ASSERT(lVaultResourceHandle.mpResourceMemory != 0,
                   "Trying to register a vault from a NULL resource handle");
        CGS_ASSERT(lpUserReceiverQueue != 0, "lpUserReceiverQueue");

        RegisterVaultRequest lRequest;
        lRequest.mpUserReceiverQueue  = lpUserReceiverQueue;
        lRequest.mVaultResourceHandle = lVaultResourceHandle;
        lRequest.miEventId            = liEventId;
        lRequest.meVaultType          = leVaultType;

        mRequestQueue.AddEvent<RegisterVaultRequest>(&lRequest, 0);
        return true;
    }

    // ---- X360 <2048> instantiations (generic bodies from CgsAttribSysSharedIOImpl.h) ----
    // Explicit MEMBER instantiations only, so the not-yet-reconstructed sibling members
    // (Construct/Clear/GetRequestQueue/4-arg overloads) are NOT forced.
    template bool AttribSysRequestInterface<2048>::RegisterSchema(
        BaseEventReceiverQueue*, void*, s32, void*, s32);
    template bool AttribSysRequestInterface<2048>::RegisterVault(
        BaseEventReceiverQueue*, ResourceHandle, s32, EAttribSysVaultType);
    template bool AttribSysRequestInterface<2048>::UnregisterVault(
        BaseEventReceiverQueue*, ResourceHandle, s32);

    // -------- InputBuffer::AppendRequestInterface<32768> @ X360 0x82671948 --------
    // Explicit instantiation of the member template (generic body in CgsAttribSysModuleIO.h).
    // Bulk-appends a source AttribSysRequestInterface<32768>'s packed request bytes into the
    // vault request queue (this+2068), forwarding to VariableEventQueue<2048,16>::Append<32768,16>.
    // Emitted by BrnResource::GameDataModule::Update. The <2048>::Append<32768,16> destination
    // symbol is already instantiated in Module/VariableEventQueue_2048_16.cpp.
    template bool InputBuffer::AppendRequestInterface<32768>(
        const AttribSysRequestInterface<32768>*);
}
}
