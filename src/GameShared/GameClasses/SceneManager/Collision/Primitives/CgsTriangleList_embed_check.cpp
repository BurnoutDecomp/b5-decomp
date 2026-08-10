// Tiny embed/usage check: instantiate the owned types and reference every public
// surface so the gate proves the headers compile and member layout/decls are coherent.
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"

// ⭐⭐ ODR FORK #2's DEFINITION SITE, DELETED 2026-08-10 (fill-worker wave 2).
// What stood here was
//     namespace CgsGeometric { namespace Triangle4 {
//         int AssertIsValid(void* lpTriangle) { (void)lpTriangle; return 0; } } }
// -- a definition of a NAMESPACE free function that merely shared a spelling with
// `struct Triangle4`'s real const member. It satisfied the link while validating
// nothing, and it is exactly the [[odr-forks-link-silently]] shape: the mangled name
// encodes neither class-key nor signature, so nothing could ever have caught it but a
// human reading both headers. ValidateTriangles now calls the real member, whose body
// lives in CgsTriangle4.cpp (X360 0x825BD808) and whose TU is now mounted.

namespace
{
    void EmbedCheck()
    {
        CgsSceneManager::CgsCollision::TriangleList lList = {};
        lList.CheckAlignment();
        lList.ValidateTriangles();

        CgsGeometric::PolygonSoup lSoup = {};
        u8* lpPoly = lSoup.GetPolygon(0);
        u8* lpVert = lSoup.GetVertex(0);
        u8  lNumP  = lSoup.GetNumPolygons();
        u8  lNumV  = lSoup.GetNumVertices();
        (void)lpPoly; (void)lpVert; (void)lNumP; (void)lNumV;
    }
}

int CgsTriangleList_embed_check_entry()
{
    EmbedCheck();
    return 0;
}
