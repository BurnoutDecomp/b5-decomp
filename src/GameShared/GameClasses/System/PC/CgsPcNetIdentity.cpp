// ============================================================================
// CgsPcNetIdentity.cpp -- the shared PC network identity (see CgsPcNetIdentity.h).
//
// [PC platform leaf] Host stand-in for the console's signed-in profile identity. Everything is
// computed once, on first use, from the environment; the values cannot change inside a process.
// ============================================================================

#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"
#include "GameShared/GameClasses/System/CgsHarnessSlot.h"

#include <cstdlib>
#include <cstring>

namespace
{
    // Persona capacity: 15 characters + NUL, the console gamertag buffer size.
    const u32 KU_NAME_CAPACITY = 16u;

    // High half of the stand-in XUID; the low half is FNV1a32 of the persona.
    const u64 KU64_XUID_BASE = 0x0009FFFF00000000ull;

    // FNV-1a 32-bit offset basis and prime (decimal, the published constants).
    const u32 KU_FNV1A32_OFFSET = 2166136261u;
    const u32 KU_FNV1A32_PRIME  = 16777619u;

    u32 Fnv1a32(const u8* lpData, u32 luLength)
    {
        u32 luHash = KU_FNV1A32_OFFSET;
        for (u32 lu = 0u; lu < luLength; ++lu)
        {
            luHash ^= lpData[lu];
            luHash *= KU_FNV1A32_PRIME;
        }
        return luHash;
    }

    // Parse BP_LAN_XUID: hex digits with an optional 0x/0X prefix, 1..16 digits, nothing else.
    bool ParseXuid(const char* lpcText, u64* lpXuidOut)
    {
        if (lpcText[0] == '0' && (lpcText[1] == 'x' || lpcText[1] == 'X'))
            lpcText += 2;
        u64 lu64Value = 0u;
        u32 luDigits  = 0u;
        for (; *lpcText != '\0'; ++lpcText)
        {
            const char lc = *lpcText;
            u32 luNibble;
            if (lc >= '0' && lc <= '9')      luNibble = static_cast<u32>(lc - '0');
            else if (lc >= 'a' && lc <= 'f') luNibble = static_cast<u32>(lc - 'a' + 10);
            else if (lc >= 'A' && lc <= 'F') luNibble = static_cast<u32>(lc - 'A' + 10);
            else                             return false;
            if (++luDigits > 16u)
                return false;
            lu64Value = (lu64Value << 4) | luNibble;
        }
        if (luDigits == 0u || lu64Value == 0u)
            return false;
        *lpXuidOut = lu64Value;
        return true;
    }

    struct Identity
    {
        char mac[KU_NAME_CAPACITY];
        u64  mu64Xuid;
        u32  muIdent;
        bool mbLan;
    };

    Identity Resolve()
    {
        Identity lIdentity;
        std::memset(&lIdentity, 0, sizeof(lIdentity));

        const char* lpcLan = std::getenv("BP_LAN");
        lIdentity.mbLan = (lpcLan != 0 && lpcLan[0] == '1' && lpcLan[1] == '\0');

        // Persona: BP_LAN_NAME, else "Slot<n>" for a harness slot, else "Player".
        const char* lpcName = std::getenv("BP_LAN_NAME");
        if (lpcName != 0 && lpcName[0] != '\0')
        {
            std::strncpy(lIdentity.mac, lpcName, KU_NAME_CAPACITY - 1u);
        }
        else
        {
            // Suffix() is "" for slot 0 / unset, "_<n>" otherwise.
            const char* lpcSuffix = CgsSystem::HarnessSlot::Suffix();
            if (lpcSuffix[0] == '_')
            {
                std::strcpy(lIdentity.mac, "Slot");
                std::strncat(lIdentity.mac, lpcSuffix + 1, KU_NAME_CAPACITY - 1u - 4u);
            }
            else
            {
                std::strcpy(lIdentity.mac, "Player");
            }
        }
        lIdentity.mac[KU_NAME_CAPACITY - 1u] = '\0';

        const char* lpcXuid = std::getenv("BP_LAN_XUID");
        if (lpcXuid == 0 || !ParseXuid(lpcXuid, &lIdentity.mu64Xuid))
        {
            lIdentity.mu64Xuid = KU64_XUID_BASE |
                Fnv1a32(reinterpret_cast<const u8*>(lIdentity.mac),
                        static_cast<u32>(std::strlen(lIdentity.mac)));
        }

        u8 laXuidBytes[8];
        for (u32 lu = 0u; lu < 8u; ++lu)
            laXuidBytes[lu] = static_cast<u8>(lIdentity.mu64Xuid >> (8u * lu));
        lIdentity.muIdent = (Fnv1a32(laXuidBytes, 8u) & 0x7FFFFFFFu) | 1u;
        return lIdentity;
    }

    const Identity& Get()
    {
        static const Identity slIdentity = Resolve();   // thread-safe one-time init
        return slIdentity;
    }
}

const char* CgsPcNetIdentityName()       { return Get().mac; }
u64         CgsPcNetIdentityXuid()       { return Get().mu64Xuid; }
u32         CgsPcNetIdentityLobbyIdent() { return Get().muIdent; }
bool        CgsPcNetLanEnabled()         { return Get().mbLan; }
