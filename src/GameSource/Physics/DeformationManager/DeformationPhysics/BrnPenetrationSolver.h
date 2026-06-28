#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                       // Vector3, VecFloat, Matrix44Affine
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (base)

// BrnPhysics::Deformation::PenetrationSolver -- the IO buffer the deformation system fills with the
// frame's penetration contacts (car-on-car and car-on-world) plus the per-body transforms/weights, then
// Solve()s to push the interpenetrating bodies apart. Homed at the mirrored DWARF path
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnPenetrationSolver.h; PenetrationContact @ :48,
// PenetrationSolver @ :66).
//
// PenetrationSolver derives CgsModule::IOBuffer (the lock-guarded per-frame IO payload base): the
// deformation producer LockForWrite()s it while accumulating contacts, the solver consumer LockForRead()s
// it. DeformationSensor::AddContactsToPenetrationSolver feeds it (it takes a PenetrationSolver*).
//
// MEMBER LAYOUT is DWARF-authoritative (member sequence + names + types verbatim from the DWARF):
// maObjectTransforms[29], mavfBodyWeighting[29], maVehicleContacts[2016], a 256-byte cache-line padding
// run, maWorldContacts[2016], a second 256-byte padding run, miNumVehicleContacts, miNumWorldContacts.
// The two padding runs (mau8PaddingForCacheLineZeroing / ...2) are REAL DWARF members -- the original
// zeroes the contact arrays a cache line at a time and over-runs the array end by up to a cache line, so
// the padding keeps the following members out of the zeroed range. They are kept by name, not invented.
//
// CAPACITIES are the DWARF named constants (KI_MAX_PENETRATION_BODIES == 29,
// KI_MAX_PENETRATION_CONTACTS == 2016).
//
// All methods are DECLARED-ONLY (bodies live in the PenetrationSolver TU). The trivial getters/counters
// are declared rather than inlined because the DWARF emits them out-of-line.

namespace BrnPhysics
{
namespace Deformation
{
    // DWARF BrnPenetrationSolver.h:32-33. Solver capacities.
    static const s32 KI_MAX_PENETRATION_BODIES  = 29;
    static const s32 KI_MAX_PENETRATION_CONTACTS = 2016;

    // DWARF BrnPenetrationSolver.h:48. One penetration contact between two bodies (or a body and the
    // world): the two contact points (one on each body), the shared separating normal, and the indices of
    // the two participating bodies into the solver's per-body transform/weight arrays.
    struct PenetrationContact
    {
        Vector3 mPointOnA;   // DWARF :50  contact point on body A
        Vector3 mPointOnB;   // DWARF :51  contact point on body B
        Vector3 mNormal;     // DWARF :52  shared separating normal
        s32     miIndexA;    // DWARF :53  body-A index into maObjectTransforms / mavfBodyWeighting
        s32     miIndexB;    // DWARF :54  body-B index (-1 / sentinel for world contacts)
    };

    // DWARF BrnPenetrationSolver.h:66. The penetration-resolution IO buffer.
    struct PenetrationSolver : public CgsModule::IOBuffer
    {
    public:
        // ---- lifecycle (DWARF :68/:69) ----------------------------------------------------------
        void Construct();   // DWARF :68
        void Destruct();    // DWARF :69

        // ---- accumulation (DWARF :75/:83/:91) ---------------------------------------------------
        // Register one participating body: its index, world transform, and solver weighting.
        void AddObject(s32 liIndex, Matrix44Affine lTransform, VecFloat lvfWeighting);   // DWARF :75
        // Append one car-on-car penetration contact.
        void AddVehicleContact(Vector3 lPointOnA, Vector3 lPointOnB, Vector3 lNormal,
                               s32 liIndexA, s32 liIndexB);                              // DWARF :83
        // Append one car-on-world penetration contact.
        void AddWorldContact(Vector3 lPointOnA, Vector3 lPointOnB, Vector3 lNormal,
                             s32 liIndexA, s32 liIndexB);                                // DWARF :91

        // ---- contact access (DWARF :94/:98/:102/:106) -------------------------------------------
        PenetrationContact* GetVehicleContacts();           // DWARF :94
        PenetrationContact* GetWorldContacts();             // DWARF :98
        s32 GetNumVehicleContacts() const;                  // DWARF :102
        s32 GetNumWorldContacts()   const;                  // DWARF :106

        // ---- count setters (DWARF :111/:116) ----------------------------------------------------
        void SetNumVehicleContacts(s32 liNum);              // DWARF :111
        void SetNumWorldContacts(s32 liNum);                // DWARF :116

        // ---- solve (DWARF :120) -----------------------------------------------------------------
        // Resolve all accumulated penetrations, writing updated per-body transforms into
        // maObjectTransforms (read back via GetUpdatedTransform).
        void Solve();                                       // DWARF :120

        // ---- result access (DWARF :124) ---------------------------------------------------------
        const Matrix44Affine* GetUpdatedTransform(s32 liIndex) const;   // DWARF :124

    private:
        // DWARF BrnPenetrationSolver.h:127-136. Member sequence verbatim.
        Matrix44Affine     maObjectTransforms[KI_MAX_PENETRATION_BODIES];   // :127  per-body world transforms
        VecFloat           mavfBodyWeighting[KI_MAX_PENETRATION_BODIES];    // :128  per-body solver weighting

        PenetrationContact maVehicleContacts[KI_MAX_PENETRATION_CONTACTS];  // :130  car-on-car contacts
        u8                 mau8PaddingForCacheLineZeroing[256];             // :131  cache-line zeroing over-run guard
        PenetrationContact maWorldContacts[KI_MAX_PENETRATION_CONTACTS];    // :132  car-on-world contacts
        u8                 mau8PaddingForCacheLineZeroing2[256];            // :133  cache-line zeroing over-run guard

        s32                miNumVehicleContacts;                            // :135  live car-on-car count
        s32                miNumWorldContacts;                              // :136  live car-on-world count
    };
}
}
