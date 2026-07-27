#ifndef GAMESHARED_GAMECLASSES_SYSTEM_ATTRIBSYS_CGSATTRIBSYSSHAREDIOIMPL_H
#define GAMESHARED_GAMECLASSES_SYSTEM_ATTRIBSYS_CGSATTRIBSYSSHAREDIOIMPL_H

// ============================================================================
// GameShared/GameClasses/System/AttribSys/CgsAttribSysSharedIOImpl.h
//
// The generic template<s32 N> method bodies for AttribSysRequestInterface<N>. Included
// by CgsAttribSysModuleIO.cpp, which then emits the X360-attested instantiations
// (<2048> RegisterSchema/RegisterVault/UnregisterVault). The <512> RegisterVault is an
// explicit MEMBER SPECIALIZATION (in the .cpp), so it is NOT defined here.
//
// Each builder assembles a typed request event on the stack (fields set BY NAME in the
// X360 store order) and forwards to mRequestQueue.AddEvent<EventT>(&event, typeId) -- the
// typed convenience overload of VariableEventQueue that fills liSize == sizeof(EventT).
// The type ids match the module drain switch: 0 RegisterVault, 1 RegisterSchema,
// 2 UnregisterVault.
// ============================================================================

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSharedIO.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsAttribSys
{
namespace AttribSysIO
{
    // -------- RegisterSchema @ X360 0x826730B0 (<2048>) --------
    // Builds a RegisterSchemaRequest (event type id 1) on the stack in the X360 store
    // order (recv, vltData, vltSize, binData, binSize) and submits it to the request
    // queue via the typed AddEvent<EventT> overload.
    template <s32 N>
    bool AttribSysRequestInterface<N>::RegisterSchema(
            CgsModule::BaseEventReceiverQueue* lpUserReceiverQueue,
            void* lpSchemaVltData, s32 liSchemaVltDataSize,
            void* lpSchemaBinData, s32 liSchemaBinDataSize)
    {
        CGS_ASSERT(lpUserReceiverQueue != 0, "lpEventReceiverQueue != NULL");
        CGS_ASSERT(lpSchemaVltData != 0, "lpSchemaVltData != NULL");
        CGS_ASSERT(liSchemaVltDataSize > 0, "liSchemaVltDataSize > 0");
        CGS_ASSERT(lpSchemaBinData != 0, "lpSchemaBinData != NULL");
        CGS_ASSERT(liSchemaBinDataSize > 0, "liSchemaBinDataSize > 0");
        CGS_ASSERT(lpUserReceiverQueue != 0, "lpUserReceiverQueue");

        RegisterSchemaRequest lEvent;
        lEvent.mpUserReceiverQueue  = lpUserReceiverQueue;   // +0x00 (var_60)
        lEvent.mpSchemaVltData      = lpSchemaVltData;       // +0x04 (var_5C)
        lEvent.miSchemaVltDataSize  = liSchemaVltDataSize;   // +0x08 (var_58)
        lEvent.mpSchemaBinData      = lpSchemaBinData;       // +0x0C (var_54)
        lEvent.miSchemaBinDataSize  = liSchemaBinDataSize;   // +0x10 (var_50)

        return mRequestQueue.AddEvent(&lEvent, 1);
    }

    // -------- RegisterVault @ X360 0x8229D6C8 (<2048>) --------
    // (recv, by-value 8-byte ResourceHandle, event id, vault type). Builds a
    // RegisterVaultRequest (event type id 0). The X360 null-guards the handle's memory
    // pointer (a NULL handle can not register a vault); the de-inlined StrStream assert
    // collapses to one CGS_ASSERT.
    // Param map re-attested vs the asm (both 0x8229D6C8 and the <512> 0x82256428): the
    // Xenon ABI passes the 8-byte handle in ONE 64-bit GPR (r5, `std r26`), so
    // r6 = liEventId (`stw r25 @+12`) and r7 = vault type (`stw r24 @+16`) are REAL
    // parameters -- the earlier "miEventId is a register artifact" note was wrong (the
    // GameData id-50/66 legs pass their event-slot index in r6 and the reply payload
    // AttribSysModule::RegisterVault posts is exactly that field; the slot lookup in
    // ProcessAttribSysRegisterVaultResponse @0x82666590 depends on it).
    template <s32 N>
    bool AttribSysRequestInterface<N>::RegisterVault(
            CgsModule::BaseEventReceiverQueue* lpUserReceiverQueue,
            CgsResource::ResourceHandle lVaultResourceHandle,
            s32 liEventId,
            EAttribSysVaultType leVaultType)
    {
        CGS_ASSERT(lVaultResourceHandle.mpResourceMemory != 0,
            "Trying to register a vault from a NULL resource handle");
        CGS_ASSERT(lpUserReceiverQueue != 0, "lpUserReceiverQueue");

        RegisterVaultRequest lEvent;
        lEvent.mpUserReceiverQueue  = lpUserReceiverQueue;   // +0x00 (var_60)
        lEvent.mVaultResourceHandle = lVaultResourceHandle;  // +0x04 (var_5C, 8 bytes)
        lEvent.miEventId            = liEventId;             // +0x0C (var_54)
        lEvent.meVaultType          = leVaultType;           // +0x10 (var_50)

        return mRequestQueue.AddEvent(&lEvent, 0);
    }

    // -------- UnregisterVault @ X360 0x826731E8 (<2048>) --------
    // (recv, by-value 8-byte ResourceHandle, event id). Builds an
    // UnregisterVaultRequest (event type id 2). r6 = liEventId is a real parameter
    // (same ABI note as RegisterVault); AttribSysModule::UnregisterVault echoes it as
    // the type-5 reply payload.
    template <s32 N>
    bool AttribSysRequestInterface<N>::UnregisterVault(
            CgsModule::BaseEventReceiverQueue* lpUserReceiverQueue,
            CgsResource::ResourceHandle lVaultResourceHandle,
            s32 liEventId)
    {
        CGS_ASSERT(lpUserReceiverQueue != 0, "lpUserReceiverQueue");

        UnregisterVaultRequest lEvent;
        lEvent.mpUserReceiverQueue  = lpUserReceiverQueue;   // +0x00 (var_40)
        lEvent.mVaultResourceHandle = lVaultResourceHandle;  // +0x04 (var_3C, 8 bytes)
        lEvent.miEventId            = liEventId;             // +0x0C

        return mRequestQueue.AddEvent(&lEvent, 2);
    }
}
}

#endif // GAMESHARED_GAMECLASSES_SYSTEM_ATTRIBSYS_CGSATTRIBSYSSHAREDIOIMPL_H
