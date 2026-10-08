// BrnDirector::DebugComponent ("Camera" debug page) -- the playlist save / load actions and the ICE
// editor entry (partfile of BrnDirectorModuleDebugCompononent.cpp).

#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"

#include "GameSource/Director/BrnDirectorModule.h"                          // DirectorModule::GetMainDirector
#include "GameSource/Director/BrnMainDirector.h"                            // MainDirector::GetArbitrator / GetICEWrapper
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                    // SharedPlaylists::Serialise<S>
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"   // Camera::DebugMenuSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"    // Camera::TextFileReadSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // Camera::TextFileWriteSerialiser
#include "SDKs/Packages/ICE/ICEAuthor.hpp"                                  // ICE::ICEAuthor::CreateNewTake
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT

namespace BrnDirector
{

// Write the arbitrator's shared playlists to "d:\\playlists.txt".
void DebugComponent::SavePlaylists(void* lpUserData)
{
    DebugComponent* lpThis = static_cast<DebugComponent*>(lpUserData);

    Camera::TextFileWriteSerialiser lSerialiser;
    lSerialiser.Construct("d:\\playlists.txt");
    lpThis->mpDirectorModule->GetMainDirector().GetArbitrator().GetStateContainer().GetSharedPlaylists().Serialise(lSerialiser);
    lSerialiser.Destruct();
}

// Read the arbitrator's shared playlists back from "d:\\playlists.txt", taking their menu entries
// down first and rebuilding them for the loaded lists.
void DebugComponent::LoadPlaylists(void* lpUserData)
{
    DebugComponent*  lpThis      = static_cast<DebugComponent*>(lpUserData);
    SharedPlaylists& lrPlaylists = lpThis->mpDirectorModule->GetMainDirector().GetArbitrator().GetStateContainer().GetSharedPlaylists();

    {
        Camera::DebugMenuSerialiser lSerialiser;
        lSerialiser.Construct(lpThis, Camera::DebugMenuSerialiser::E_MODE_REMOVE_FROM_MENU);
        lrPlaylists.Serialise(lSerialiser);
    }

    {
        Camera::TextFileReadSerialiser lSerialiser;
        lSerialiser.Construct("d:\\playlists.txt");
        lrPlaylists.Serialise(lSerialiser);
        lSerialiser.Destruct();
    }

    {
        Camera::DebugMenuSerialiser lSerialiser;
        lSerialiser.Construct(lpThis, Camera::DebugMenuSerialiser::E_MODE_ADD_TO_MENU);
        lrPlaylists.Serialise(lSerialiser);
    }
}

// Create a new ICE take named "New Take" and open the in-game ICE editor on it.
void DebugComponent::StartEditor(void* lpUserData)
{
    DebugComponent* lpThis      = static_cast<DebugComponent*>(lpUserData);
    ICEWrapper&     lrICEWrapper = lpThis->mpDirectorModule->GetMainDirector().GetICEWrapper();

    ICE::ICETakeData* lpTakeData = lrICEWrapper.GetAuthor().CreateNewTake("New Take", -1);
    CGS_ASSERT(lpTakeData, "lpTakeData");
    lrICEWrapper.EditorOn(lpTakeData);
}

} // namespace BrnDirector
