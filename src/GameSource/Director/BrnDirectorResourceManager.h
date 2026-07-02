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

namespace Attrib { namespace Gen { class shotgroup; } }   // GameSource/AttribSys/Generated/classes/shotgroup.h

namespace BrnDirector
{

// The director-side ICE owner GetKeyAnim / GetShakeTakes reach through. Forward-declared
// (the inlines below only call its methods through a pointer; a using-TU includes the
// full home GameSource/Director/BrnDirectorICEWrapper.h before instantiating them).
class ICEWrapper;
class DirectorResourceManager;

class ICEResourceMgr : public ICE::IResourceManager
{
public:
    void Construct(DirectorResourceManager* lpResourceManager);

    const ICE::ICETakeData* GetTakeData(CgsResource::ID lId) const override;
    const ICE::ICETakeData* GetTakeData(s32 liIndex) const override;

private:
    DirectorResourceManager* mpResourceManager;
};

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
        return mpICEWrapper->GetICETakeData(BrnResource::MakeICEMovieId( lpacKeyAnimName ));
    }

    inline ICE::ICEGroup* GetShakeTakes() const
    {
        return mpICEWrapper->GetShakeGroup();
    }

    const ICE::IResourceManager* GetIceResourceManager() const
    {
        return &mICEResourceMgr;
    }

    // The three crash-energy shotgroup banks (DWARF h:351/h:357/h:354; members
    // mFastCrashShotGroup/mNormalCrashShotGroup/mSlowCrashShotGroup at X360
    // manager+1400/+1416/+1432 -- ShotSelector::GetCrashShot @0x82239708.. inlines
    // these accessors to those direct field addresses). ADDITIVE GROW:
    // declaration-only (the member bank + bodies land with this manager's own TU).
    const Attrib::Gen::shotgroup& GetFastCrashShots() const;
    const Attrib::Gen::shotgroup& GetNormalCrashShots() const;
    const Attrib::Gen::shotgroup& GetSlowCrashShots() const;

    DirectorResourceManager(int a1);

private:
    // X360 members preceding mICEResourceMgr occupy 552 bytes. Their concrete
    // resource queue/handle types are owned by their respective TUs.
    u8 maPaddingBeforeICEResourceMgr[552];
    ICEResourceMgr mICEResourceMgr;
    ICEWrapper* mpICEWrapper;
    u8 maPaddingAfterICEWrapper[1064];
};

inline DirectorResourceManager::DirectorResourceManager(int a1)
{
    // The pseudocode calls a massive list of subs, likely initializing a bunch of inline objects/arrays.
    // However, the signature is `DirectorResourceManager(int a1)`, we can reconstruct its body directly.
    // Wait, the dossier shows it's literally just initialization. Let's write the exact body from pseudo, adapted to class pointer.
}

}

#endif
