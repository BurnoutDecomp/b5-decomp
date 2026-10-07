#pragma once
#include "types.hpp"

namespace rw { namespace movie
{
    // ARTIST 82BC0EB8/82B42DC0 and DecFIGS renderable.h: a typed payload,
    // not a polymorphic object. Native pointers widen by their declarations.
    class VideoRenderable
    {
    public:
        enum VideoFormat
        {
            VIDEOFORMAT_YV12, VIDEOFORMAT_YV12_BORDER48, VIDEOFORMAT_RGB24,
            VIDEOFORMAT_ARGB32, VIDEOFORMAT_MAXNUM
        };
        using FrameNumber = s32;
        VideoRenderable();
        ~VideoRenderable();
        void SetData(u8* lpData, unsigned int luIndex) { mData[luIndex] = lpData; }
        u8* GetData(unsigned int luIndex) { return mData[luIndex]; }
        void SetSize(u32 luSize, unsigned int luIndex) { mSize[luIndex] = luSize; }
        u32 GetSize(unsigned int luIndex) { return mSize[luIndex]; }
        void SetStride(u32 luStride, unsigned int luIndex) { mStride[luIndex] = luStride; }
        u32 GetStride(unsigned int luIndex) { return mStride[luIndex]; }
        void SetWidth(unsigned int luWidth) { mWidth = luWidth; }
        unsigned int GetWidth() { return mWidth; }
        void SetHeight(unsigned int luHeight) { mHeight = luHeight; }
        unsigned int GetHeight() { return mHeight; }
        void SetFormat(VideoFormat leFormat) { mFormat = leFormat; }
        VideoFormat GetFormat() { return mFormat; }
        void SetFrameNumber(FrameNumber liFrame) { mFrameNumber = liFrame; }
        FrameNumber GetFrameNumber() { return mFrameNumber; }
        void SetNumBuffersUsed(unsigned int luCount) { mNumBuffersUsed = luCount; }
        unsigned int GetNumBuffersUsed() { return mNumBuffersUsed; }
        void IncrementUseCount() { ++mUseCount; }
        void DecrementUseCount() { --mUseCount; }
        int GetUseCount() { return mUseCount; }
        void SetContext(void* lpContext, unsigned int luIndex) { mContext[luIndex] = lpContext; }
        void* GetContext(unsigned int luIndex) { return mContext[luIndex]; }
        bool IsReadyToRender() { return mIsReadyToRender; }
        void SetReadyToRender(bool lbReady) { mIsReadyToRender = lbReady; }
        bool CheckValidity() { return mMagicNumber == KU_MAGIC_NUMBER; }
        bool GetDropFrameFlag() { return mDropFrameFlag; }
        void SetDropFrameFlag(bool lbDrop) { mDropFrameFlag = lbDrop; }
        bool IsFrameContentFlipped() { return mFlipFlag; }
        void SetFrameContentFlippedFlag(bool lbFlip) { mFlipFlag = lbFlip; }
        // ARTIST 82BC0F18 uses r3/r4 for width/height: no implicit this.
        static void GetDataBufSizes(unsigned int luWidth, unsigned int luHeight,
                                    VideoFormat leFormat, unsigned int luPacked, u32* lpSizes);
    private:
        static const u32 KU_MAGIC_NUMBER = 0x37047734;
        u32 mMagicNumber;
        u8* mData[3];
        u32 mSize[3];
        u32 mStride[3];
        unsigned int mWidth, mHeight;
        VideoFormat mFormat;
        FrameNumber mFrameNumber;
        unsigned int mNumBuffersUsed;
        int mUseCount;
        volatile bool mIsReadyToRender;
        void* mContext[3];
        bool mDropFrameFlag, mFlipFlag;
    };
} }
