// Tiny embed/usage check: instantiate the owned types and reference every public
// surface so the gate proves the headers compile and member layout/decls are coherent.
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"

// Provide the external Triangle4 validator that ValidateTriangles calls (homed in
// another group's TU). Stubbed here so the embed check links its symbol shape.
namespace CgsGeometric { namespace Triangle4 {
    int AssertIsValid(void* lpTriangle) { (void)lpTriangle; return 0; }
} }

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
