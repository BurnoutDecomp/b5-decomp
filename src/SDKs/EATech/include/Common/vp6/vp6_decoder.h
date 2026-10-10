#ifndef VP6_DECODER_H
#define VP6_DECODER_H

// On2 VP6 decoder internals: the playback instance, its bitstream readers, entropy state and the
// entry points the library's translation units share.
//
// Every member offset noted "+0xNNN" is the console offset proven by the decoder's code; the
// host build reaches every member by name, so the 8-byte host pointers only move the tail of
// each structure. Allocations use the host sizeof, never the console byte counts.

#include "types.hpp"
#include "SDKs/EATech/include/Common/vp6/vfw_pb_interface.h"

namespace EA
{
    namespace Jobs { struct Job; struct Param; }
    namespace Thread { class Semaphore; }
    namespace Allocator { class ICoreAllocator; }
}

#if defined(_MSC_VER)
    #define VP6_ALIGN16 __declspec(align(16))
#else
    #define VP6_ALIGN16 alignas(16)
#endif

// ---- bitstream readers ---------------------------------------------------------------------

// Arithmetic (boolean) decoder.
struct BOOL_CODER
{
    unsigned int         lowvalue;   // +0x00
    unsigned int         range;      // +0x04
    unsigned int         value;      // +0x08
    int                  count;      // +0x0C bits left before the next byte is shifted in
    unsigned int         pos;        // +0x10 next byte to read
    const unsigned char* buffer;     // +0x14
};

// Raw big-endian bit reader over the uncompressed frame header.
struct HEADER_BUFFER
{
    const unsigned char* buffer;          // +0x00
    unsigned int         value;           // +0x04 left-aligned bits not yet consumed
    unsigned int         bits_available;  // +0x08
    unsigned int         pos;             // +0x0C next 32-bit word to load
};

// Bit reader of the Huffman-coded coefficient partition.
struct HUFF_BIT_READER
{
    int                  BitsLeft;   // +0x00 valid bits in Value
    unsigned int         Value;      // +0x04
    const unsigned char* Position;   // +0x08 next 32-bit word to load
};

// ---- Huffman trees -------------------------------------------------------------------------

// One branch of a Huffman tree node: either a token value (Leaf) or the index of another node.
struct HUFF_CHILD
{
    unsigned int Value : 7;
    unsigned int Leaf  : 1;
    unsigned int Spare : 24;
};

struct HUFF_NODE
{
    HUFF_CHILD    Left;    // +0x00 the 0 branch
    HUFF_CHILD    Right;   // +0x04 the 1 branch
    unsigned char Freq;    // +0x08 probability of the 0 branch, in 1/256
};

// Six-bit look-ahead table entry of a Huffman tree: where the walk ends after Length bits.
struct HUFF_LUT_ENTRY
{
    unsigned short Leaf   : 1;
    unsigned short Value  : 11;
    unsigned short Length : 4;
};

// Eight-bit look-ahead table entry of the DC token tree, fully unpacked: the coefficient value,
// the zero run (minus one) or the end-of-block flag, the context for the next token and how many
// bits the token consumed (0 when the eight bits do not hold a whole token).
struct TOKEN_LUT_ENTRY
{
    short          Value;
    unsigned short Eob     : 1;
    unsigned short Run     : 7;
    unsigned short Context : 2;
    unsigned short Bits    : 6;
};

// ---- per-block / per-macroblock state ------------------------------------------------------

struct MOTION_VECTOR
{
    short x;
    short y;
};

// Context a decoded block leaves for its right and lower neighbours.
struct BLOCK_CONTEXT
{
    unsigned char  Token;       // +0x00 1 when the block's DC was non-zero
    int            BlockMode;   // +0x04 coding mode of the block, -1 before the first one
    unsigned short Frame;       // +0x08 reference frame of the block, 4 before the first one
    short          Dc;          // +0x0A reconstructed DC
    int            Spare;       // +0x0C
};

// Extra-bit coding of one DCT token.
struct VP6_TOKENEXTRABITS
{
    unsigned short MinVal;     // smallest value the token codes
    short          Length;     // extra bits minus one
    unsigned char  Probs[12];  // probability of each extra bit, most significant first
};

// ---- frame geometry ------------------------------------------------------------------------

