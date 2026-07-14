// ===================================================================================
// BrnGui::CompletedGame  -- the offline post-event "completed game" presentation state
//   class:BrnGui::CompletedGame
//
//   CompletedGame (ctor) @ 0x82500828
// Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnCompletedGame.h"

namespace BrnGui
{
    // @0x82500828 -- default constructor. The X360 image inlines this as a flat run of
    // vtable-pointer stores: the CompletedGame own vtable at +0 (off_820744C0) followed by the
    // default-construction vtable pointer of every embedded GUI sub-component (the license /
    // photo / animation / textfield components and their substate machines) at their fixed byte
    // offsets (+0x38, +0xB0, +0x1D8 ... +0xE78). There are NO explicit non-vtable member stores
    // in the asm at all -- no scalar or sentinel initialisations. On the host each of those
    // vtable pointers is written implicitly by the base ctor chain and the members' own default
    // constructors (mirrors the committed InstantResultsState sibling), so the reconstructed
    // body carries no explicit statements.
    CompletedGame::CompletedGame()
    {
    }
}
