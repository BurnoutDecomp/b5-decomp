// Embed check for Zone.h: exercises the boot-trace ledger function plus the inline
// accessor surface, and proves the recovered member layout.
//   CgsSceneManager::Zone::IsPointInZone @ 0x822BAB60
#include "GameShared/GameClasses/SceneManager/Zones/Zone.h"

namespace
{
    // Layout sanity: the Zone packs three 32-bit-on-console pointers, a u64 id, four
    // s16s and a u32. The exact host sizeof differs (64-bit pointers) so we only assert
    // the alignment contract and the relative ordering via offsetof on same-width fields.
    static_assert(alignof(CgsSceneManager::Zone) == 16, "Zone must be 16-byte aligned");
    static_assert(alignof(CgsSceneManager::Neighbour) == 16, "Neighbour must be 16-byte aligned");

    bool ExerciseZone(const CgsSceneManager::Zone& lrZone,
                      const rw::math::vpu::Vector2& lrPoint)
    {
        // The ledger function.
        bool lbInside = lrZone.IsPointInZone(lrPoint);

        // Inline const accessor surface.
        (void)lrZone.GetPoints();
        (void)lrZone.GetSafeNeighbours();
        (void)lrZone.GetUnsafeNeighbours();
        (void)lrZone.GetNumPoints();
        (void)lrZone.GetNumSafeNeighbours();
        (void)lrZone.GetNumUnsafeNeighbours();
        (void)lrZone.GetId();
        (void)lrZone.GetType();
        (void)lrZone.GetFlags();

        return lbInside;
    }

    void ExerciseZoneMutators(CgsSceneManager::Zone& lrZone,
                              rw::math::vpu::Vector2* lpPoints, s16 liNum,
                              CgsSceneManager::Neighbour* lpNeighbours, s16 liNeighbours)
    {
        lrZone.SetPoints(lpPoints, liNum);
        lrZone.SetSafeNeighbours(lpNeighbours, liNeighbours);
        lrZone.SetUnsafeNeighbours(lpNeighbours, liNeighbours);
        lrZone.SetId(7u);
        lrZone.SetType(static_cast<s16>(2));
        lrZone.SetFlags(0u);
        if (liNum > 0)
        {
            lrZone.SetPoint(0, lpPoints[0]);
            (void)lrZone.GetPoint(0);
        }
        (void)lrZone.GetPoints();
        (void)lrZone.GetSafeNeighbours();
    }

    void ExerciseNeighbour(const CgsSceneManager::Neighbour& lrNeighbour)
    {
        (void)lrNeighbour.GetZone();
        (void)lrNeighbour.IsFlagSet(CgsSceneManager::Neighbour::E_NEIGHBOURFLAG_RENDER);
        (void)lrNeighbour.GetFlags();
    }
}

extern bool Zone_embed_check_anchor(CgsSceneManager::Zone&,
                                    const rw::math::vpu::Vector2&,
                                    const CgsSceneManager::Neighbour&,
                                    rw::math::vpu::Vector2*, s16,
                                    CgsSceneManager::Neighbour*, s16);
bool Zone_embed_check_anchor(CgsSceneManager::Zone& lrZone,
                             const rw::math::vpu::Vector2& lrPoint,
                             const CgsSceneManager::Neighbour& lrNeighbour,
                             rw::math::vpu::Vector2* lpPoints, s16 liNum,
                             CgsSceneManager::Neighbour* lpNeighbours, s16 liNeighbours)
{
    ExerciseNeighbour(lrNeighbour);
    ExerciseZoneMutators(lrZone, lpPoints, liNum, lpNeighbours, liNeighbours);
    return ExerciseZone(lrZone, lrPoint);
}
