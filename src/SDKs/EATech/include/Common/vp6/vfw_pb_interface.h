#ifndef VP6_VFW_PB_INTERFACE_H
#define VP6_VFW_PB_INTERFACE_H

// On2 VP6 decoder ("playback") public interface, the Xbox 360 build of the library.
//
// Besides the classic synchronous entry points this build decodes on the EA job system:
// VP6_StartDecoder can attach a scheduler, VP6_DecodeFrameToYUV_JOB queues one frame as a
// decode job plus a completion job that reports the finished picture through a callback.

#include "types.hpp"
#include "SDKs/EATech/include/Common/vp6/codec_common_interface.h"

namespace EA { namespace Jobs { class JobScheduler; } }

struct PB_INSTANCE;
typedef PB_INSTANCE* xPB_INST;

enum PB_COMMAND_TYPE
{
    PBC_SET_POSTPROC,
    PBC_SET_CPUFREE,
    PBC_MAX_PARAM,
    PBC_SET_TESTMODE,
    PBC_SET_PBSTRUCT,
    PBC_SET_BLACKCLAMP,
    PBC_SET_WHITECLAMP,
    PBC_SET_REFERENCEFRAME,
    PBC_SET_DEINTERLACEMODE,
    PBC_SET_ADDNOISE
};

// Completion callback of VP6_DecodeFrameToYUV_JOB: the frame number the decode was queued
// with, the decoded luma plane (border included) and its visible size, then the three
// caller values passed through unchanged.
typedef int (*VP6_DECODE_CALLBACK)(s64 FrameNumber, char* YBufferStart, int YWidth, int YHeight,
                                   void* Context, int Param0, int Param1);

extern "C"
{
    void VP6_VPInitLibrary(void);

    // Create a decoder for ImageWidth x ImageHeight pictures. With UseJobs set the decoder
    // owns two decode/completion job pairs, run on Scheduler with Affinity.
    int  VP6_StartDecoder(xPB_INST* pbi, u32 ImageWidth, u32 ImageHeight, bool UseJobs,
                          EA::Jobs::JobScheduler* Scheduler, int Affinity);
    void VP6_SetPbParam(xPB_INST pbi, PB_COMMAND_TYPE Command, uintptr_t Parameter);
    void VP6_GetYUVConfig(xPB_INST pbi, YUV_BUFFER_CONFIG* YuvConfig);
    int  VP6_DecodeFrameToYUV(xPB_INST pbi, char* VideoBufferPtr, unsigned int ByteCount,
                              u32 ImageWidth, u32 ImageHeight);
    int  VP6_DecodeFrameToYUV_JOB(xPB_INST pbi, char* VideoBufferPtr, unsigned int ByteCount,
                                  u32 ImageWidth, u32 ImageHeight, int FrameNumber,
                                  VP6_DECODE_CALLBACK Callback, void* Context, int Param0,
                                  int Param1);
    int  VP6_StopDecoder(xPB_INST* pbi);
}

#endif // VP6_VFW_PB_INTERFACE_H
