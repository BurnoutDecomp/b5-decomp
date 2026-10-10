#ifndef VP6_CODEC_COMMON_INTERFACE_H
#define VP6_CODEC_COMMON_INTERFACE_H

// On2 VP6 codec: the decoded-picture description shared by the codec and its callers.

// The planes of a decoded frame. The Y/U/V pointers address the visible picture inside the
// reconstruction buffer; YBufferStart is the start of the luma plane including its border.
struct YUV_BUFFER_CONFIG
{
    int   YWidth;
    int   YHeight;
    int   YStride;

    int   UVWidth;
    int   UVHeight;
    int   UVStride;

    char* YBuffer;
    char* UBuffer;
    char* VBuffer;

    char* YBufferStart;
};

#endif // VP6_CODEC_COMMON_INTERFACE_H
