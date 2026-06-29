#pragma once

// CgsNetwork::SyncTimeMessageManager moved to its canonical owning header,
// CgsSyncTimeBase.h (matching the original game source file name CgsSyncTimeBase.cpp,
// which the X360 assert strings reference). This file is kept as a compatibility shim
// so existing includers of the old path keep compiling and the class has a single
// definition (no ODR conflict).

#include "GameShared/GameClasses/Network/Time/CgsSyncTimeBase.h"
