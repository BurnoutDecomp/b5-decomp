#pragma once

#include "types.hpp"

class ELFHASH
{
public:
    static s32 BuildHash(const char* lpcText, char lcTerminator);
};
