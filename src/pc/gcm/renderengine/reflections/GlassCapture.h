#pragma once

#include "pc/gcm/renderengine/reflections/SceneSettings.h"
#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsMaterialTechnique.h"
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include <new>

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: split authored transparent vehicle surfaces (glass
    // and lamp lenses) from solid parts, without changing streamed resources or
    // dispatch packet layouts. Copies live in the owning command bin until join.
    inline const Renderable* FilterVehicleMeshes(const Renderable* lpSource,
        CgsGraphics::DispatchFrame& lrFrame, u8 luTechnique, bool lbTransparent)
    {
        if (!lpSource) return nullptr;
        auto matches = [=](const RenderableMesh* lpMesh) {
            const auto* lpAssembly = lpMesh->mpMaterialAssembly;
            if (!lpAssembly || !lpAssembly->GetLength() || !lpMesh->mu8NumVertexDescriptors) return false;
            const u32 luIndex = (std::min)(static_cast<u32>(luTechnique),
                (std::min)(static_cast<u32>(lpMesh->mu8NumVertexDescriptors), static_cast<u32>(lpAssembly->GetLength())) - 1u);
            const auto* lpMaterial = lpAssembly->GetMaterial(luIndex);
            return lpMaterial && ((lpMaterial->mu16StateFlags & 1u) != 0) == lbTransparent;
        };
        u32 luCount = 0;
        for (u32 lu = 0; lu < lpSource->mu16NumMeshes; ++lu) if (matches(lpSource->mppMeshes[lu])) ++luCount;
        if (!luCount) return nullptr;
        auto& lrBin = lrFrame.GetBin();
        const u32 luQwords = static_cast<u32>((sizeof(Renderable) + luCount * sizeof(RenderableMesh*) + 15u) / 16u);
        if (lrBin.GetUsedQwords() + luQwords + 256u >= lrBin.GetSizeQwords()) lrBin.HandleMemoryOverflow(luQwords + 256u);
        auto* lpCopy = new (lrBin.AllocateMemoryFast(luQwords)) Renderable(*lpSource);
        lpCopy->mppMeshes = reinterpret_cast<RenderableMesh**>(lpCopy + 1);
        lpCopy->mu16NumMeshes = static_cast<u16>(luCount);
        u32 luOut = 0;
        for (u32 lu = 0; lu < lpSource->mu16NumMeshes; ++lu)
            if (matches(lpSource->mppMeshes[lu])) lpCopy->mppMeshes[luOut++] = lpSource->mppMeshes[lu];
        return lpCopy;
    }

    inline void SubmitVehiclePart(const CgsGraphics::Model* lpModel, s32 liBodyLod,
        f32 lfDistanceSquared, CgsGraphics::DispatchFrame& lrFrame, s32 liObjectList,
        s32 liOpaqueList, u8 luTechnique, u8 luExcludeBits)
    {
        auto submit = [&](const Renderable* lpRenderable, s32 liMeshList) {
            if (!lpRenderable) return;
            auto* lpList = lrFrame.GetList(liObjectList);
            lrFrame.GetBin().BeginPacket();
            CgsGraphics::DrawRenderable::AddToBin(lpRenderable, &lrFrame, true,
                static_cast<s8>(liMeshList), static_cast<s8>(liMeshList), 1, luTechnique,
                false, 0xffu, 0, 0, luExcludeBits);
            lpList->Submit(0, lrFrame.GetBin().EndPacket());
        };
        submit(FilterVehicleMeshes(lpModel->GetRenderable(static_cast<CgsGraphics::Model::State>(liBodyLod)),
            lrFrame, luTechnique, false), liOpaqueList);
        if (Glass().IsVisible(lfDistanceSquared, sfVehicleDrawDistance))
        {
            const s32 liGlassLod = SelectVehicleLod(Glass(), lpModel, lfDistanceSquared);
            if (liGlassLod >= 0)
                submit(FilterVehicleMeshes(lpModel->GetRenderable(static_cast<CgsGraphics::Model::State>(liGlassLod)),
                    lrFrame, luTechnique, true), GlassMeshList());
        }
    }
}
