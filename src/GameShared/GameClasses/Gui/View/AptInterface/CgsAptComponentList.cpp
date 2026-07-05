#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptComponentList.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT, CgsDev::Assert::Begin/Fire/EndAssert

// CgsGui::AptComponentList accessors, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU (class:CgsGui::AptComponentList) bodies the parallel-array accessors the
// APT GUI interface uses to read/write per-component state:
//
//   GetKeyValue            @ 0x828469E8      SetKeyValue            @ 0x82846B60
//   GetAptValue            @ 0x82846A68      SetAptValue            @ 0x82846BE8
//   GetUsedData            @ 0x82846AE0      SetUsedData            @ 0x82846C68
//                                            SetHashedName          @ 0x82846CE8
//                                            SetHashedReferenceName @ 0x82846D68
//   MoveComponent          @ 0x8284E1F0
//
// X360 verification notes (calling convention taken from the asm prologue):
//   - `this` is the list base (r3); the component/key indices and values follow in
//     r4/r5/r6. Hex-Rays renders `this` as the leading `int a1` / `_DWORD *result`;
//     it is the AptComponentList base pointer in every case.
//   - Each accessor guards the component index with the inlined CgsAptCommunicator
//     bounds asserts: liComponent < 0 and liComponent >= 256 each fire a separate
//     "Invalid Component Index" assert (MoveComponent uses the "...in Copy From" /
//     "...in Copy To" variants). The baked CgsAptCommunicator.h file/line cites are
//     discarded per project convention; the assert strings are X360 rodata.
//   - maapComponentData / mapComponentReference hold POINTERS: the X360 reads them
//     with lwzx and writes them with stwx (32-bit on the PPC target). Hex-Rays
//     renders those word loads/stores as `int`, but the DecFIGS DWARF declares
//     KeyValue* / AptValue* element types (matching Get/SetKeyValue's KeyValue* and
//     Get/SetAptValue's AptValue*), so they are modelled and accessed as pointers.
//   - The setters' Hex-Rays `_DWORD*` return is a decompiler artifact: r3 still holds
//     `this` on the no-assert path, and the inlined EndAssert() leaks its result into
//     r3 on the assert path. The value is never consumed by any caller, so the
//     logical setters are `void`. Likewise MoveComponent's `result` is the mid-body
//     read of the source HashedName left in r3 by the folded code; the operation is a
//     logical void move, so it is reconstructed as `void`.
//   - All member access is by named parallel-array members; the X360 word/byte index
//     math reproduces exactly at each member's natural index (see the header layout).

namespace CgsGui
{
    // @ 0x82846A68 : result = *(4 * (liComponent + 0x2000) + this) (lwzx)
    //               == mapComponentReference[liComponent].
    AptValue* AptComponentList::GetAptValue(s32 liComponent) const
    {
        CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
        CGS_ASSERT(liComponent < KU_MAX_COMPONENTS, "Invalid Component Index");

        return mapComponentReference[liComponent];
    }

    // @ 0x828469E8 : result = *(4 * (32 * liComponent + liKey) + this) (lwzx)
    //               == maapComponentData[liComponent][liKey].
    KeyValue* AptComponentList::GetKeyValue(s32 liComponent, s32 liKey) const
    {
        CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
        CGS_ASSERT(liComponent < KU_MAX_COMPONENTS, "Invalid Component Index");

        return maapComponentData[liComponent][liKey];
    }

    // @ 0x82846AE0 : result = *(liComponent + this + 0x8C00) (lbzx)
    //               == maiNumUsedData[liComponent].
    u8 AptComponentList::GetUsedData(s32 liComponent) const
    {
        CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
        CGS_ASSERT(liComponent < KU_MAX_COMPONENTS, "Invalid Component Index");

        return maiNumUsedData[liComponent];
    }

    // @ 0x82846BE8 : this[liComponent + 0x2000] = lpValue (stwx)
    //               == mapComponentReference[liComponent] = lpValue.
    void AptComponentList::SetAptValue(s32 liComponent, AptValue* lpValue)
    {
        CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
        CGS_ASSERT(liComponent < KU_MAX_COMPONENTS, "Invalid Component Index");

        mapComponentReference[liComponent] = lpValue;
    }

