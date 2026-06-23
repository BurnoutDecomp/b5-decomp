#include "GameShared/GameClasses/Gui/CgsGuiEventLocalisedTextPointerRemoved.h"
#include "GameShared/GameClasses/Containers/CgsHash.h"   // CgsContainers::CgsHash::CalculateHash

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGui::GuiEventLocalisedTextPointerRemoved::IsThisTheRemovedString @ 0x82541B38
//   CgsGui::GuiEventLocalisedTextPointerRemoved::SetStringRemoved       @ 0x828467E0
//
// Both walk lpacString to its NUL to measure its length (the asm's
// `while (*p++);` then `len = p - str - 1`), hash the bytes [str, str+len) with the
// container CRC-32 (CgsContainers::CgsHash::CalculateHash), and either store the
// hash (SetStringRemoved) or compare it to the stored id (IsThisTheRemovedString).
// CalculateHash takes a mutable char*; the strings here are read-only ids so the
// cast is harmless (the hash never writes through the pointer).

namespace CgsGui
{

u32 GuiEventLocalisedTextPointerRemoved::SetStringRemoved(const char* lpacString)
{
    const char* lpcEnd = lpacString;
    while (*lpcEnd)
        ++lpcEnd;
    const s32 liLength = static_cast<s32>(lpcEnd - lpacString);

    const u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpacString), liLength);
    muStringHash = luHash;
    return luHash;
}

bool GuiEventLocalisedTextPointerRemoved::IsThisTheRemovedString(const char* lpacString) const
{
    const char* lpcEnd = lpacString;
    while (*lpcEnd)
        ++lpcEnd;
    const s32 liLength = static_cast<s32>(lpcEnd - lpacString);

    const u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpacString), liLength);
    return luHash == muStringHash;
}

}
