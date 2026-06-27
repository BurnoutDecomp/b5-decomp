#pragma once

// ===========================================================================
//  NFSMixRecords.hpp -- the serialised / runtime mixer record structs of the EA
//  "NFS mix" system, pointed at by NFSMixMap / NFSMixMapState. These are the
//  on-disk MixMap data format (headers + params) and the runtime proc/shared/unique
//  records the map allocates and walks.
//
//  Layouts from ProStreet08Milestone.pdb (POD records). Being the serialised MixMap
//  data format, they are fixed across EA titles (low cross-build divergence). The
//  "proc" records are pairs of {shared*, unique*} pointers (8 B on X360 = 2x4); the
//  NFSMixMapState GetXxxProc accessors index arrays of them, confirming this shape.
//  x64 widths (real pointers); the X360 sizeof is noted per struct.
// ===========================================================================

#include "types.hpp"

// ---- envelope stage (stEvtMixCtlUniqueData.eCurrentStage) ----
enum eEnvelopeStage
{
    eEnvelopeStage_Off     = 0,
    eEnvelopeStage_Attack  = 1,
    eEnvelopeStage_Decay   = 2,
    eEnvelopeStage_Sustain = 3,
    eEnvelopeStage_Release = 4,
};

// ---- forward decls (proc records reference shared/unique; some cross-reference) ----
struct stMixCtlSharedData;   struct stMixCtlUniqueData;
struct stEvtMixCtlSharedData; struct stEvtMixCtlUniqueData;
struct st3DMixCtlSharedData;  struct st3DMixCtlUniqueData;
struct stMixChSharedData;     struct stMixChUniqueData;
struct stMasterMixChSharedData; struct stMasterMixChUniqueData;
struct st3DMixCtlProc;        struct st3DStateParams;

// ---- map-params (the static, serialised per-control parameters) ----
struct stMixShapeDist        { int ndx; int ndy; };                                   // X360 sizeof 8
struct stMixCtlParams        { int nINPUTID; int nUScaleCntSwing; };                  // 8
struct stSubMixChParams      { int MIXCHID; int UpperLowerSwing; };                   // 8
struct stMasterMixChParams   { int MIXCHID; int MixData; int SFXOBJID; };             // 12
struct stMixEvtParams        { int nEVTCTLID; int nUScaleCntSwing; int nTriggerID;
                               int nParam_00; int nParam_01; int nParam_02; };        // 24
struct st3DStateParams       { int n3DSTATEINFOID; int nCURVEID_DOPPLER;
                               int nQ0MinMax; int nQ1MinMax; int nQ2MinMax; int nQ3MinMax; }; // 24
struct st3DMixCtlParams      { int nINPUTID; st3DStateParams StateParams; };          // 28

// ---- mix-control records ----
struct stCurveDataProc       { int nINPUTID; int* pInputParam; int dBOutput; int Q15Output; }; // 16
struct stMixCtlSharedData    { stMixCtlParams* pstMixCtlParms; int MIXCTLOBJID; int nOffset; int nRatio; }; // 16
struct stMixCtlUniqueData    { stCurveDataProc* pstCurveData; int** ppScaleRatios; int CmpdBOut; };         // 12
struct stMixCtlProc          { stMixCtlSharedData* psdata; stMixCtlUniqueData* pudata; };                   // 8

// ---- event mix-control records ----
struct stEvtMixCtlSharedData { stMixEvtParams* pMapParms; int nOffset; int nRatio; }; // 12
struct stEvtMixCtlUniqueData { eEnvelopeStage eCurrentStage; float msStageElapsed; int qStart; float msStart;
                               int* pTriggerPtr; int** ppScaleRatios; int output; int qoutput; }; // 32
struct stEvtMixCtlProc       { stEvtMixCtlSharedData* pData_S; stEvtMixCtlUniqueData* pData_U; }; // 8

// ---- 3D mix-control records ----
struct st3DMixCtlSharedData  { st3DMixCtlParams* pMapParams; st3DStateParams* pCurStateParams;
                               int CurCamState; int PrevCamState; int msSinceCamTrans; }; // 20
struct st3DMixCtlUniqueData  { int nINPUTID; int* pInputs; int azimuth; int dBRolloff; int q15Rolloff;
                               int DopplerCents; float fPrevDist; float fPrevDeltaDist; }; // 32
struct st3DMixCtlProc        { st3DMixCtlSharedData* p3DMixCtlData_S; st3DMixCtlUniqueData* p3DMixCtlData_U; }; // 8

// ---- sub-mix channel records ----
struct stMixChSharedData     { stSubMixChParams* pMapParams; int MIXCHINID; int NumInputs; }; // 12
struct stMixChUniqueData     { int* pInputs; int Output; };                                   // 8
struct stSubMixChProc        { stMixChSharedData* pMixChData_S; stMixChUniqueData* pMixChData_U; }; // 8

// ---- master-mix channel records ----
struct stMasterMixChSharedData { stMasterMixChParams* pMapParams; int MIXCHINID; int NumInputs; int* pPRESETS; }; // 16
struct stMasterMixChUniqueData { int outputID; int* pInputs; int Output; st3DMixCtlProc** p3DData; int* pOutputs; }; // 20
struct stMasterMixChProc       { stMasterMixChSharedData* pMixChData_S; stMasterMixChUniqueData* pMixChData_U; };    // 8

// ---- serialised section headers (fixed 16/32-byte blob headers) ----
struct stMixMapHeader        { int MixMapID; int NumStates; int StateTableOffset; int DynamicMapOffset; }; // 16
struct stMixMapStateHdr      { int StateIndex; int OffsetMixCtlData; int Offset3DMixCtlData; int OffsetSubMixData;
                               int OffsetMasterMixData; int OffsetPresetData; int OffsetEventCtlData; int Offset_Reserved_07; }; // 32
struct stMixCtlHdr           { int NumMixCtls; int NumNewMixDataProcs; int NumMainMixDataProcs; int NumMainMixCtlOut; }; // 16
struct st3DMixCtlHdr         { int Num3DMixCtls; int NumMainMap3DMixCtls; int Reserved_02; int Reserved_03; }; // 16
struct stMixEventHdr         { int NumEvents; int Reserved_01; int Reserved_02; int Reserved_03; }; // 16
struct stMixChHdr            { int NumMixChannels; int NumUniqueSFXOBJs; int NumMainIn; int NumSecIn; }; // 16
