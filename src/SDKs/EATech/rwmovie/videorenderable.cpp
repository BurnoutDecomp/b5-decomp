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
            void* Construct();
            void* Destruct();
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

        void* VideoRenderable::Construct()
        {
            uintptr_t lBase = reinterpret_cast<uintptr_t>(this);
            auto Word = [lBase](int liOff) -> u32& { return *reinterpret_cast<u32*>(lBase + liOff); };

            Word(48) = 4;
            Word(4)  = 0;
            Word(8)  = 0;
            Word(12) = 0;
            Word(16) = 0;
            Word(20) = 0;
            Word(24) = 0;
            Word(40) = 0;
            Word(44) = 0;
            Word(52) = 0;
            Word(56) = 0;
            Word(60) = 0;
            Word(68) = 0;
            Word(72) = 0;
            Word(76) = 0;
            Word(0)  = 923039540;   // vtable word (0x37060034)
            Word(64) = 0;
            *reinterpret_cast<u8*>(lBase + 80) = 0;
            *reinterpret_cast<u8*>(lBase + 81) = 0;
            return this;
        }

        void* VideoRenderable::Destruct()
        {
            *reinterpret_cast<u32*>(this) = 0;
            return this;
        }
    }
}
