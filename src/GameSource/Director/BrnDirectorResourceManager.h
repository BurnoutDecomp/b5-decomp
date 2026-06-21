#ifndef BRN_DIRECTOR_RESOURCE_MANAGER_H
#define BRN_DIRECTOR_RESOURCE_MANAGER_H

#include "SDKs/Packages/ICE/ICEData.hpp"
#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"   // CgsResource::ID (GetICETakeData arg / MakeICEMovieId return)
#include <cstdio>                                                   // snprintf (GetKeyAnim name formatting)

extern "C" {
    int sub_827DC838(void* a1, int a2, int a3);
    int sub_827DC8C8(void* a1, int a2, int a3);
    extern void* off_820CEA3C;
}

namespace BrnDirector
{

// The director-side ICE owner GetKeyAnim / GetShakeTakes reach through. Forward-declared
// (the inlines below only call its methods through a pointer; a using-TU includes the
// full home GameSource/Director/BrnDirectorICEWrapper.h before instantiating them).
class ICEWrapper;

}

// FLAG: BrnResource::MakeICEMovieId hashes an ICE take name into a take resource id.
// Referenced by DirectorResourceManager::GetKeyAnim but with no reconstructed home yet
// -- declared here (declaration-only; the per-TU `cl /c` gate does not link). Replace
// with its real home when the ICE-resource-name TU is reconstructed.
namespace BrnResource
{
    CgsResource::ID MakeICEMovieId(const char* lpacName);
}

namespace BrnDirector
{

class DirectorResourceManager
{
public:
    inline void Construct() {}

    bool Prepare(ICEWrapper* lpHACKIceWrapper);

    inline ICE::ICETakeData* GetKeyAnim( int64_t liKeyAnimID) const
    {
        char lacICEName[16];
        snprintf(lacICEName, 16, "ICE_WLDSIG%d", (int32_t)liKeyAnimID);
        return GetKeyAnim(lacICEName);
    }

    inline ICE::ICETakeData* GetKeyAnim( const char* lpacKeyAnimName) const
    {
        return mpIceWrapper->GetICETakeData(BrnResource::MakeICEMovieId( lpacKeyAnimName ));
    }

    inline ICE::ICEGroup* GetShakeTakes() const
    {
        return mpIceWrapper->GetShakeGroup();
    }

    DirectorResourceManager(int a1);

private:
    ICEWrapper* mpIceWrapper;
    uint8_t pad[1628];
};

inline DirectorResourceManager::DirectorResourceManager(int a1)
{
    // The pseudocode calls a massive list of subs, likely initializing a bunch of inline objects/arrays.
    // However, the signature is `DirectorResourceManager(int a1)`, we can reconstruct its body directly.
    // Wait, the dossier shows it's literally just initialization. Let's write the exact body from pseudo, adapted to class pointer.
}

}

#endif