// The picture size and strides the instance was configured with.
struct CONFIG_TYPE
{
    unsigned int VideoFrameWidth;    // +0x00
    unsigned int VideoFrameHeight;   // +0x04
    unsigned int YStride;            // +0x08
    unsigned int UVStride;           // +0x0C
    unsigned int Interlaced;         // +0x10
};

// Plane layout of one reconstruction buffer: where each plane's visible picture starts and the
// width of the replicated border around it.
struct FRAME_INFO
{
    unsigned int YDataOffset;   // +0x00
    unsigned int UDataOffset;   // +0x04
    unsigned int VDataOffset;   // +0x08
    unsigned int HFragments;    // +0x0C picture width in 8x8 blocks
    unsigned int VFragments;    // +0x10 picture height in 8x8 blocks
    unsigned int YStride;       // +0x14
    unsigned int UVStride;      // +0x18
    unsigned int UMVBorder;     // +0x1C luma border width
};

// ---- dequantisation ------------------------------------------------------------------------

// Console size 0x88C bytes; only the members below are used by the decoder.
struct QUANTIZER
{
    int           FrameQIndex;                 // +0x00 quantiser index of the current frame
    int           ThisFrameQuantizerValue;     // +0x04
    int           LastFrameQuantizerValue;     // +0x08 value the tables were last built for
    unsigned char QuantIndex[64];              // +0x0C scan position of each raster coefficient
    short*        DequantCoeffs[2];            // +0x4C luma / chroma, per raster coefficient
    float*        FloatDequantCoeffs[2];       // +0x54 the same values as floats
    unsigned char Unused[0x88C - 0x5C];
};

// ---- job records ---------------------------------------------------------------------------

// Data block of a decode job.
struct VP6_DECODE_JOB_DATA
{
    xPB_INST     pbi;            // +0x00
    char*        VideoBuffer;    // +0x04
    unsigned int ByteCount;      // +0x08
    int          FrameNumber;    // +0x0C
    u32          ImageWidth;     // +0x10 passed on to VP6_DecodeFrameToYUV, never filled in
    u32          ImageHeight;    // +0x14
};

// Data block of a completion job.
struct VP6_CALLBACK_JOB_DATA
{
    s64                 FrameNumber;  // +0x00
    VP6_DECODE_CALLBACK Callback;     // +0x08
    void*               Context;      // +0x0C
    int                 Param0;       // +0x10
    int                 Param1;       // +0x14
    xPB_INST            pbi;          // +0x18
};

// ---- the playback instance -----------------------------------------------------------------

