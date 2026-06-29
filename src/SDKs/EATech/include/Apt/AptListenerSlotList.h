#pragma once

#include "types.hpp"

// IAptListener: minimal abstract interface for objects held by AptListenerSlotList.
// Two virtual methods matching the vtable slots accessed in the add/remove bodies
// (slot 0 called by add, slot 1 called by remove):
//   vtable[0] = addRef()  : called when the listener is added to the slot list
//   vtable[1] = release() : called when the listener is removed from the slot list
// Both return int (the updated reference count on X360 COM-style refcounting).
struct IAptListener
{
    virtual int addRef()  = 0;
    virtual int release() = 0;
};

// AptListenerSlotList<T>: fixed-capacity slot array for APT event listeners.
//
// Layout from X360 ARTIST pseudocode (int* a1):
//   +0x00  muHead      *a1         scan-start / approx. occupied-slot count
//   +0x04  muPad1      *(a1+1)    (not accessed)
//   +0x08  muCapacity  *(a1+2)    total slot count
//   +0x0C  muPad2      *(a1+3)    (not accessed)
//   +0x10  mppSlots    *(a1+4)    pointer to the external T* slot array
//
// add()    @ X360: *>::add    @ 0x82ADBCE0
// remove() @ X360: *>::remove @ 0x82ADBC28
//
// Callers of add/remove: AptKey::sMethod_addListener/removeListener,
//   AptDisplayList::_addToSetCaches, AptCIH::AssociateInstToClass,
//   AptCIH::objectMemberSet, AptCIH::ClearCIH.
template<typename T>
struct AptListenerSlotList
{
    u32  muHead;        // *a1:    scan-start / approximate count of occupied slots
    u32  muPad1;        // *(a1+1): not accessed
    u32  muCapacity;    // *(a1+2): total slots
    u32  muPad2;        // *(a1+3): not accessed
    T**  mppSlots;      // *(a1+4): pointer to external T* array (capacity elements)

    // Add lpListener to the first empty slot found by scanning forward from muHead+1.
    // Calls lpListener->addRef() (vtable[0]) after storing it. Returns its result.
    // X360: *>::add @ 0x82ADBCE0
    int add(T* lpListener);

    // Remove lpListener from the slot list (linear scan).
    // Calls lpListener->release() (vtable[1]) before clearing the slot.
    // Returns 1 if found and removed, 0 if not found or list is empty.
    // X360: *>::remove @ 0x82ADBC28
    int remove(T* lpListener);
};

// ---------------------------------------------------------------------------
// AptListenerSlotList<T>::add
// ---------------------------------------------------------------------------
template<typename T>
inline int AptListenerSlotList<T>::add(T* lpListener)
{
    // v2 = *(a1 + 4) — data buffer base pointer
    // v3 = (*a1 + 1) — start search one past current head
    // Loop: advance *a1 to v3, scan forward until an empty slot (nullptr) is found;
    //       if v3 reaches capacity, wrap to (u32)(-1) then the prefix ++ brings it to 0.
    u32 luNext = muHead + 1u;
    for (muHead = luNext; mppSlots[luNext] != nullptr; ++luNext)
    {
        if (luNext >= muCapacity)
            luNext = static_cast<u32>(-1);   // wrap; ++ in for-increment → 0
    }
    mppSlots[luNext] = lpListener;
    return lpListener->addRef();             // vtable[0](self): (**a2)(a2)
}

// ---------------------------------------------------------------------------
// AptListenerSlotList<T>::remove
// ---------------------------------------------------------------------------
template<typename T>
inline int AptListenerSlotList<T>::remove(T* lpListener)
{
    // Guard: if muHead == 0, list is considered empty.
    if (!muHead)
        return 0;

    // Linear scan for lpListener in the slot array.
    u32 luIndex = 0u;
    if (muCapacity)
    {
        T** lpSlot = mppSlots;
        do
        {
            if (*lpSlot == lpListener)
                break;
            ++luIndex;
            ++lpSlot;
        }
        while (luIndex < muCapacity);
    }

    if (luIndex >= muCapacity)
        return 0;   // not found

    // Found at luIndex: call release() (vtable[1]) then clear the slot.
    // X360: (*(**(v6 + v5) + 4))(*(v6 + v5))  =  mppSlots[luIndex]->release()
    --muHead;
    mppSlots[luIndex]->release();            // vtable[1](self)
    mppSlots[luIndex] = nullptr;
    return 1;
}
