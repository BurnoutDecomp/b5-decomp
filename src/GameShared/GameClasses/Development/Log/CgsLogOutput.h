#ifndef CGS_LOG_OUTPUT_H
#define CGS_LOG_OUTPUT_H

#include "types.hpp"

namespace CgsDev
{
namespace Log
{
class LogOutput
{
public:
    int Append(const char* lpcText);
};
}
}
#endif
