// Fixture diagnostic globals only. Actual scalar formatting and buffered streams
// come from the pinned production CgsStrStream.cpp. This follows GuiModelIO and
// GuiInterpreterIO: optional log categories are off; CGS_ASSERT remains active.
#include <cstddef>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace CgsDev {
namespace Log { StrStreamBase* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}