    // @ 0x82846B60 : this[32 * liComponent + liKey] = lpKeyValue (stwx)
    //               == maapComponentData[liComponent][liKey] = lpKeyValue.
    void AptComponentList::SetKeyValue(s32 liComponent, s32 liKey, KeyValue* lpKeyValue)
    {
        CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
        CGS_ASSERT(liComponent < KU_MAX_COMPONENTS, "Invalid Component Index");

        maapComponentData[liComponent][liKey] = lpKeyValue;
    }

    // @ 0x82846C68 : *(this + liComponent + 0x8C00) = lu8Used (stbx)
    //               == maiNumUsedData[liComponent] = lu8Used.
    void AptComponentList::SetUsedData(s32 liComponent, u8 lu8Used)
    {
        CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
        CGS_ASSERT(liComponent < KU_MAX_COMPONENTS, "Invalid Component Index");

        maiNumUsedData[liComponent] = lu8Used;
    }

    // @ 0x82846CE8 : this[liComponent + 0x2100] = luHash (stwx)
    //               == mauHashedName[liComponent] = luHash.
    void AptComponentList::SetHashedName(s32 liComponent, u32 luHash)
    {
        CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
        CGS_ASSERT(liComponent < KU_MAX_COMPONENTS, "Invalid Component Index");

        mauHashedName[liComponent] = luHash;
    }

    // @ 0x82846D68 : this[liComponent + 0x2200] = luHash (stwx)
    //               == mauHashedReferenceName[liComponent] = luHash.
    void AptComponentList::SetHashedReferenceName(s32 liComponent, u32 luHash)
    {
        CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
        CGS_ASSERT(liComponent < KU_MAX_COMPONENTS, "Invalid Component Index");

        mauHashedReferenceName[liComponent] = luHash;
    }

    // Store the component's name text into its maacName row (AddNewAptComponent
    // @0x82849B88 calls it after the duplicate check). Bounds-asserted like the
    // sibling setters; NUL-guaranteed at the buffer edge.
    void AptComponentList::SetName(s32 liComponent, const char* lpacName)
    {
        CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
        CGS_ASSERT(liComponent < KU_MAX_COMPONENTS, "Invalid Component Index");

        char* lpacDest = maacName[liComponent];
        s32 li = 0;
        if (lpacName != 0)
        {
            for (; lpacName[li] != 0 && li < KI_COMPONENT_NAME_LENGTH - 1; ++li)
                lpacDest[li] = lpacName[li];
        }
        lpacDest[li] = 0;
    }

    // @ 0x8284E1F0 : copy the full per-component record from slot liFrom into slot
    // liTo, then zero slot liFrom. The X360 copies the 32-word key-pointer block,
    // then the component reference, hashed name, hashed reference name and used byte
    // one at a time (reading each source field before writing the destination), and
    // finally clears the source's key block and four scalar fields. Reconstructed as
    // re-rolled loops + named member copies. The per-component name table (maacName)
    // is deliberately left untouched, matching the X360 body.
    void AptComponentList::MoveComponent(s32 liFrom, s32 liTo)
    {
        CGS_ASSERT(liFrom >= 0, "Invalid Component Index in Copy From");
        CGS_ASSERT(liFrom < KU_MAX_COMPONENTS, "Invalid Component Index in Copy From");
        CGS_ASSERT(liTo >= 0, "Invalid Component Index in Copy To");
        CGS_ASSERT(liTo < KU_MAX_COMPONENTS, "Invalid Component Index in Copy To");

        // Copy the 32-pointer key block (X360 do-while over r11=32).
        for (s32 liKey = 0; liKey < KI_MAX_DATA_PER_COMPONENT; ++liKey)
        {
            maapComponentData[liTo][liKey] = maapComponentData[liFrom][liKey];
        }

        // Copy the four scalar fields source -> destination.
        mapComponentReference[liTo]  = mapComponentReference[liFrom];
        mauHashedName[liTo]          = mauHashedName[liFrom];
        mauHashedReferenceName[liTo] = mauHashedReferenceName[liFrom];
        maiNumUsedData[liTo]         = maiNumUsedData[liFrom];

        // Clear the source slot: its key block (X360 bdnz over ctr=32) and scalars.
        for (s32 liKey = 0; liKey < KI_MAX_DATA_PER_COMPONENT; ++liKey)
        {
            maapComponentData[liFrom][liKey] = 0;
        }
        mapComponentReference[liFrom]  = 0;
        mauHashedName[liFrom]          = 0;
        mauHashedReferenceName[liFrom] = 0;
        maiNumUsedData[liFrom]         = 0;
    }
}
