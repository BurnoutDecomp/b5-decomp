// CgsGeometricUnity: one of ARTIST's unity translation units (DWARF compile unit CgsGeometricUnity.cpp),
// ARTIST 0x82839550-0x82846524.
// Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x820DFA30: ...gameshared\gameclasses\geometric\Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 12/16 transitions forward);
// first member: the one PS3's static constructor names (_GLOBAL__I_ of CgsGeometric::CubicSpline::BuildCatmullRom).

#include "Primitives/CgsFrustum.cpp"
#include "Primitives/PolygonSoup/CgsPolygonSoupList.cpp"
#include "Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.cpp"
#include "Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap_wBT_01.cpp"  // part of CgsPolygonSoupListSpatialMap.cpp (x360 tree partfile)
#include "Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap_Build.cpp"  // holds functions of CgsPolygonSoupListSpatialMap.cpp (x360 tree split)
#include "Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap_Query.cpp"  // holds functions of CgsPolygonSoupListSpatialMap.cpp (x360 tree split)
#include "Intersection/CgsPolygonSoupTests.cpp"
#include "Intersection/CgsPolygonSoupTests_LineNearest.cpp"  // holds functions of CgsPolygonSoupTests.cpp (x360 tree split)
#include "Intersection/CgsTriangleSphere.cpp"
#include "Intersection/CgsTriangleBox.cpp"
#include "Primitives/PolygonSoup/CgsPolygonSoupListResourceType.cpp"
