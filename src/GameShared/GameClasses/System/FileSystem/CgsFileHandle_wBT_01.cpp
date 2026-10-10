#include "GameShared/GameClasses/System/FileSystem/CgsFileHandle.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"

// CgsFileHandle partfile (blocked-TU wave): the two header-inline FileHandle members the replay
// module's file wait needs, beside their home CgsFileHandle.cpp.
namespace CgsFileSystem
{
    // Header-inline on the console (ReplayModule::WaitForOpenReplayFiles inlines it as a direct
    // FileSystem::GetStatus(mpFileSystem, muFileId) call): the file-system slot state.
    FileState FileHandle::GetStatus() const
    {
        return mpFileSystem->GetStatus(muFileId);
    }

    // Header-inline on the console (a two-word copy at every assignment site).
    FileHandle& FileHandle::operator=(const FileHandle& lOtherHandle)
    {
        mpFileSystem = lOtherHandle.mpFileSystem;
        muFileId     = lOtherHandle.muFileId;
        return *this;
    }
}
