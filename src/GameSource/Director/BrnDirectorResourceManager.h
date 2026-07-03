#ifndef BRN_DIRECTOR_RESOURCE_MANAGER_H
#define BRN_DIRECTOR_RESOURCE_MANAGER_H

#include "SDKs/Packages/ICE/ICEData.hpp"
#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"   // CgsResource::ID (GetICETakeData arg / MakeICEMovieId return)
#include <cstdio>                                                   // snprintf (GetKeyAnim name formatting)

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

    // @0x821F69A8 (own ledger fn -- declared for the dev-tools GameTalk handler
    // @0x822095A0). Resolve an ICE take GUID to its take data: the wrapper editor's
    // edited-take list first (ICE::ICEAuthor::FindEditedTakeFromGuid on the X360
    // rm+560 wrapper's editor), else the resource take list
    // (BrnResource::ICEList::GetICETakeDataFromGuid on the rm+544 list pointer --
    // both interiors live in maPaddingBeforeICEResourceMgr here). DECLARATION-ONLY;
    // the body lands with this manager's own TU.
    ICE::ICETakeData* GetKeyAnimFromGuid(s32 liGuid) const;

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

    // The two crash-moment shotgroup banks right after the trio above (X360
    // manager+1448/+1464, the same 16-byte stride -- MomentStationaryCrash::Update
    // @0x82272EA8 picks between them by its tumbling latch). ADDITIVE GROW:
    // declaration-only (the member bank + bodies land with this manager's own TU).
    const Attrib::Gen::shotgroup& GetTumblingCrashShots() const;     // +1448
    const Attrib::Gen::shotgroup& GetStationaryCrashShots() const;   // +1464

    // The player-jumping ICE shot-group instance (X360 manager+1320 / +0x528 --
    // MomentPlayerJumping::Prepare @0x82251048 reads *(behaviour-manager
    // mpDirectorResourceManager)+0x528 and resolves its ShotList attribute, key
    // 0x7533C0E2_15246B49, for up to five ice shots). ADDITIVE GROW:
    // declaration-only (the member + body land with this manager's own TU).
    // FLAG: accessor name inferred from the consumer's role.
    const Attrib::Gen::shotgroup& GetPlayerJumpingShots() const;      // +1320

private:
    // X360 members preceding mICEResourceMgr occupy 552 bytes. Their concrete
    // resource queue/handle types are owned by their respective TUs.
    u8 maPaddingBeforeICEResourceMgr[552];
    ICEResourceMgr mICEResourceMgr;
    ICEWrapper* mpICEWrapper;
    u8 maPaddingAfterICEWrapper[1064];
};

}

#endif
