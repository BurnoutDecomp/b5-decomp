// Harness for run_sensor_world_cull.py (crash parity G25-D1): steps (1)+(2) of the shipped
// DeformationSensor::AddContactsToPenetrationSolver are pasted into Sensor::Cull() through
// extracted.inc. Console 0x825E1F84..0x825E2098 / 0x825E23AC..0x825E2408 drops a world contact
// that lies behind another contact's plane before the penetration solver sees it.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include <cmath>
#include <cstdio>

#define CGS_ASSERT(c, m) do { (void)sizeof(c); } while (0)

static const u32 KU_MAX_STORED_CONTACTS = 3;
inline f32 Dot3(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vector3 Sub3(const Vector3& a, const Vector3& b) { return Vector3{ a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; }
inline bool IsValid3(const Vector3& v) { return v.x == v.x && v.y == v.y && v.z == v.z; }

struct StoredContact
{
    Vector3 mLocalPointOnA, mLocalPointOnB, mNormal;
    f32 mfProjectedDist;
    bool mbVehicle;
    bool IsVehicleContact() const { return mbVehicle; }
};

struct Sensor
{
    StoredContact maStoredContacts[KU_MAX_STORED_CONTACTS];
    s32 mi32NumStoredContacts = 0;

    // The extracted block carries the tree's null-solver `return;`, so the body is a void function
    // that reports through out-parameters; Cull() wraps it.
    void CullInto(u16* lauOut, s32* lpiNum) const
    {
        const void* lpSolver = this;
        const void* lpDefObjBase = this;
#include "extracted.inc"
        for (s32 li = 0; li < liNumWorld; ++li) lauOut[li] = lauWorldContacts[li];
        (void)liNumVehicle; (void)lauVehicleContacts;
        *lpiNum = liNumWorld;
    }
    s32 Cull(u16* lauOut) const { s32 n = -1; CullInto(lauOut, &n); return n; }
};

static StoredContact World(Vector3 B, Vector3 n, f32 dist)
{
    StoredContact c{};
    c.mLocalPointOnB = B;
    c.mNormal = n;
    c.mLocalPointOnA = Vector3{ -0.4f * n.x, -0.4f * n.y, -0.4f * n.z, 0.0f };   // defeats the old A-coincidence test
    c.mfProjectedDist = dist;
    c.mbVehicle = false;
    return c;
}

int main()
{
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };
    u16 lau[3];
    const StoredContact wallX = World({ 0.0f, 0.0f, 0.15f, 0 }, { -1, 0, 0, 0 }, -0.65f);
    const StoredContact wallZ = World({ 0.25f, 0.0f, 0.0f, 0 }, { 0, 0, -1, 0 }, -0.55f);

    // (1) convex corner: exactly one contact survives, the deeper -0.65 one, in both store orders.
    {
        Sensor s; s.maStoredContacts[0] = wallX; s.maStoredContacts[1] = wallZ; s.mi32NumStoredContacts = 2;
        const s32 n = s.Cull(lau);
        Check(n == 1 && s.maStoredContacts[lau[0]].mfProjectedDist == -0.65f, "convex corner [x,z] keeps only the deeper wall");
    }
    {
        Sensor s; s.maStoredContacts[0] = wallZ; s.maStoredContacts[1] = wallX; s.mi32NumStoredContacts = 2;
        const s32 n = s.Cull(lau);
        Check(n == 1 && s.maStoredContacts[lau[0]].mfProjectedDist == -0.65f, "convex corner [z,x] keeps only the deeper wall");
    }
    // (2) concave floor + wall (centre (0.7,0.3,0), r 0.4): both survive in both orders.
    {
        const StoredContact floor = World({ 0.7f, 0.0f, 0.0f, 0 }, { 0, 1, 0, 0 }, 0.3f - 0.4f);
        const StoredContact wall  = World({ 1.0f, 0.3f, 0.0f, 0 }, { -1, 0, 0, 0 }, 0.3f - 0.4f);
        Sensor a; a.maStoredContacts[0] = floor; a.maStoredContacts[1] = wall; a.mi32NumStoredContacts = 2;
        Sensor b; b.maStoredContacts[0] = wall; b.maStoredContacts[1] = floor; b.mi32NumStoredContacts = 2;
        Check(a.Cull(lau) == 2 && b.Cull(lau) == 2, "concave floor+wall keeps both contacts");
    }
    // (3) arm asymmetry (>= vs <): face then edge drops the edge; edge then face keeps both.
    {
        const f32 l = std::sqrt(0.01f + 0.09f);
        const StoredContact face = World({ 0.0f, 0.0f, 0.0f, 0 }, { 0, 1, 0, 0 }, -0.1f);
        const StoredContact edge = World({ 0.1f, 0.0f, 0.0f, 0 }, { -0.1f / l, 0.3f / l, 0, 0 }, -0.0838f);
        Sensor a; a.maStoredContacts[0] = face; a.maStoredContacts[1] = edge; a.mi32NumStoredContacts = 2;
        Sensor b; b.maStoredContacts[0] = edge; b.maStoredContacts[1] = face; b.mi32NumStoredContacts = 2;
        Check(a.Cull(lau) == 1, "[face, edge] drops the edge (dist_j >= dist_i arm, d.n_i >= 0)");
        Check(b.Cull(lau) == 2, "[edge, face] keeps both (the < arm tests d.n_j < 0)");
    }
    std::printf("SensorWorldCull: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
