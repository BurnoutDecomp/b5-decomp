#pragma once

#include "types.hpp"

struct NetGameUtilRefT;

extern "C"
{
    NetGameUtilRefT* NetGameUtilCreate();
    void NetGameUtilReset(NetGameUtilRefT* ref);
    void NetGameUtilDestroy(NetGameUtilRefT* ref);
}
