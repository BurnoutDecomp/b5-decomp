#pragma once

#include "types.hpp"

struct NetGameUtilRefT;

extern "C"
{
    NetGameUtilRefT* NetGameUtilCreate();
    void NetGameUtilReset(NetGameUtilRefT* ref);
    void NetGameUtilDestroy(NetGameUtilRefT* ref);
    void NetGameUtilControl(NetGameUtilRefT* ref, s32 selector, s32 value);
    s32 NetGameUtilStatus(NetGameUtilRefT* ref, s32 selector, void* pBuffer, s32 size);
    s32 NetGameUtilConnect(NetGameUtilRefT* ref, s32 connection, const char* address);
    void* NetGameUtilComplete(NetGameUtilRefT* ref);
}