struct PB_INSTANCE
{
    // Current macroblock / block.
    short*         Coeffs;                      // +0x000 six blocks of 64 coefficients
    BLOCK_CONTEXT* AboveContext;                // +0x004 context of the block being decoded
    BLOCK_CONTEXT* LeftContext;                 // +0x008
    short*         LastDc;                      // +0x00C last DC per reference frame, this plane
    int            Mode;                        // +0x010 macroblock coding mode
    int            BlockMode[6];                // +0x014
    MOTION_VECTOR  Mv[6];                       // +0x02C
    MOTION_VECTOR  NearestInterMv;              // +0x044
    MOTION_VECTOR  NearInterMv;                 // +0x048
    int            NearestMvIndex;              // +0x04C neighbour index of NearestInterMv, 12 if none
    MOTION_VECTOR  NearestGoldMv;               // +0x050
    MOTION_VECTOR  NearGoldMv;                  // +0x054
    int            NearestGoldMvIndex;          // +0x058
    unsigned char  Unused05C[0xA4 - 0x5C];
    int            PixelRow;                    // +0x0A4 position of the block in its plane
    int            PixelColumn;                 // +0x0A8
    int            ReconOffset;                 // +0x0AC offset of the block in a frame buffer
    int            ReconStride;                 // +0x0B0 destination stride (doubled when interlaced)
    int            CurrentPlane;                // +0x0B4
    int            MvShift;                     // +0x0B8 2 luma, 3 chroma
    int            MvMask;                      // +0x0BC 3 luma, 7 chroma
    int            CurrentStride;               // +0x0C0 stride of the plane
    int            MbInterlaced;                // +0x0C4 the current macroblock is field coded
    BLOCK_CONTEXT  LeftBlockContexts[4];        // +0x0C8 upper luma, lower luma, U, V
    BLOCK_CONTEXT* AboveBlockContexts[3];       // +0x108 per plane, two (luma) or one entry per macroblock
    void*          AboveBlockContextsAlloc[3];  // +0x114
    short          LastDcValues[3][4];          // +0x120 [plane][reference frame]
    QUANTIZER*     quantizer;                   // +0x138
    unsigned int   Buff2Offset;                 // +0x13C start of the coefficient partition
    HEADER_BUFFER  HeaderBits;                  // +0x140
    unsigned char  Unused150[0x158 - 0x150];
    int*           FragmentValues;              // +0x158 allocated with the code arrays only, never read
    void*          FragmentValuesAlloc;         // +0x15C
    BOOL_CODER     br;                          // +0x160 modes, vectors and probabilities
    unsigned char  Unused178[0x180 - 0x178];
    BOOL_CODER     br2;                         // +0x180 arithmetic-coded coefficients
    unsigned char  Unused198[0x1A0 - 0x198];
    HUFF_BIT_READER br3;                        // +0x1A0 Huffman-coded coefficients
    FRAME_INFO*    ReconFrameInfo;              // +0x1AC
    CONFIG_TYPE    Configuration;               // +0x1B0
    unsigned int   PostProcessingLevel;         // +0x1C4
    unsigned int   YPlaneSize;                  // +0x1C8
    unsigned int   UVPlaneSize;                 // +0x1CC
    unsigned int   VFragments;                  // +0x1D0
    unsigned int   HFragments;                  // +0x1D4
    unsigned int   UnitFragments;               // +0x1D8
    unsigned int   YPlaneFragments;             // +0x1DC
    unsigned int   UVPlaneFragments;            // +0x1E0
    unsigned int   ReconYPlaneSize;             // +0x1E4
    unsigned int   ReconUVPlaneSize;            // +0x1E8
    unsigned int   YDataOffset;                 // +0x1EC
    unsigned int   UDataOffset;                 // +0x1F0
    unsigned int   VDataOffset;                 // +0x1F4
    unsigned int   ReconYDataOffset;            // +0x1F8
    unsigned int   ReconUDataOffset;            // +0x1FC
    unsigned int   ReconVDataOffset;            // +0x200
    int            LastMode;                    // +0x204 mode of the previous macroblock
    unsigned int   MacroBlocks;                 // +0x208 including the two-macroblock border
    unsigned int   MBRows;                      // +0x20C
    unsigned int   MBCols;                      // +0x210
    unsigned char* ThisFrameReconAlloc;         // +0x214
    unsigned char* GoldenFrameAlloc;            // +0x218
    unsigned char* LastFrameReconAlloc;         // +0x21C
    unsigned char* ThisFrameRecon;              // +0x220
    unsigned char* GoldenFrame;                 // +0x224
    unsigned char* LastFrameRecon;              // +0x228
    unsigned char* SpareFrame;                  // +0x22C buffer released by the golden-frame swap
    unsigned char  Unused230[0x238 - 0x230];
    short*         ReconDataBuffer;             // +0x238 inverse-transformed residual
    void*          TmpDataBuffer;               // +0x23C
    short*         PredictionBuffer;            // +0x240 motion-compensated prediction
    unsigned char  Unused244[0x248 - 0x244];
    unsigned char  ScanOrder[64];               // +0x248 scan order frozen for the Huffman tokens
    unsigned char  ModifiedScanOrder[64];       // +0x288
    unsigned char  EobOffsetTable[64];          // +0x2C8
    unsigned char  ScanBands[64];               // +0x308 coefficient band of each raster position
    int            mvNearOffset[12];            // +0x348 macroblock-index offsets of the neighbours
    unsigned char  Unused378[0x388 - 0x378];
    int            MultiStream;                 // +0x388
    int            RefreshGoldenFrame;          // +0x38C
    unsigned int   ProbInterlaced;              // +0x390
    unsigned char  ZeroRunProbs[2][14];         // +0x394
    signed char*   MbModes;                     // +0x3B0 coding mode of every macroblock
    void*          MbModesAlloc;                // +0x3B4
    MOTION_VECTOR* MbMotionVectors;             // +0x3B8 vector of every macroblock
    void*          MbMotionVectorsAlloc;        // +0x3BC
    unsigned char  Unused3C0[0x3C8 - 0x3C0];
    unsigned int   DcHuffCode[2][12];           // +0x3C8
    HUFF_NODE      DcHuffTree[2][12];           // +0x428
    unsigned int   DcHuffProbs[2][12];          // +0x548
    unsigned char  DcHuffLength[2][12];         // +0x5A8
    unsigned int   AcHuffCode[3][2][6][12];     // +0x5C0
    HUFF_NODE      AcHuffTree[3][2][6][12];     // +0xC80
    unsigned int   AcHuffProbs[3][2][6][12];    // +0x20C0
    unsigned char  AcHuffLength[3][2][6][12];   // +0x2780
    unsigned int   ZeroHuffCode[2][14];         // +0x2930
    HUFF_NODE      ZeroHuffTree[2][14];         // +0x29A0
    unsigned int   ZeroHuffProbs[2][14];        // +0x2AF0
    unsigned char  ZeroHuffLength[2][14];       // +0x2B60
    int            UseHuffman;                  // +0x2B7C
    HUFF_LUT_ENTRY DcHuffLUT[2][64];            // +0x2B80
    HUFF_LUT_ENTRY AcHuffLUT[3][2][6][64];      // +0x2C80
    HUFF_LUT_ENTRY ZeroHuffLUT[2][64];          // +0x3E80
    TOKEN_LUT_ENTRY DcTokenLUT[2][256];         // +0x3F80
    int            CurrentDcRunLen[2];          // +0x4780 pending zero-DC run, per plane
    int            CurrentAc1RunLen[2];         // +0x4788 pending zero-first-AC run, per plane
    unsigned char  Unused4790[0x47B0 - 0x4790];
    void*          MbFlagsAlloc;                // +0x47B0 allocated with the code arrays only, never read
    unsigned char* MbFlags;                     // +0x47B4
    unsigned char  Unused47B8[0x47C8 - 0x47B8];
    unsigned char  MvSignProbs[2];              // +0x47C8
    unsigned char  IsMvShortProb[2];            // +0x47CA
    unsigned char  MvShortProbs[2][7];          // +0x47CC
    unsigned char  MvSizeProbs[2][8];           // +0x47DA
    unsigned char  ModeProbs[4][2][10];         // +0x47EA [context][0 mode, 1 same-as-last][mode]
    unsigned char  ProbModeSame[3][10];         // +0x483A [context][last mode]
    unsigned char  Unused4858[0x4862 - 0x4858];
    unsigned char  ProbMode[3][10][9];          // +0x4862 [context][last mode][tree node]
    unsigned char  Unused4970[0x49CA - 0x4970];
    unsigned char  DcProbs[2][11];              // +0x49CA
    unsigned char  AcProbs[2][3][6][11];        // +0x49E0 [plane][preceding context][band][node]
    unsigned char  DcNodeContexts[2][3][5];     // +0x4B6C [plane][context][node]
    unsigned char  FrameType;                   // +0x4B8A 0 for a key frame
    int            UseJobs;                     // +0x4B8C
    EA::Jobs::JobScheduler* JobScheduler;       // +0x4B90
    int            JobAffinity;                 // +0x4B94
    int            CurrentJob;                  // +0x4B98 job pair the next frame is queued on
    int            CreateCodeArrays;            // +0x4B9C
    EA::Jobs::Job* Jobs;                        // +0x4BA0 two { decode, completion } pairs
    VP6_CALLBACK_JOB_DATA* CallbackJobData;     // +0x4BA4 one per pair
    VP6_DECODE_JOB_DATA*   DecodeJobData;       // +0x4BA8 one per pair
    EA::Thread::Semaphore* JobSlotSemaphore;    // +0x4BAC posted once a completion job has the picture
    EA::Thread::Semaphore* FrameReleasedSemaphore; // +0x4BB0 posted once a completion callback returned
    void*          JobSlotSemaphoreMemory;      // +0x4BB4
    void*          FrameReleasedSemaphoreMemory; // +0x4BB8
    void*          FilterTmpBuffer;             // +0x4BBC 512-byte scratch of the vector filters and transforms
};

