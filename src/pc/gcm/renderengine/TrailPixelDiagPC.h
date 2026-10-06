#pragma once

#include "GameSource/Effects/Particles/Native/BrnTrailSystem.h"
#include <cmath>
#include <limits>

namespace renderengine {

// FLAG PC-platform leaf: default-off observation of the ORIGINAL trail pass.
// Sampling uses simulation seconds, never a retained dt or a present count.
struct TrailPixelClockPC
{
    u32 attempts = 0;
    f32 last = 0;
    bool Take(f32 now, f32 start)
    {
        if (!std::isfinite(now) || now < start || attempts >= 12
            || (attempts && now - last < 1.0f)) return false;
        last = now; ++attempts; return true;
    }
};

struct TrailPixelBatchPC
{
    BrnParticle::Native::TrailEmitter* const* emitters;
    s32 count;
};

struct TrailPixelRectPC
{
    bool valid = false;
    u32 left = 0, top = 0, right = 0, bottom = 0; // half-open native pixel bounds
};

struct TrailPixelTrackPC
{
    using Segment = BrnParticle::Native::TrailSegmentCollection::TrailSegment;
    bool selected = false;
    s32 type = -1, segment = -1;
    Segment ends[2] = {};
    f32 lastAdded = 0;
    u32 matches = 0, active = 0;

    TrailPixelRectPC Project(Matrix44::InParam matrix, u32 width, u32 height) const
    {
        TrailPixelRectPC result;
        if (!selected || !width || !height) return result;
        f32 minX = (std::numeric_limits<f32>::infinity)(), minY = minX;
        f32 maxX = -minX, maxY = -minY;
        for (s32 end = 0; end < 2; ++end) for (s32 side = -1; side <= 1; side += 2)
        {
            const Vector3 p = ends[end].mPosition.GetVector3();
            const Vector3 t = ends[end].mTangent.GetVector3();
            // Original TrailRenderer::Render extrudes each side by 0.125m.
            const f32 x = p.x + side * 0.125f * t.x;
            const f32 y = p.y + side * 0.125f * t.y;
            const f32 z = p.z + side * 0.125f * t.z;
            const f32 cx = x*matrix.xAxis.x + y*matrix.yAxis.x + z*matrix.zAxis.x + matrix.wAxis.x;
            const f32 cy = x*matrix.xAxis.y + y*matrix.yAxis.y + z*matrix.zAxis.y + matrix.wAxis.y;
            const f32 cz = x*matrix.xAxis.z + y*matrix.yAxis.z + z*matrix.zAxis.z + matrix.wAxis.z;
            const f32 cw = x*matrix.xAxis.w + y*matrix.yAxis.w + z*matrix.zAxis.w + matrix.wAxis.w;
            if (!std::isfinite(cw) || cw <= 0 || cz < 0 || cz > cw) return result;
            const f32 sx = (cx/cw + 1)*0.5f*width, sy = (1 - cy/cw)*0.5f*height;
            if (!std::isfinite(sx) || !std::isfinite(sy)) return result;
            if (sx < minX) minX = sx; if (sx > maxX) maxX = sx;
            if (sy < minY) minY = sy; if (sy > maxY) maxY = sy;
        }
        if (maxX < 0 || maxY < 0 || minX >= width || minY >= height) return result;
        // Include a two-pixel raster edge without changing the actual geometry.
        minX = std::floor(minX)-2; minY = std::floor(minY)-2;
        maxX = std::ceil(maxX)+2; maxY = std::ceil(maxY)+2;
        result.left = minX < 0 ? 0u : static_cast<u32>(minX);
        result.top = minY < 0 ? 0u : static_cast<u32>(minY);
        result.right = maxX > width ? width : static_cast<u32>(maxX);
        result.bottom = maxY > height ? height : static_cast<u32>(maxY);
        result.valid = result.right > result.left && result.bottom > result.top;
        return result;
    }

    void Observe(const TrailPixelBatchPC* batches, Matrix44::InParam matrix,
                 u32 width, u32 height, f32 now)
    {
        matches = 0; active = 0;
        TrailPixelTrackPC candidate;
        u32 bestArea = 0;
        for (s32 kind = 0; kind < BrnParticle::Native::KI_MAX_NUM_TRAIL_TYPES; ++kind)
        {
            active += batches[kind].count;
            for (s32 entry = 0; entry < batches[kind].count; ++entry)
            {
                const auto* emitter = batches[kind].emitters[entry];
                if (!emitter->mpCurrentSegments || emitter->mn8NumSegments < 2) continue;
                for (s32 index = 1; index < emitter->mn8NumSegments; ++index)
                {
                    const auto& a = emitter->mpCurrentSegments->maSegments[index-1];
                    const auto& b = emitter->mpCurrentSegments->maSegments[index];
                    if (selected)
                    {
                        // Array slots/pointers can be reused: match the two fixed world
                        // positions AND original laid times, rather than the slot alone.
                        const Vector3 pa = a.mPosition.GetVector3(), pb = b.mPosition.GetVector3();
                        const Vector3 qa = ends[0].mPosition.GetVector3(), qb = ends[1].mPosition.GetVector3();
                        if (kind == type && pa.x == qa.x && pa.y == qa.y && pa.z == qa.z
                            && pb.x == qb.x && pb.y == qb.y && pb.z == qb.z
                            && a.mTangent.GetPlus() == ends[0].mTangent.GetPlus()
                            && b.mTangent.GetPlus() == ends[1].mTangent.GetPlus())
                        { ++matches; lastAdded = emitter->mrTimeLastSegmentAdded; }
                        continue;
                    }
                    if (now-a.mTangent.GetPlus() >= 7.0f || now-b.mTangent.GetPlus() >= 7.0f
                        || a.mPosition.GetPlus() <= 0 || b.mPosition.GetPlus() <= 0) continue;
                    candidate.selected = true; candidate.type = kind; candidate.segment = index;
                    candidate.ends[0] = a; candidate.ends[1] = b;
                    candidate.lastAdded = emitter->mrTimeLastSegmentAdded;
                    const TrailPixelRectPC rect = candidate.Project(matrix,width,height);
                    const u32 area = rect.valid ? (rect.right-rect.left)*(rect.bottom-rect.top) : 0;
                    if (area > bestArea)
                    {
                        bestArea = area; type = kind; segment = index;
                        ends[0] = a; ends[1] = b; lastAdded = candidate.lastAdded;
                    }
                }
            }
        }
        if (!selected && bestArea) { selected = true; matches = 1; }
    }
};

struct TrailPixelDeltaPC
{
    u32 rgb = 0, alpha = 0, roiRgb = 0;
    u64 roiMagnitude = 0;
    void Add(u32 before, u32 after, u32 x, u32 y, const TrailPixelRectPC& roi)
    {
        const bool changed = ((before^after)&0xffffffu) != 0;
        rgb += changed; alpha += ((before^after)&0xff000000u) != 0;
        if (changed && roi.valid && x >= roi.left && x < roi.right && y >= roi.top && y < roi.bottom)
        {
            ++roiRgb;
            for (u32 channel = 0; channel < 24; channel += 8)
            {
                const s32 difference = s32((before >> channel)&255u) - s32((after >> channel)&255u);
                roiMagnitude += difference < 0 ? -difference : difference;
            }
        }
    }
};

bool TrailPixelDiag_EnabledPC();
bool TrailPixelDiag_BeginPC(f32 now, Matrix44::InParam matrix, const TrailPixelBatchPC* batches);
void TrailPixelDiag_EndPC();
}
