#ifndef CGS_CORE_X360_H
#define CGS_CORE_X360_H

// CgsCore's Xbox 360 platform layer: the memory copy/fill entry points every GameShared and
// GameSource TU calls as CgsCore::MemCpy / CgsCore::MemSet.
//
// Evidence:
// - DecFIGS declares them in the PS3 platform header CgsCorePS3.h, inside namespace CgsCore
//   (CgsCore.h:98): `void* MemCpy(void*, const void*, size_t)` at :174 and
//   `void* MemSet(void*, int, size_t)` at :183, and names them as the calls of 65 compile units
//   (e.g. Memory/CgsGatherStream.cpp, System/AttribSys/CgsAttribSysVaultAllocator.cpp,
//   Network/Packeting/CgsCompressionAndEncryptionUtils.cpp). Neither build holds a body of its own
//   (no CgsCore::MemCpy / MemSet symbol in the PS3 export): they are header-inline.
// - Every ARTIST site calls the XDK's XMemCpy / XMemSet directly (e.g. EventScoreUploadData::
//   Construct 0x82584AE0, ServerInterfaceCustomCommands::OverWriteServerFriendsRecord 0x825902B0),
//   so the Xenon bodies forward to them.
// - LegacySrc CgsCore.h:119 includes the platform header as "Core/PS3/CgsCorePS3.h"; ARTIST's own
//   X360 platform files follow <Dir>/X360/<Name>X360.h (Gui/CgsSaveLoadX360.h 0x820CE2F0,
//   System/GameTalk/X360/CgsGameTalkProtocolX360.h 0x820DF3A8). This file's name follows that
//   convention; no ARTIST string names it (it holds no assert).

#include <xtl.h>     // XMemCpy / XMemSet (XDK xbox.h)
#include <stddef.h>  // size_t

namespace CgsCore
{
    // Parameter names from the DecFIGS compile-unit dumps (e.g. _compile/CgsSceneManagerUnity.cpp,
    // _compile/BrnGameStateUnity.cpp).
    inline void* MemCpy(void* lpDestination, const void* lpSource, size_t liSize)
    {
        return XMemCpy(lpDestination, lpSource, liSize);
    }

    inline void* MemSet(void* lpDestination, int liValue, size_t liSize)
    {
        return XMemSet(lpDestination, liValue, liSize);
    }
}

#endif // CGS_CORE_X360_H
