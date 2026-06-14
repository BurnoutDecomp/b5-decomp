#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::movie::VideoRenderable::GetDataBufSizes  @ 0x82BC0F18
//   rw::movie::VideoRenderable::VideoRenderable  @ 0x82BC0EB8
//   rw::movie::VideoRenderable::~VideoRenderable @ 0x82B42DC0
//
// GetDataBufSizes computes the three plane sizes for a frame of width*height pixels,
// per pixel format (a3) and a planar/packed flag (a4). The constructor zero-inits the
// renderable and installs the vtable word (0x37060034) and default plane count (4);
// the destructor clears the vtable word.

namespace rw
{
    namespace movie
    {
        class VideoRenderable
        {
        public:
            int  GetDataBufSizes(int liWidth, int liHeight, u32 luFormat, int liPacked, u32* pOutSizes);
            VideoRenderable();
            ~VideoRenderable();

        private:
            u32 muVTable;
            u32 muUnknown4;
            u32 muUnknown8;
            u32 muUnknown12;
            u32 muUnknown16;
            u32 muUnknown20;
            u32 muUnknown24;
            u8  mPad28[12];
            u32 muUnknown40;
            u32 muUnknown44;
            u32 muPlaneCount;
            u32 muUnknown52;
            u32 muUnknown56;
            u32 muUnknown60;
            u32 muUnknown64;
            u32 muUnknown68;
            u32 muUnknown72;
            u32 muUnknown76;
            u8  mbUnknown80;
            u8  mbUnknown81;
        };

        int VideoRenderable::GetDataBufSizes(int liWidth, int liHeight, u32 luFormat, int liPacked, u32* pOutSizes)
        {
            if (luFormat < 2)
            {
                u32 luArea = static_cast<u32>(liWidth * liHeight);
                if (liPacked == 1)
                {
                    pOutSizes[1] = 0;
                    pOutSizes[2] = 0;
                    pOutSizes[0] = 3 * (luArea >> 1);
                }
                else
                {
                    pOutSizes[0] = luArea;
                    pOutSizes[1] = luArea >> 2;
                    pOutSizes[2] = luArea >> 2;
                }
            }
            else if (luFormat == 2)
            {
                if (liPacked != 1)
                    return liWidth;
                pOutSizes[0] = 3 * liWidth * liHeight;
                pOutSizes[1] = 0;
                pOutSizes[2] = 0;
            }
            else if (luFormat < 4 && liPacked == 1)
            {
                pOutSizes[0] = 4 * liWidth * liHeight;
                pOutSizes[1] = 0;
                pOutSizes[2] = 0;
            }
            return liWidth;
        }

        VideoRenderable::VideoRenderable()
        {
            muPlaneCount = 4;
            muUnknown4 = 0;
            muUnknown8 = 0;
            muUnknown12 = 0;
            muUnknown16 = 0;
            muUnknown20 = 0;
            muUnknown24 = 0;
            muUnknown40 = 0;
            muUnknown44 = 0;
            muUnknown52 = 0;
            muUnknown56 = 0;
            muUnknown60 = 0;
            muUnknown68 = 0;
            muUnknown72 = 0;
            muUnknown76 = 0;
            muVTable = 923039540;
            muUnknown64 = 0;
            mbUnknown80 = 0;
            mbUnknown81 = 0;
        }

        VideoRenderable::~VideoRenderable()
        {
            muVTable = 0;
        }
    }
}
