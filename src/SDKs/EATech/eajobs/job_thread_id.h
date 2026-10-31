#pragma once

#include "types.hpp"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

namespace EA { namespace Jobs {
typedef u64 JobThreadId;

// ARTIST 82915920 and 829158F8, respectively.
JobThreadId ToJobThreadId(EA::Thread::ThreadId lThreadId);
JobThreadId GetJobThreadId();
}}
