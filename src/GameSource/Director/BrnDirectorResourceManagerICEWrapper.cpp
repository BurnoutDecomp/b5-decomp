// ============================================================================
// GameSource/Director/BrnDirectorResourceManagerICEWrapper.cpp
//
// [marked deviation -- FILE SPLIT, not a code change] The three DirectorResourceManager
// bodies that reach an ICEWrapper method with no mounted home. Moved VERBATIM out of
// BrnDirectorResourceManagerICE.cpp on 2026-08-01 (which was itself split out of
// BrnDirectorResourceManager.cpp -- see that file's banner) so that the two bodies the
// ICE camera family actually needs, GetICEAuthor and GetKeyAnimFromGuid, could be
// mounted without them.
//
// ⭐ WHY THIS BOUNDARY -- it is the BLOCKER, not the topic. All five bodies are ICE-cone
// accessors, but they do not cost the same:
//     GetICEAuthor        -> ICEWrapper::GetAuthor          HEADER INLINE, free
//     GetKeyAnimFromGuid  -> ICEAuthor::FindEditedTakeFromGuid   (ICEAuthorTakeOps.cpp)
//                            ICEList::GetICETakeDataFromGuid     (ICEList.cpp, mounted)
//   ---- vs the three below ----
//     GetKeyAnim x2       -> BrnResource::MakeICEMovieId    NO reconstructed home
//                            ICEWrapper::GetICETakeData     )  real bodies in
//     GetShakeTakes       -> ICEWrapper::GetShakeGroup      )  BrnDirectorICEWrapper.cpp,
//                                                              MEASURED at +6 unresolved
//                                                              to mount (ICEController::
//                                                              {EditorOn,SetState},
//                                                              ICEManager::GetCameraTake,
//                                                              DebugInterface::{En,Dis}
//                                                              ableConsole, ...)
// Splitting on that line takes the mountable half to ZERO opened externals.
//
// ⭐ WHY A SPLIT AND NOT THREE LINK STUBS (unchanged from the parent split's banner):
// every one of these returns a POINTER a caller dereferences or a REFERENCE that cannot
// be null. A `return 0` stub for GetICETakeData/GetShakeGroup would be a silent-drop gate
// of exactly the shape that cost this project the RaceCarState::operator= incident.
// Splitting the file adds NOTHING to the link and invents NOTHING.
//
// ⚠️ NOT MOUNTED. Mount this WITH BrnDirectorICEWrapper.cpp + a BrnResource::MakeICEMovieId
// home, and merge it -- and BrnDirectorResourceManagerICE.cpp -- straight back into
// BrnDirectorResourceManager.cpp at the same time. Its callers (BrnMomentPlayerStunt,
// ICEWrapper, BrnDirectorDevTools) are all unmounted too.
//
// ⚠️ SIGNATURE NOTE, carried across unchanged and NOT resolved here: the DecFIGS DWARF
// (GameSource/Director/BrnDirectorResourceManager.h:261) declares ONE key-anim getter,
// `const ICETakeData* GetKeyAnim(ID) const` -- taking a CgsResource::ID and returning a
// CONST pointer -- where the tree carries the (int64_t) / (const char*) pair returning a
// non-const pointer. The X360 body @0x821F6948 formats a name and hashes it, which is
// what the (const char*) overload models, so the pair is not wrong so much as
// unreconciled with the PS3 header. Settle it when MakeICEMovieId gets a home; do not
// "fix" it blind (the DWARF is the PS3 build and version drift is a known trap here).
// DELETE-WHEN: BrnDirectorICEWrapper.cpp is mountable and MakeICEMovieId has a home.
// ============================================================================

#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/BrnDirectorICEWrapper.h"   // ICEWrapper::GetICETakeData / GetShakeGroup

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

}
