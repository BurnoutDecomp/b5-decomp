#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h"
typedef BrnDirector::Camera::BehaviourSpirallingDeathcam D;
typedef char data_size[(sizeof(D::VectorData)==32)?1:-1];
typedef char data_align[(__alignof(D::VectorData)==16)?1:-1];
typedef char parameters_size[(sizeof(D::Parameters)==0xB0)?1:-1];
typedef char behavior_size[(sizeof(D)==0x300)?1:-1];
__declspec(noinline) void set_angle(D::VectorData* p, float v) { p->YRotationDegs() = v; }
__declspec(noinline) void set_speed(D::VectorData* p, float v) { p->YRotationSpeedDegsPS() = v; }
__declspec(noinline) void set_height(D::VectorData* p, float v) { p->Height() = v; }
__declspec(noinline) void set_growth(D::VectorData* p, float v) { p->HeightIncreaseSpeed() = v; }
__declspec(noinline) void set_radius(D::VectorData* p, float v) { p->Radius() = v; }
__declspec(noinline) void set_radial(D::VectorData* p, float v) { p->RadiusIncreaseSpeed() = v; }
__declspec(noinline) void set_offset(D::VectorData* p, float v) { p->LookOffsetAmount() = v; }
__declspec(noinline) float get_angle(const D::VectorData* p) { return p->YRotationDegs(); }
__declspec(noinline) float get_speed(const D::VectorData* p) { return p->YRotationSpeedDegsPS(); }
__declspec(noinline) float get_height(const D::VectorData* p) { return p->Height(); }
__declspec(noinline) float get_growth(const D::VectorData* p) { return p->HeightIncreaseSpeed(); }
__declspec(noinline) float get_radius(const D::VectorData* p) { return p->Radius(); }
__declspec(noinline) float get_radial(const D::VectorData* p) { return p->RadiusIncreaseSpeed(); }
__declspec(noinline) float get_offset(const D::VectorData* p) { return p->LookOffsetAmount(); }
