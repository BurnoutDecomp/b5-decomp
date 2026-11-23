#include "SDKs/Packages/Lion/Final/eauk_common/Common/ElfHash.h"

#include <cstdio>

class cKeyStringTable
{
public:
    void BuildHashes();

private:
    struct KeyEntry
    {
        u32         muKey;
        const char* mpcName;
        s32         miHash;
    };

    u32       muCount;
    KeyEntry* mpEntries;
};

void cKeyStringTable::BuildHashes()
{
    for (u32 luIndex = 0; luIndex < muCount; ++luIndex)
    {
        KeyEntry& lEntry = mpEntries[luIndex];
        if (lEntry.mpcName)
        {
            lEntry.miHash = ELFHASH::BuildHash(lEntry.mpcName, 0);
        }
        else
        {
            std::printf("uh oh\n");
        }
    }
}
