#include "types.hpp"

#include <cstdio>

class ELFHASH
{
public:
    static s32 BuildHash(const char* lpcText, char lcTerminator);
};

class cKeyStringTable
{
public:
    void* BuildHashes();

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

void* cKeyStringTable::BuildHashes()
{
    void* lpResult = this;

    for (u32 luIndex = 0; luIndex < muCount; ++luIndex)
    {
        KeyEntry& lEntry = mpEntries[luIndex];
        if (lEntry.mpcName)
        {
            lEntry.miHash = ELFHASH::BuildHash(lEntry.mpcName, 0);
            lpResult = reinterpret_cast<void*>(static_cast<uintptr_t>(lEntry.miHash));
        }
        else
        {
            lpResult = reinterpret_cast<void*>(static_cast<uintptr_t>(std::printf("uh oh\n")));
        }
    }

    return lpResult;
}
