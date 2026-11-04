// DirtySDK CommUDPTick, ARTIST 0x82B2A960.
#include "types.hpp"
struct CommUDPRef;
extern "C" u32 NetTick(void);
extern "C" u32 CommUDPTick(CommUDPRef*) { return NetTick(); }
