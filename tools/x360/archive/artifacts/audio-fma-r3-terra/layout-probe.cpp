#include <stddef.h>
#include "rw/audio/core/Butterworth.h"
#include "rw/audio/core/plugins/HighPassButterworth.h"
#include "rw/audio/core/plugins/LowPassButterworth.h"
typedef char ButterworthSize[(sizeof(rw::audio::core::Butterworth)==0x30)?1:-1];
typedef char HighTail[(offsetof(rw::audio::core::HighPassButterworth,mButterworth)==0x60)?1:-1];
typedef char LowTail[(offsetof(rw::audio::core::LowPassButterworth,mButterworth)==0x60)?1:-1];
