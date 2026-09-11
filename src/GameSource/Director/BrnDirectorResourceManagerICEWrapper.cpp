// ============================================================================
// GameSource/Director/BrnDirectorResourceManagerICEWrapper.cpp
//
// [marked deviation -- FILE SPLIT, not a code change] DirectorResourceManager::
// GetShakeTakes, the one body of this class that reaches an ICEWrapper method with no
// mounted home. Moved VERBATIM out of BrnDirectorResourceManagerICE.cpp on 2026-08-01
// (which was itself split out of BrnDirectorResourceManager.cpp -- see that file's
// banner) so that the rest of the ICE-cone accessors could be mounted without it.
//
// WHY THIS BOUNDARY -- it is the BLOCKER, not the topic. The other ICE-cone accessors
// cost nothing: GetICEAuthor is a header inline, and GetKeyAnim / GetKeyAnimFromGuid
// reach only ICEList and ICEAuthor, both mounted. GetShakeTakes reaches
// ICEWrapper::GetShakeGroup, whose real body is in BrnDirectorICEWrapper.cpp, and that
// TU costs the link unresolved externals it does not have today.
//
// WHY A SPLIT AND NOT A LINK STUB (unchanged from the parent split's banner): this
// returns a POINTER a caller dereferences. A `return 0` stub would be a silent-drop gate
// of exactly the shape that cost this project the RaceCarState::operator= incident.
// Splitting the file adds NOTHING to the link and invents NOTHING.
//
// NOT MOUNTED. Mount this with BrnDirectorICEWrapper.cpp and merge it -- and
// BrnDirectorResourceManagerICE.cpp -- straight back into BrnDirectorResourceManager.cpp
// at the same time.
// DELETE-WHEN: BrnDirectorICEWrapper.cpp is mountable.
// ============================================================================

#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/BrnDirectorICEWrapper.h"   // ICEWrapper::GetShakeGroup

namespace BrnDirector
{

ICE::ICEGroup* DirectorResourceManager::GetShakeTakes() const
{
    return mpICEWrapper->GetShakeGroup();
}

}
