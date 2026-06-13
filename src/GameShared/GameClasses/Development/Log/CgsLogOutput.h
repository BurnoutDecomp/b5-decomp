#ifndef CGS_LOG_OUTPUT_H
#define CGS_LOG_OUTPUT_H

#include "types.hpp"
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);

namespace CgsDev
{
namespace Log
{
class LogOutput
{
public:
    int Append(int a1, int a2);
};

inline int LogOutput::Append(int a1, int a2)
{
    OutputDebugStringA((const char*)a2);
    return 0;
}
}
}
#endif
