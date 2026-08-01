// ============================================================================
// GameSource/Director/BrnDirectorResourceManagerICE.cpp
//
// [marked deviation -- FILE SPLIT, not a code change] The five DirectorResourceManager
// bodies that reach the ICE take-runtime cone. On the console these live in
// GameSource/Director/BrnDirectorResourceManager.cpp with the rest of the class; they were
// moved to this sibling on 2026-08-01, VERBATIM, so that the parent TU could be mounted for
// DirectorResourceManager::Prepare @0x8225CA08 (the 65 shot-group slot builds -- the whole
// point of the class) without dragging six unresolved externals into the link:
//
//     BrnResource::MakeICEMovieId                     (no reconstructed home yet)
//     BrnDirector::ICEWrapper::GetICETakeData         )
//     BrnDirector::ICEWrapper::GetShakeGroup          ) real bodies in
//     BrnDirector::ICEWrapper::GetAuthor              ) BrnDirectorICEWrapper.cpp
//     ICE::ICEAuthor::FindEditedTakeFromGuid          (SDKs/Packages/ICE/ICEAuthorTakeOps.cpp)
//     BrnResource::ICEList::GetICETakeDataFromGuid    (SharedClasses/DataLists/ICEList.cpp)
//
// ⭐ WHY A SPLIT AND NOT SIX LINK STUBS. Every one of these six returns a POINTER that a
// caller then dereferences or a REFERENCE that cannot be null. A `return 0` stub for
// GetICETakeData/FindEditedTakeFromGuid would be a silent-drop gate of exactly the shape
// that cost this project the RaceCarState::operator= incident (an empty body that discarded
// every copy in the tree and was justified by "off the boot path" until the path lit up).
// Splitting the file adds NOTHING to the link and invents NOTHING; the parent TU keeps every
// body the linked set actually reaches, and these five keep their real, faithful
// implementations for the day the ICE group goes in.
//
// ⚠️ NOT MOUNTED. Mount this WITH the ICE take-runtime group (BrnDirectorICEWrapper.cpp +
// ICEAuthorTakeOps.cpp + ICEList.cpp + the MakeICEMovieId home) and merge it back into
// BrnDirectorResourceManager.cpp at the same time -- the split has no reason to outlive the
// blocker. Its callers (BrnMomentPlayerStunt, ICEWrapper, BrnDirectorDevTools,
// BrnKeyAnimController, BrnBehaviourIceAnim) are all unmounted too.
// DELETE-WHEN: the ICE take-runtime group lands.
// ============================================================================

#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/BrnDirectorICEWrapper.h"   // ICEWrapper::GetICETakeData / GetShakeGroup / GetAuthor
#include "SDKs/Packages/ICE/ICEAuthor.hpp"               // ICE::ICEAuthor::FindEditedTakeFromGuid
#include "SharedClasses/DataLists/ICEList.h"             // BrnResource::ICEList::GetICETakeDataFromGuid

namespace BrnDirector
{

// @0x821F6948 -- resolve a world-signature key-anim by its numeric id. The console
// formats the take name and hashes it; the name form below is the one the X360 builds.
ICE::ICETakeData* DirectorResourceManager::GetKeyAnim(int64_t liKeyAnimID) const
{
    char lacICEName[16];
    snprintf(lacICEName, 16, "ICE_WLDSIG%d", (int32_t)liKeyAnimID);
    return GetKeyAnim(lacICEName);
}

ICE::ICETakeData* DirectorResourceManager::GetKeyAnim(const char* lpacKeyAnimName) const
{
    return mpICEWrapper->GetICETakeData(BrnResource::MakeICEMovieId(lpacKeyAnimName));
}

ICE::ICEGroup* DirectorResourceManager::GetShakeTakes() const
{
    return mpICEWrapper->GetShakeGroup();
}

// The in-game ICE editor's author/edit store. The console reaches it at manager +560 --
// i.e. through mpICEWrapper -- which is why this is not a plain member read.
ICE::ICEAuthor& DirectorResourceManager::GetICEAuthor() const
{
    return mpICEWrapper->GetAuthor();
}

// @0x821F69A8 -- resolve an ICE take GUID to its take data. The editor's edited-take list
// wins over the on-disk list, so an in-editor edit is what plays back.
ICE::ICETakeData* DirectorResourceManager::GetKeyAnimFromGuid(s32 liGuid) const
{
    ICE::ICETakeData* lpTakeData = mpICEWrapper->GetAuthor().FindEditedTakeFromGuid(liGuid);
    if (lpTakeData == 0)
    {
        lpTakeData = const_cast<ICE::ICETakeData*>(
            mpICEDictionaryList->GetICETakeDataFromGuid(liGuid));
    }
    return lpTakeData;
}

}
