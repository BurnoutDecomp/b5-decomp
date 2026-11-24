#pragma once

#include "types.hpp"

// ARTIST 0x82FAB638 and DecFIGS LionPerfMon.h. Construct registers the
// handles before the particle runtime reads them.
struct LionPerfMon
{
    void Construct();

    s32 miLionRender;                   // +0x00  0x82FAB638  "Render"
    s32 miParticleEmitterBlend;         // +0x04  0x82FAB63C  "EmitterBlend"
    s32 miParticleEmitterUpdate;        // +0x08  0x82FAB640  "EmitterUpdate"
    s32 miParticleEmitterGenerate;      // +0x0C  0x82FAB644  "EmitterGenerate"
    s32 miParticleEmitterParticleBuild; // +0x10  0x82FAB648  "EmitterParticleBuild"
    s32 miSimulateMatrixParticles;      // +0x14  0x82FAB64C  "SimulateMatrixParticles"
    s32 miSimulateVectorParticles;      // +0x18  0x82FAB650  "SimulateVectorParticles"
    s32 miSimulateLocalParticles;       // +0x1C  0x82FAB654  "SimulateLocalParticles"
    s32 miEmitterRender;                // +0x20  0x82FAB658  "EmitterRender"
    s32 miEmitterCubeRender;            // +0x24  0x82FAB65C  "EmitterCubeRender"
    s32 miBaseColourVarianceBehaviour;  // +0x28  0x82FAB660  "BaseColourVarianceBehaviour"
    s32 miColourStepsBehaviour;         // +0x2C  0x82FAB664  "ColourStepsBehaviour"
    s32 miWaveRgbBehaviour;             // +0x30  0x82FAB668  "WaveRgbBehaviour"
    s32 miWaveBehaviour;                // +0x34  0x82FAB66C  "WaveBehaviour"
    s32 miAlphaFadeBehaviour;           // +0x38  0x82FAB670  "AlphaFadeBehaviour"
    s32 miRotationBehaviour;            // +0x3C  0x82FAB674  "RotationBehaviour"
    s32 miSizeBehaviour;                // +0x40  0x82FAB678  "SizeBehaviour"
    s32 miDragBehaviour;                // +0x44  0x82FAB67C  "DragBehaviour"
    s32 miMultiFrameBehaviour;          // +0x48  0x82FAB680  "MultiFrameBehaviour"
    s32 miParticleBehaviourRADIAL;      // +0x4C  0x82FAB684  "ParticleBehaviourRADIAL"
    s32 miParticleBehaviourOFFSETROT;   // +0x50  0x82FAB688  "ParticleBehaviourOFFSETROT"
    s32 miRandomSeedBuild;              // +0x54  0x82FAB68C  "RandomSeed::Build"
    s32 miRandomSeedUpdate;             // +0x58  0x82FAB690  "    RandomSeed::Update"
};

extern LionPerfMon gLionPerfMon;
