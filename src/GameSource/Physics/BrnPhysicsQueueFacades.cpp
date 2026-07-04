#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // BeginAssert / FireAssert / EndAssert

// ============================================================================
// BrnPhysics -- out-of-line template-instance / de-inlined-accessor FACADES
// (X360 ARTIST build). IDA demangles these symbols under "BrnPhysics" with
// TRUNCATED names (Def / Ph / Physics / PhysicsMo / PhysicsModuleIO / Pr). The
// asm-authoritative bodies show they are NOT methods of a `BrnPhysics` class:
// they are the per-instantiation out-of-line emissions of generic
// container/smart-pointer accessors and de-inlined CgsModule::IOBuffer deep-member
// getters, whose GENERIC homes are already committed:
//   - CgsModule::BaseEventQueue<T>  (GameShared/.../Module/CgsBaseEventQueue.h)
//   - CgsResource::ResourcePtr<T>   (GameShared/.../System/Resource/CgsResourcePtr.h)
//   - CgsModule::IOBuffer           (lock-bit-guarded &member returns)
//
// They are bodied here as standalone facades that reproduce the X360 store order /
// strides / bit tests / member byte-offsets EXACTLY, operating on the raw object
// layout. This directly mirrors the committed sibling
// GameSource/Physics/PropManager/SharedIO/BrnPropQueueFacades.cpp.
//
// FLAG (un-homed element/resource types): the queue element stride (8 bytes) and
// the ResourcePtr pointee type are NOT pinned to a committed payload struct in this
// batch, so those facades take/return raw byte pointers rather than typed T&. The
// IOBuffer accessors return raw u8* to `this + <attested offset>` guarded by the
// lock bit the asm names (bit 4 == read, bit 3 == write). They are bit-identical to
// the X360 ABI; swap in the typed generic instantiation once the exact
// element/resource/buffer types for these call sites are homed.
// ============================================================================

namespace BrnPhysics
{
    // ---- minimal raw view of CgsModule::BaseEventQueue<T> (layout-faithful) -------
    // Matches CgsBaseEventQueue.h member order: T* mpEvents; s32 miMaxLength; s32 miLength.
    struct RawEventQueue
    {
        u8* mpEvents;     // +0x00
        s32 miMaxLength;  // +0x04
        s32 miLength;     // +0x08
    };

    // ---- minimal raw view of CgsResource::BaseResourcePtr (first word only) -------
    struct RawResourcePtr
    {
        void* mpResourceMemory; // +0x00
    };

    // 0x822C7708 -- CgsResource::ResourcePtr<T>::operator->()  (baked CgsResourcePtr.h:544)
    // asm: assert mpResourceMemory != NULL ("Can not instance resource pointer - it has
    // no main memory resource"); return mpResourceMemory. The vehicle-graphics resource
    // deref path (only the baked assert FILE string differs from the prop siblings).
    void* ResourcePtrDeref_VehicleGraphics(RawResourcePtr* lpPtr)
    {
        CGS_ASSERT(lpPtr->mpResourceMemory != nullptr,
                   "Can not instance resource pointer - it has no main memory resource\n");
        return lpPtr->mpResourceMemory;
    }

    // 0x8279F080 -- de-inlined CgsModule::IOBuffer write-locked deep-member accessor
    // (BrnPhysicsModuleIO.h:293). asm: lbz r11,0(this); extrwi r11,r11,1,28 == (*p>>3)&1
    // (write-lock bit 3); on failure streams "Not locked for writing\n"; return this+324064.
    u8* IOBufferGetMember_Write_0x4F1E0(u8* lpBuffer)
    {
        CGS_ASSERT(((lpBuffer[0] >> 3) & 1) != 0, "Not locked for writing\n");
        return lpBuffer + 324064;
    }

    // 0x8259FA98 -- de-inlined CgsModule::IOBuffer read-locked deep-member accessor
    // (BrnPhysicsModuleIO.h:284). asm: lbz r11,0(this); extrwi r11,r11,1,27 == (*p>>4)&1
    // (read-lock bit 4); on failure streams "Not locked for reading\n"; return this+149632.
    u8* IOBufferGetMember_Read_0x24880(u8* lpBuffer)
    {
        CGS_ASSERT(((lpBuffer[0] >> 4) & 1) != 0, "Not locked for reading\n");
        return lpBuffer + 149632;
    }

    // 0x8279F790 -- de-inlined CgsModule::IOBuffer read-locked deep-member accessor
    // (BrnPhysicsModuleIO.h:363). read-lock bit 4; return this+159648.
    u8* IOBufferGetMember_Read_0x26FA0(u8* lpBuffer)
    {
        CGS_ASSERT(((lpBuffer[0] >> 4) & 1) != 0, "Not locked for reading\n");
        return lpBuffer + 159648;
    }

    // 0x825A01D0 -- de-inlined CgsModule::IOBuffer write-locked deep-member accessor
    // (BrnPhysicsModuleIO.h:364); same member as the read overload 0x8279F790.
    // write-lock bit 3; return this+159648.
    u8* IOBufferGetMember_Write_0x26FA0(u8* lpBuffer)
    {
        CGS_ASSERT(((lpBuffer[0] >> 3) & 1) != 0, "Not locked for writing\n");
        return lpBuffer + 159648;
    }

    // 0x825BC680 -- CgsModule::BaseEventQueue<T (stride 8)>::GetEvent(int) const
    // (CgsBaseEventQueue.h:272/274/275). asm: assert mpEvents!=NULL; assert liIndex<miLength;
    // assert liIndex>=0; result = 8*liIndex + mpEvents. The *8 stride proves sizeof(T)==8.
    const void* GetEvent_Stride8_Const(const RawEventQueue* lpQueue, s32 liIndex)
    {
        CGS_ASSERT(lpQueue->mpEvents != nullptr, "mpEvents != NULL");
        CGS_ASSERT(liIndex < lpQueue->miLength, "liIndex < GetLength()");
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        return lpQueue->mpEvents + 8 * static_cast<u32>(liIndex);
    }
}