// ---- the library allocator -----------------------------------------------------------------

namespace vp6
{
    void  SetAllocator(EA::Allocator::ICoreAllocator* lpAllocator);
    void* Alloc(unsigned int luSize);
    void  Free(void* lpBlock);
}

// ---- shared entry points -------------------------------------------------------------------

extern "C"
{
    // Machine-specific kernels, installed by VP6_DMachineSpecificConfig / UtilMachineSpecificConfig.
    extern void (*VP6_BuildQuantIndex)(QUANTIZER* pbi);
    extern void (*FilterBlockBil_8)(unsigned char* ReconPtr1, unsigned char* ReconPtr2,
                                    unsigned char* ReconRefPtr, unsigned int PixelsPerLine,
                                    int ModX, int ModY);
    extern void (*ClearSysState)(void);
    extern void (*SubtractBlock)(unsigned char* SrcPtr, short* DestPtr, unsigned int SrcPixelsPerLine);

    // Allocation.
    void* duck_malloc(unsigned int size, int type);
    void* duck_mallocAlign(unsigned int size, unsigned int align, int type);
    void  duck_free(void* ptr);
    void  duck_freeAlign(void* ptr);

    // Instance lifetime.
    void        VP6_DeleteTmpBuffers(PB_INSTANCE* pbi);
    int         VP6_AllocateTmpBuffers(PB_INSTANCE* pbi);
    void        VP6_DeletePBInstance(xPB_INST* pbi);
    PB_INSTANCE* VP6_CreatePBInstance(void);
    void        VP6_DeleteFragmentInfo(PB_INSTANCE* pbi);
    int         VP6_AllocateFragmentInfo(PB_INSTANCE* pbi);
    void        VP6_DeleteFrameInfo(PB_INSTANCE* pbi);
    int         VP6_AllocateFrameInfo(PB_INSTANCE* pbi, unsigned int FrameSize);
    int         VP6_InitFrameDetails(PB_INSTANCE* pbi);
    void        ChangeFrameInfoConfiguration(FRAME_INFO* FrameInfo, const CONFIG_TYPE* Config);
    FRAME_INFO* CreateFrameInfoInstance(const CONFIG_TYPE* Config);
    void        DeleteFrameInfoInstance(FRAME_INFO** FrameInfo);

    // Dequantisation.
    void       VP6_BuildQuantIndex_Generic(QUANTIZER* pbi);
    void       VP6_init_dequantizer(QUANTIZER* pbi);
    void       VP6_UpdateQ(QUANTIZER* pbi);
    void       VP6_DeleteQuantizer(QUANTIZER** pbi);
    QUANTIZER* VP6_CreateQuantizer(void);

    // Generic block kernels and frame utilities.
    void UnpackBlock_C(unsigned char* ReconPtr, short* ReconRefPtr, unsigned int ReconPixelsPerLine);
    void SubtractBlock_C(unsigned char* SrcPtr, short* DestPtr, unsigned int SrcPixelsPerLine);
    void InitVPUtil(void);
    void UtilMachineSpecificConfig(void);
    void VP6_DMachineSpecificConfig(void);
    void FilterBlock2dBil_FirstPass(unsigned char* SrcPtr, int* OutputPtr, unsigned int SrcPixelsPerLine,
                                    unsigned int PixelStep, unsigned int OutputHeight,
                                    unsigned int OutputWidth, const int* VpFilter);
    void FilterBlock1dBil_8(unsigned char* SrcPtr, unsigned char* OutputPtr, unsigned int SrcPixelsPerLine,
                            unsigned int PixelStep, unsigned int OutputHeight,
                            unsigned int OutputWidth, const int* VpFilter);
    void FilterBlock2dBil_SecondPass_8(int* SrcPtr, unsigned char* OutputPtr, unsigned int OutputPitch,
                                       unsigned int PixelStep, unsigned int OutputHeight,
                                       unsigned int OutputWidth, const int* VpFilter);
    void FilterBlock2dBil_8(unsigned char* SrcPtr, unsigned char* OutputPtr, unsigned int SrcPixelsPerLine,
                            const int* HFilter, const int* VFilter);
    void FilterBlockBil_8_C(unsigned char* ReconPtr1, unsigned char* ReconPtr2, unsigned char* ReconRefPtr,
                            unsigned int PixelsPerLine, int ModX, int ModY);
    void UpdateUMVBorder(FRAME_INFO* FrameInfo, unsigned char* DestReconPtr);

    // Huffman trees.
    void BoolTreeToHuffCodes(const unsigned char* BoolTreeProbs, unsigned int* HuffProbs);
    void ZerosBoolTreeToHuffCodes(const unsigned char* BoolTreeProbs, unsigned int* HuffProbs);
    void ConvertBoolTrees(PB_INSTANCE* pbi);
    void VP6_BuildHuffTree(HUFF_NODE* HuffTreeRoot, unsigned int* Counts, int Values);
    void VP6_BuildHuffLookupTable(const HUFF_NODE* HuffTreeRoot, HUFF_LUT_ENTRY* HuffTable);
    void VP6_BuildDCUnpackLookupTable(const HUFF_NODE* HuffTreeRoot, TOKEN_LUT_ENTRY* DcTable);
    void VP6_CreateCodeArray(const HUFF_NODE* HuffRoot, int HuffTreeIndex, unsigned int* HuffCodeArray,
                             unsigned char* HuffCodeLengthArray, unsigned int CodeValue,
                             int CodeLength);

    // Entropy configuration.
    void BuildScanOrder(PB_INSTANCE* pbi, const unsigned char* ScanBands);
    void VP6_ConfigureEntropyDecoder(PB_INSTANCE* pbi, unsigned char FrameType);
    void VP6_ConfigureContexts(PB_INSTANCE* pbi);
    void VP6_ResetLeftContext(PB_INSTANCE* pbi);
    void VP6_ResetAboveContext(PB_INSTANCE* pbi);
    void VP6_ConfigureMvEntropyDecoder(PB_INSTANCE* pbi, unsigned char FrameType);
    void VP6_BuildModeTree(PB_INSTANCE* pbi);
    int  VP6_decodeModeDiff(PB_INSTANCE* pbi);
    void VP6_DecodeModeProbs(PB_INSTANCE* pbi);

    // Bitstream.
    void         VP6_StartDecode(BOOL_CODER* br, const unsigned char* source);
    void         InitHeaderBuffer(HEADER_BUFFER* Header, const unsigned char* Buffer);
    unsigned int ReadHeaderBits(HEADER_BUFFER* Header, unsigned int BitsToRead);
    int          VP6_LoadFrame(PB_INSTANCE* pbi);
    int          VP6_DecodeBool(BOOL_CODER* br, int probability);
    int          VP6_DecodeBool128(BOOL_CODER* br);
    int          nDecodeBool(BOOL_CODER* br, int probability);

    // Macroblocks and blocks.
    void          VP6_DecodeFrameMbs(PB_INSTANCE* pbi);
    void          VP6_DecodeMacroBlock(PB_INSTANCE* pbi, unsigned int MBrow, unsigned int MBcol);
    int           VP6_DecodeMode(PB_INSTANCE* pbi, int LastMode, int Context);
    int           VP6_DecodeBlockMode(PB_INSTANCE* pbi);
    void          VP6_decodeMotionVector(PB_INSTANCE* pbi, MOTION_VECTOR* mv, int Mode);
    void          VP6_FindNearestandNextNearest(PB_INSTANCE* pbi, unsigned int MBrow, unsigned int MBcol,
                                                unsigned char Frame, int* Type);
    void          VP6_PredictDC(PB_INSTANCE* pbi, unsigned int bp, short* LastDc,
                                BLOCK_CONTEXT* Above, BLOCK_CONTEXT* Left);
    unsigned char VP6_ReadTokensPredictA(PB_INSTANCE* pbi, short* Coeffs, unsigned int Plane,
                                         unsigned char* AboveToken, unsigned char* LeftToken);
    void          VP6_DecodeBlock(PB_INSTANCE* pbi, unsigned int MBrow, unsigned int MBcol, unsigned int bp);

    // Vector reconstruction kernels.
    void ScalarReconInter_Xenon(unsigned char* ReconPtr, const unsigned char* RefPtr,
                                const short* ChangePtr, unsigned int LineStep);
    void ScalarReconIntra_Xenon(unsigned char* ReconPtr, const short* ChangePtr, unsigned int LineStep);
    void ReconBlock_Xenon(const short* SrcPtr, const short* ChangePtr, unsigned char* DestPtr,
                          unsigned int LineStep);
    void FilterBlock2dBil_FirstPass_Xenon(const unsigned char* SrcPtr, float* OutputPtr,
                                          unsigned int SrcPixelsPerLine, unsigned int PixelStep,
                                          unsigned int OutputHeight, unsigned int OutputWidth,
                                          const float* VpFilter);
    void FilterBlock1dBilV_Xenon(const unsigned char* SrcPtr, short* OutputPtr,
                                 unsigned int SrcPixelsPerLine, unsigned int PixelStep,
                                 unsigned int OutputHeight, unsigned int OutputWidth,
                                 const float* VpFilter);
    void FilterBlock_Xenon(const unsigned char* ReconPtr1, const unsigned char* ReconPtr2,
                           short* ReconRefPtr, unsigned int PixelsPerLine, int ModX, int ModY,
                           int UseBicubic, float* TmpBuffer);

    // Inverse transforms of the vector build (vp6_idct_xenon.cpp): dequantise, transform into the
    // 8x8 residual and clear the coefficients that were used.
    void IDct1_Xenon(short* Coeffs, const short* Dequant, short* ReconData);
    void IDct10_Xenon(short* Coeffs, const float* FloatDequant, short* ReconData, unsigned char* lpTmp);
    void IDctSlow_Xenon(short* Coeffs, const float* FloatDequant, short* ReconData, unsigned char* lpTmp);

    // Constant tables (vp6_tables.cpp).
    extern const int  VP6_NearMacroBlockPositions[12][2];
    extern const int  VP6_QThreshTable[64];
    extern const int  VP6_UvQThreshTable[64];
    extern const short VP6_DcQuant[64];
    extern const short VP6_UvDcQuant[64];
    extern const int  VP6_ZigZag[64];
    extern const unsigned char VP6_BlockToPlane[6];
    extern const int  VP6_CoeffToBand[64];
    extern const int  VP6_DctRangeMinVals[12];
    extern const int  VP6_CoeffToHuffBand[64];
    extern const unsigned char VP6_DefaultScanBands[64];
    extern const unsigned char VP6_DefaultInterlacedScanBands[64];
    extern const int  VP6_ModeUsesMC[10];
    extern const int  VP6_Mode2Frame[10];
    extern const unsigned int VP6_LoMaskTbl[33];
    extern const unsigned char VP6_DcUpdateProbs[2][11];
    extern const unsigned char VP6_ScanBandUpdateProbs[64];
    extern const unsigned char VP6_ZeroRunUpdateProbs[2][14];
    extern const unsigned char VP6_DefaultZeroRunProbs[2][14];
    extern const unsigned char VP6_AcUpdateProbs[3][2][6][11];
    extern const int  VP6_DcNodeEqs[5][3][2];
    extern const unsigned char VP6_MvUpdateProbs[2][17];
    extern const unsigned char VP6_DefaultMvShortProbs[2][7];
    extern const unsigned char VP6_DefaultMvLongProbs[2][8];
    extern const unsigned char VP6_DefaultIsShortProbs[2];
    extern const unsigned char VP6_DefaultSignProbs[2];
    extern const unsigned char VP6_ModeVq[3][16][20];
    extern const unsigned char VP6_DefaultModeProbs[4][2][10];
    extern const VP6_TOKENEXTRABITS VP6_DctExtraBits[12];
    extern const int  VP6_BilinearFilters[8][2];
    extern const float VP6_BilinearFiltersXenon[8][4];
    extern const unsigned char VP6_XenonConstants[54][16];
}

// PowerPC shifts: a shift count of 32..63 yields 0 (C++ leaves it undefined).
inline unsigned int Vp6Srw(unsigned int luValue, unsigned int luShift)
{
    return (luShift & 0x20u) ? 0u : (luValue >> (luShift & 0x1Fu));
}

inline unsigned int Vp6Slw(unsigned int luValue, unsigned int luShift)
{
    return (luShift & 0x20u) ? 0u : (luValue << (luShift & 0x1Fu));
}

// Big-endian 32-bit load from the bitstream.
inline unsigned int Vp6ReadBigEndian32(const unsigned char* lpBytes)
{
    return (static_cast<unsigned int>(lpBytes[0]) << 24) | (static_cast<unsigned int>(lpBytes[1]) << 16) |
           (static_cast<unsigned int>(lpBytes[2]) << 8) | static_cast<unsigned int>(lpBytes[3]);
}

#endif // VP6_DECODER_H
