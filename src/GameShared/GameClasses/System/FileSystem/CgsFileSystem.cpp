#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"

// CgsFileSystem::FileSystem - see the header. DEFERRED (rw::core::filesys / Win32) inert
// stubs; Prepare/Release report success so the ResourceModule stage machine advances.
namespace CgsFileSystem
{
    void FileSystem::Construct() {}
    bool FileSystem::Prepare()  { return true; }
    bool FileSystem::Release()  { return true; }
    void FileSystem::Destruct() {}
}
