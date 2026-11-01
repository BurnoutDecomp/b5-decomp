#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/CapsuleVolume.hpp"
#include "vendor/renderware/collision/CylinderVolume.hpp"
#include "vendor/renderware/collision/TriangleVolume.hpp"
#include "vendor/renderware/collision/AggregateVolume.hpp"

namespace rw { namespace collision {

// ARTIST 82F91740..82F919F4: each record stores the real primitive method
// addresses. Box and Triangle pass direction/segment vectors in VMX registers;
// the SDK base slot types retain the generic member-pointer declaration.
extern const Volume::VTable gVolumeHandler_82F91740 = {
    VOLUMETYPESPHERE,
    static_cast<Volume::GetBBoxFn>(&SphereVolume::GetBBox),
    static_cast<Volume::GetBBoxDiagFn>(&SphereVolume::GetBBoxDiag),
    static_cast<Volume::GetIntervalFn>(&SphereVolume::GetInterval),
    static_cast<Volume::GetMaximumFeatureFn>(&SphereVolume::GetMaximumFeature),
    static_cast<Volume::CreateGPInstanceFn>(&SphereVolume::CreateGPInstance),
    static_cast<Volume::LineSegIntersectFn>(&SphereVolume::LineSegIntersect),
    static_cast<Volume::ReleaseFn>(&SphereVolume::Release), "SphereVolume", 0
};
extern const Volume::VTable gVolumeHandler_82F918C0 = {
    VOLUMETYPECAPSULE,
    static_cast<Volume::GetBBoxFn>(&CapsuleVolume::GetBBox),
    static_cast<Volume::GetBBoxDiagFn>(&CapsuleVolume::GetBBoxDiag),
    static_cast<Volume::GetIntervalFn>(&CapsuleVolume::GetInterval),
    static_cast<Volume::GetMaximumFeatureFn>(&CapsuleVolume::GetMaximumFeature),
    static_cast<Volume::CreateGPInstanceFn>(&CapsuleVolume::CreateGPInstance),
    static_cast<Volume::LineSegIntersectFn>(&CapsuleVolume::LineSegIntersect),
    static_cast<Volume::ReleaseFn>(&CapsuleVolume::Release), "CapsuleVolume", 0
};
extern const Volume::VTable gVolumeHandler_82F919A4 = {
    VOLUMETYPETRIANGLE,
    static_cast<Volume::GetBBoxFn>(&TriangleVolume::GetBBox),
    static_cast<Volume::GetBBoxDiagFn>(&TriangleVolume::GetBBoxDiag),
    reinterpret_cast<Volume::GetIntervalFn>(&TriangleVolume::GetInterval),
    reinterpret_cast<Volume::GetMaximumFeatureFn>(&TriangleVolume::GetMaximumFeature),
    static_cast<Volume::CreateGPInstanceFn>(&TriangleVolume::CreateGPInstance),
    reinterpret_cast<Volume::LineSegIntersectFn>(&TriangleVolume::LineSegIntersect),
    static_cast<Volume::ReleaseFn>(&TriangleVolume::Release), "TriangleVolume", 0
};
extern const Volume::VTable gVolumeHandler_82F9176C = {
    VOLUMETYPEBOX,
    static_cast<Volume::GetBBoxFn>(&BoxVolume::GetBBox),
    static_cast<Volume::GetBBoxDiagFn>(&BoxVolume::GetBBoxDiag),
    reinterpret_cast<Volume::GetIntervalFn>(&BoxVolume::GetInterval),
    reinterpret_cast<Volume::GetMaximumFeatureFn>(&BoxVolume::GetMaximumFeature),
    static_cast<Volume::CreateGPInstanceFn>(&BoxVolume::CreateGPInstance),
    static_cast<Volume::LineSegIntersectFn>(&BoxVolume::LineSegIntersect),
    static_cast<Volume::ReleaseFn>(&BoxVolume::Release), "BoxVolume", 0
};
extern const Volume::VTable gVolumeHandler_82F91894 = {
    VOLUMETYPECYLINDER,
    static_cast<Volume::GetBBoxFn>(&CylinderVolume::GetBBox),
    static_cast<Volume::GetBBoxDiagFn>(&CylinderVolume::GetBBoxDiag),
    static_cast<Volume::GetIntervalFn>(&CylinderVolume::GetInterval),
    static_cast<Volume::GetMaximumFeatureFn>(&CylinderVolume::GetMaximumFeature),
    static_cast<Volume::CreateGPInstanceFn>(&CylinderVolume::CreateGPInstance),
    static_cast<Volume::LineSegIntersectFn>(&CylinderVolume::LineSegIntersect),
    static_cast<Volume::ReleaseFn>(&CylinderVolume::Release), "CylinderVolume", 0
};
extern const Volume::VTable gVolumeHandler_82F919D0 = {
    VOLUMETYPEAGGREGATE,
    static_cast<Volume::GetBBoxFn>(&AggregateVolume::GetBBox),
    static_cast<Volume::GetBBoxDiagFn>(&AggregateVolume::GetBBoxDiag),
    0,
    static_cast<Volume::GetMaximumFeatureFn>(&AggregateVolume::GetMaximumFeature),
    static_cast<Volume::CreateGPInstanceFn>(&AggregateVolume::CreateGPInstance),
    0,
    static_cast<Volume::ReleaseFn>(&AggregateVolume::Release), "AggregateVolume", 0
};

} }
