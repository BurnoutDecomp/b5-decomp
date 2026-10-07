#include "SDKs/EATech/rwmovie/videorenderable.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::movie::VideoRenderable::GetDataBufSizes  @ 0x82BC0F18
//   rw::movie::VideoRenderable::VideoRenderable  @ 0x82BC0EB8
//   rw::movie::VideoRenderable::~VideoRenderable @ 0x82B42DC0
//
// GetDataBufSizes computes the three plane sizes for a frame of width*height pixels,
// per pixel format (a3) and a planar/packed flag (a4). The constructor zero-inits the
// renderable and installs the magic word (0x37047734) and invalid-format value (4);
// the destructor clears the magic word.

namespace rw
{
    namespace movie
    {
        void VideoRenderable::GetDataBufSizes(unsigned int liWidth, unsigned int liHeight, VideoFormat luFormat, unsigned int liPacked, u32* pOutSizes)
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
                    return;
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
            return;
        }

        VideoRenderable::VideoRenderable()
        {
            mMagicNumber = KU_MAGIC_NUMBER;
            for (unsigned int lu = 0; lu < 3; ++lu)
            {
                mData[lu] = nullptr;
                mSize[lu] = 0;
                mContext[lu] = nullptr;
            }
            // ARTIST leaves the three stride words untouched.
            mWidth = mHeight = 0;
            mFormat = VIDEOFORMAT_MAXNUM;
            mFrameNumber = 0;
            mNumBuffersUsed = 0;
            mUseCount = 0;
            mIsReadyToRender = false;
            mDropFrameFlag = false;
            mFlipFlag = false;
        }

        VideoRenderable::~VideoRenderable()
        {
            mMagicNumber = 0;
        }
    }
}
