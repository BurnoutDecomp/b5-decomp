#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"

// CgsSound::Logic::Content -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   Construct @ 0x826C4CA0
//   Destruct  @ 0x826C4D98

namespace CgsSound
{
namespace Logic
{

// ARTIST strings: the Construct/Destruct asserts name this file (0x820B3288).
#define CGS_ARTIST_LOGIC_CONTENT_CPP "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsContent.cpp"

// DWARF CgsContent.cpp:72 (this file, so the sound unity sees it). ARTIST's vector deleting
// destructor 0x826DC378 installs the vtable and drops the held reference: mContentHandle's
// destructor; ~ObjectPool<StateManager::RegisteredContent,4,int> 0x826EAC90 expands it per element.
Content::~Content()
{
}

// ---------------------------------------------------------------------------
// CgsSound::Logic::Content::Construct  @ 0x826C4CA0
//
// Create the held playback content through the module's embedded Playback::Module
// (X360 r4 == lpModule + 0x238), keyed by the module's unique id (module vtable
// +0x48) and the two Command::QueueElement selectors. The returned temporary handle
// is assigned (the inlined Handle::operator=: acquire the new, release the old,
// store) and then destroyed (its release), ARTIST 0x826C4D20..0x826C4D60.
// ---------------------------------------------------------------------------
void Content::Construct(Module* lpModule,
                        Command::QueueElement lFactoryName,
                        Command::QueueElement lContentSpecName)
{
    CGS_ASSERT_AT(lpModule && !IsCreated(), "( lpModule ) && ( !IsCreated() )",
                  CGS_ARTIST_LOGIC_CONTENT_CPP, 95);

    // DWARF BrnSoundUnity: the local `uint32_t lContentId` (CgsContent.cpp:96).
    const u32 lContentId = lpModule->GetUniqueId();
    mContentHandle = lpModule->GetPlaybackModule().CreateContent(
        lContentId, lFactoryName, lContentSpecName);
    mpModule = lpModule;

    CGS_ASSERT_AT(mContentHandle, "mContentHandle", CGS_ARTIST_LOGIC_CONTENT_CPP, 102);
}

// ---------------------------------------------------------------------------
// CgsSound::Logic::Content::Destruct  @ 0x826C4D98
//
// Tear down the held content: assert it was created (CgsContent.cpp:119), begin its
// removal through the handle (Handle::operator-> and its "mpObject" assert,
// CgsHandle.h:287), then drop the reference by assigning the empty handle (the
// inlined operator=: release, store null, ARTIST 0x826C4E0C..0x826C4E20).
// ---------------------------------------------------------------------------
void Content::Destruct()
{
    CGS_ASSERT_AT(IsCreated(), "Content not yet created!", CGS_ARTIST_LOGIC_CONTENT_CPP, 119);
    mContentHandle->BeginRemove();
    mContentHandle = ContentHandle();
}

bool Content::IsLoaded() const
{
    // DWARF CgsContent.h:233 `bool lbResult`; ARTIST 0x82692BB8 reads the state through the const
    // Handle::operator-> and its "mpObject" assert (CgsHandle.h:296).
    bool lbResult = false;
    if (IsCreated())
    {
        lbResult = mContentHandle->GetContentState() == CgsSound::Playback::E_CONTENT_STATE_LOADED;
    }
    return lbResult;
}

} // namespace Logic
} // namespace CgsSound

#undef CGS_ARTIST_LOGIC_CONTENT_CPP
