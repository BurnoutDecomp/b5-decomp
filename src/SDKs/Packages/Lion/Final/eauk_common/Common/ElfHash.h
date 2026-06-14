#pragma once

#include "types.hpp"

class ELFHASH
{
public:
    static s32 BuildHash(const char* lpcText, char lcTerminator);
};

class cLionTokenTable
{
public:
    void* BuildHashes();

private:
    struct TokenEntry
    {
        u32         muToken;
        u32         muUnknown4;
        u32         muUnknown8;
        const char* mpcName;
        s32         miHash;
    };

    TokenEntry* mpEntries;
};

inline void* cLionTokenTable::BuildHashes()
{
    void* lpResult = this;

    for (TokenEntry* lpEntry = mpEntries; lpEntry->muToken != 0; ++lpEntry)
    {
        if (lpEntry->mpcName)
        {
            lpEntry->miHash = ELFHASH::BuildHash(lpEntry->mpcName, 0);
            lpResult = reinterpret_cast<void*>(static_cast<uintptr_t>(lpEntry->miHash));
        }
        else
        {
            lpEntry->miHash = 0;
            lpResult = nullptr;
        }
    }

    return lpResult;
}
