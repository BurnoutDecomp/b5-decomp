#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsLinearHashTable.h"   // LinearHashTable / LinearHashEntry
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"   // ID (key = id.GetHash())

// CgsResource::HashTable - the Pool's resource-index map. A thin wrapper over
// LinearHashTable<u64,s32> that keys on a resource ID's 64-bit hash and stores the
// resource's entry index (s32). FindEntry returns the index or -1 when absent.
//
// Reconstructed from the DecFIGS DWARF (CgsResourceHashTable.h). The load-path forwards
// (Initialize/AddEntry/FindEntry) inline to the decompiled container methods; the
// remove/size/verify forwards are reconstructed with the unload path.

namespace CgsResource
{
    struct HashTable
    {
        typedef CgsContainers::LinearHashTable<u64, s32> InternalHashTable;   // :37
        typedef CgsContainers::LinearHashEntry<u64, s32> HashEntry;           // :36

        s32  CalculateRequiredSize(s32 liLength);                              // :52

        void Initialize(HashEntry* lpEntries, s32 liLength)                    // :55
        {
            mHashTable.Initialize(lpEntries, static_cast<u64>(liLength));
        }

        void AddEntry(ID lID, s32 liIndex)                                     // :58
        {
            mHashTable.AddEntry(lID.GetHash(), &liIndex);
        }

        void RemoveEntry(ID lID);                                             // :61

        s32 FindEntry(ID lID)                                                  // :64
        {
            s32* lpValue = mHashTable.FindEntry(lID.GetHash());
            return lpValue != 0 ? *lpValue : -1;
        }

        bool VerifyHashTable();                                                // :67

    private:
        InternalHashTable mHashTable;   // :71
    };
}
