// Embed/usage check for the World-AI group's bodied leaf heuristics:
//   - BrnAI::AStar::EuclideanDistance         @0x8276E0D8
//   - BrnAI::AStar::EuclideanDistanceXBiased  @0x8276E100
//   - BrnAI::AStar::EuclideanDistanceYBiased  @0x8276E138
// The three are private members; taking member-function pointers forces the
// inline bodies to instantiate without needing public access.

#include "GameSource/World/AI/Route/BrnAStar.h"

namespace BrnAI
{
// Befriend a probe so we can reference the private heuristics from outside.
// The heuristics are static (context-free) members called through a plain function
// pointer (mpDistanceFunction / KAP_DISTANCE_FUNCTIONS), so these are free function
// pointers, not member-function pointers.
struct AStarEmbedProbe
{
    static void Touch()
    {
        typedef f32 (*DistFn)(const AStarVector2&, const AStarVector2&);
        // Casting through a local volatile prevents the optimizer from
        // discarding the references before the bodies are emitted.
        DistFn lpfns[3];
        lpfns[0] = &AStar::EuclideanDistance;
        lpfns[1] = &AStar::EuclideanDistanceXBiased;
        lpfns[2] = &AStar::EuclideanDistanceYBiased;
        for (int i = 0; i < 3; ++i)
        {
            volatile DistFn lpfn = lpfns[i];
            (void)lpfn;
        }
    }
};
}

// Layout sanity: the node-position vector is the rwmath fpu 2-component float.
static_assert(sizeof(rw::math::fpu::Vector2Template<float>) == 8,
              "AStarVector2 must be two packed f32");

namespace { void EmbedCheck() { BrnAI::AStarEmbedProbe::Touch(); } }
