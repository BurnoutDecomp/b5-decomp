// CgsFileLog.cpp — CgsFileSystem::FileLog
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Only one function is attested for this TU in the X360 ledger: the constructor
// FileLog::FileLog @0x828DCE98. It registers the single global logger through
// the static singleton _mpThis and touches no instance state. All other
// declared methods (Construct/Destruct/Log*/ReadLogs/PushEntry/PopEntry) are
// declare-only here — their bodies are separate TUs.

#include "GameShared/GameClasses/System/FileSystem/CgsFileLog.h"
#define CGS_ARTIST_CGSFILELOG_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsFileLog.cpp"
namespace CgsFileSystem
{
    // The one global logger. External linkage; written by the constructor and
    // read by every using TU through the class declaration. (X360 dword_830EA3F8.)
    FileLog* FileLog::_mpThis = 0;

    // @0x828DCE98. Asserts no logger has been created yet, then registers this
    // as the single global file-system logger. No instance members are touched.
    FileLog::FileLog()
    {
        CGS_ASSERT_AT(_mpThis == 0,
                   "Already created a file system logger - can't create any more!\n", CGS_ARTIST_CGSFILELOG_FILE, 44);
        _mpThis = this;
    }

} // namespace CgsFileSystem
