#include "types.hpp"
#include "BrnCommonTypes.h"
#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
namespace CgsGraphics { class DispatchFrame; }
namespace BrnWorld { struct ShadowMap; }
namespace BrnVehicle { class GraphicsSpec; }
namespace BrnWheel { class GraphicsSpec; }
struct RenderProbe
{
    mutable __vector4 distance, fog1, fog2;
    mutable const void* pointers[5];
    mutable int lists[3];
    mutable bool draw;
    __declspec(noinline) void RenderRaceCar(CgsGraphics::DispatchFrame*, rw::math::vpu::VecFloat, Vector4, Vector4,
        const BrnWorld::ActiveRaceCar::RenderParams*, const CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>&,
        const CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>&, int, int, int, BrnWorld::ShadowMap*, bool) const;
};
void RenderProbe::RenderRaceCar(CgsGraphics::DispatchFrame* frame, rw::math::vpu::VecFloat d, Vector4 f1, Vector4 f2,
    const BrnWorld::ActiveRaceCar::RenderParams* rp, const CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>& gfx,
    const CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>& wheel, int a, int b, int c, BrnWorld::ShadowMap* shadow, bool visible) const
{
    distance=d.mV; fog1=f1.mV; fog2=f2.mV;
    pointers[0]=frame; pointers[1]=rp; pointers[2]=&gfx; pointers[3]=&wheel; pointers[4]=shadow;
    lists[0]=a; lists[1]=b; lists[2]=c; draw=visible;
}
struct RenderInputs
{
    rw::math::vpu::VecFloat distance;
    Vector4 fog1, fog2;
    CgsGraphics::DispatchFrame* frame;
    const BrnWorld::ActiveRaceCar::RenderParams* rp;
    const CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>* gfx;
    const CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>* wheel;
    int a,b,c;
    BrnWorld::ShadowMap* shadow;
    bool visible;
};
extern "C" __declspec(noinline) void RenderProbeCaller(RenderProbe* p, const RenderInputs* i)
{
    p->RenderRaceCar(i->frame, i->distance, i->fog1, i->fog2, i->rp, *i->gfx, *i->wheel,
                     i->a, i->b, i->c, i->shadow, i->visible);
}
