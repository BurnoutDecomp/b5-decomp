#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828177E0
//   (CgsDev::DebugComponentPerfMonCpu::GetName)
//
// Ground truth: the Feb-2007 header declares this as an inline virtual override:
//     virtual const char* GetName() const { return "CPU Monitors"; }
// The X360 pseudocode (`return "CPU Monitors";`) agrees. Reconstructed as the
// out-of-line definition of that override.

namespace CgsDev
{
    class DebugComponentPerfMonCpu
    {
    protected:
        virtual const char* GetName() const;
    };

    const char* DebugComponentPerfMonCpu::GetName() const
    {
        return "CPU Monitors";
    }
}
