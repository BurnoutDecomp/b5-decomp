// DirtySDK dirtysock -- string hash (dirtynet).
//
// Only NetHash is reconstructed; the game manager uses it for ConnApi client ids.

// XDK size optimization matches ARTIST at 0x82B2FF70.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "dirtynet.h"

extern "C" s32 NetHash(const char* pString)
{
    s32 iHash;
    s32 iChar;

    // the shift is arithmetic: the hash is a signed word
    for (iHash = 0; (iChar = (s32)(signed char)*pString) != 0; ++pString)
    {
        iHash = (iHash >> 27) ^ (s32)((u32)iHash << 5) ^ iChar;
    }
    return iHash;
}
