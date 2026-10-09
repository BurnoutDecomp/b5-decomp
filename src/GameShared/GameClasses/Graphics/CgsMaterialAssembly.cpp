// CgsGraphics::MaterialAssembly accessors.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte-match):
//   CgsGraphics::MaterialAssembly::GetMaterial @ 0x827E6720
//
// Layout from the DecFIGS DWARF (CgsMaterialAssembly.h:54).

#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsMaterialAnimation.h"
#include <cstdlib>
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // the one-shot boot-gate log

namespace CgsGraphics
{
    // ARTIST 827FBA00..BB90: absent constants default to zero; duration zero
    // leaves a material static. Otherwise select the original CPU shader, mark
    // every technique animated, and attach it to the serialized CPU block.
    void MaterialAssembly::FixupAnimatedMaterial()
    {
        if (!mpCPUShaderConstants.Get()) return;

        Vector4 lValue;
        const f32 lfDuration = mpCPUShaderConstants->GetValue("AnimDuration", lValue) ? lValue.x : 0.0f;
        const f32 lfStepsU = mpCPUShaderConstants->GetValue("AnimNumberOfFramesU", lValue) ? lValue.x : 0.0f;
        const f32 lfStepsV = mpCPUShaderConstants->GetValue("AnimNumberOfFramesV", lValue) ? lValue.x : 0.0f;
        ICPUShader* lpShader = MaterialAnimationFactory::Instance().Create(lfDuration, lfStepsU, lfStepsV);
        if (!lpShader) return;

        for (u32 luIndex = 0; luIndex < mu8NumMaterials; ++luIndex)
            mappMaterials[luIndex]->mu16StateFlags |= 6u;
        mpCPUShaderConstants->SetCPUShader(lpShader, this);

        // Passive load-time witness; does not change authored animation values.
        const char* lpDiag = std::getenv("BRN_MATERIAL_ANIM_DIAG");
        if (lpDiag && lpDiag[0] && lpDiag[0] != '0')
            *CgsDev::Log::gpDebugPrint << "[material-anim] attach material=" << muNameHash
                << " duration=" << lfDuration << " framesU=" << lfStepsU << " framesV=" << lfStepsV << "\n";
    }
    // GetMaterial @ 0x827E6720. Bounds-checks luIndex against mu8NumMaterials (the asm
    // reads the byte at offset 8) and returns mappMaterials[luIndex]:
    //   r11 = *this (mappMaterials @+0); return *(r11 + 4*luIndex).
    MaterialTechnique* MaterialAssembly::GetMaterial(u32 luIndex) const
    {
        CGS_ASSERT(luIndex < mu8NumMaterials, "Material technique index out of range."); // CgsMaterialAssembly.h:174
        return mappMaterials[luIndex];
    }
}
