// ============================================================================
// GameSource/Director/BrnDirectorResourceManagerICE.cpp
//
// [marked deviation -- FILE SPLIT, not a code change] The two DirectorResourceManager
// bodies that reach the ICE take-runtime cone AND have every callee available. On the
// console these live in GameSource/Director/BrnDirectorResourceManager.cpp with the rest
// of the class (DWARF GameSource/Director/BrnDirectorResourceManager.cpp:22 for
// GetKeyAnimFromGuid); they were moved to this sibling on 2026-08-01, VERBATIM, so that
// the parent TU could be mounted for DirectorResourceManager::Prepare @0x8225CA08 (the 65
// shot-group slot builds -- the whole point of the class) without dragging unresolved
// externals into the link.
//
// ⚠️ RE-SPLIT 2026-08-01 (ICE camera-family closure wave): the three bodies that reach an
// ICEWrapper method with no mounted home -- GetKeyAnim x2 and GetShakeTakes -- moved on
// again, to BrnDirectorResourceManagerICEWrapper.cpp. The split boundary now follows the
// BLOCKER instead of the topic, and what is left costs NOTHING to mount:
//
//     GetICEAuthor       -> ICEWrapper::GetAuthor              header inline, free
//     GetKeyAnimFromGuid -> ICEAuthor::FindEditedTakeFromGuid  ICEAuthorTakeOps.cpp
//                           ICEList::GetICETakeDataFromGuid    ICEList.cpp (mounted)
//
// Both are needed by the ICE camera family (BehaviourIceAnim reaches GetICEAuthor,
// KeyAnimController reaches GetKeyAnimFromGuid), which is why they are carved out.
//
// ⚠️ STILL A SPLIT. Merge this AND BrnDirectorResourceManagerICEWrapper.cpp back into
// BrnDirectorResourceManager.cpp the moment the ICEWrapper group lands -- neither split
// has a reason to outlive its blocker.
// DELETE-WHEN: the ICE take-runtime group lands (BrnDirectorICEWrapper.cpp + a
// BrnResource::MakeICEMovieId home).
// ============================================================================

#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/BrnDirectorICEWrapper.h"   // ICEWrapper::GetAuthor (header inline)
#include "SDKs/Packages/ICE/ICEAuthor.hpp"               // ICE::ICEAuthor::FindEditedTakeFromGuid
#include "SharedClasses/DataLists/ICEList.h"             // BrnResource::ICEList::GetICETakeDataFromGuid

namespace BrnDirector
{

// The in-game ICE editor's author/edit store. The console reaches it at manager +560 --
// i.e. through mpICEWrapper -- which is why this is not a plain member read.
//
// No standalone X360 symbol: every call site expands to the two-instruction
// `lwz r11, 0x230(manager); addi r3, r11, 0x2750` (see GetKeyAnimFromGuid's own asm right
// below), i.e. the manager's mpICEWrapper load followed by ICEWrapper::GetAuthor taken in
// place. GetAuthor is already a header inline, so this forwarder reproduces exactly that
// expansion.
ICE::ICEAuthor& DirectorResourceManager::GetICEAuthor() const
{
    return mpICEWrapper->GetAuthor();
}

// @0x821F69A8 -- resolve an ICE take GUID to its take data. The editor's edited-take list
// wins over the on-disk list, so an in-editor edit is what plays back.
//
// The console body is five instructions of substance and matches this shape exactly:
//     lwz  r11, 0x230(r31)                 ; mpICEWrapper
//     addi r3,  r11, 0x2750                ; ->GetAuthor()   (inlined)
//     bl   ICE::ICEAuthor::FindEditedTakeFromGuid
//     cmplwi r3, 0 ; bne <done>            ; the editor's take wins
//     lwz  r3,  0x220(r31)                 ; mpICEDictionaryList
//     bl   BrnResource::ICEList::GetICETakeDataFromGuid
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
