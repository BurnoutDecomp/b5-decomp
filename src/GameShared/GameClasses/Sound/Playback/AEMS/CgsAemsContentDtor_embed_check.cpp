// Embed-check for CgsAemsContent.h (grown) + CgsAemsContentDtor.cpp:
//   CgsSound::Playback::AemsContent::`vector deleting destructor' @ 0x826DA150
//
// Forces emission of the deleting destructor (member/base dtor unlink + operator
// delete) and pins down the layout the destructor depends on without asserting
// absolute X360 offsets across pointer members (project rule).

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsContent.h"

using namespace CgsSound::Playback;

// AemsContent derives from Content (object begins with the Content base; mLoader
// sits immediately after it -- the embedded ResourcePtr alias links the dtor unlinks
// live inside mLoader). ABI-stable structural invariants only:
static_assert(static_cast<Content*>(static_cast<AemsContent*>(nullptr)) == nullptr,
              "AemsContent must derive from Content");
static_assert(sizeof(AemsContent) > sizeof(Content),
              "AemsContent must add mLoader/handle/flags/data past the Content base");

// delete-through-pointer forces the compiler to emit the deleting destructor
// (member/base dtor unlink + operator delete).
void embed_delete_aems_content(AemsContent* lpContent)
{
    delete lpContent;
}
