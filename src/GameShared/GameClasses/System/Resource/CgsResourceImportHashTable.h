#pragma once

#include "GameShared/GameClasses/Containers/CgsLinearSOAHashTable.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"

namespace CgsResource
{
    // DWARF CgsResourceImportHashTable.h:49; ARTIST 828EDF28..828EDF50
    // supplies exactly these two pointers as the value of the 64-bit ID key.
    struct ImportHashTableValue
    {
        void* mpTempAddress;
        void* mpDestAddress;
    };

    struct ImportHashTable
    {
        typedef CgsContainers::LinearSOAHashTable<ImportHashTableValue, u64> InternalImportHashTable;

        void Initialize(u64* lpKeys, ImportHashTableValue* lpValues, s32 liLength)
        {
            mHashTable.Initialize(lpKeys, lpValues, static_cast<u64>(liLength));
        }

        // Inlined in ScratchPool::Clear, 828EDE40..828EDE70: full 64-bit stores.
        void Clear()
        {
            for (u64 lu = 0; lu < mHashTable.miLength; ++lu)
                mHashTable.mpKeys[lu] = mHashTable.miInvalidKey;
        }

        void AddEntry(ID lId, void* lpTempAddress, void* lpDestAddress)
        {
            const ImportHashTableValue lValue = { lpTempAddress, lpDestAddress };
            mHashTable.AddEntry(lId.GetHash(), &lValue);
        }

        void* GetTempAddress(ID lId)
        {
            const ImportHashTableValue* lpValue = mHashTable.FindEntry(lId.GetHash());
            return lpValue ? lpValue->mpTempAddress : nullptr;
        }

        void* GetDestAddress(ID lId)
        {
            const ImportHashTableValue* lpValue = mHashTable.FindEntry(lId.GetHash());
            return lpValue ? lpValue->mpDestAddress : nullptr;
        }

    private:
        InternalImportHashTable mHashTable;
    };
}
