// Native DirtySDK 5.5.3, ARTIST 82B27BF8 is a tail branch to SocketLookup.
#include "protoname.h"
extern "C" HostentT* SocketLookup(const char* pName, s32 iTimeout);
extern "C" HostentT* ProtoNameAsync(const char* pName, s32 iTimeout)
{
    return SocketLookup(pName, iTimeout);
}
