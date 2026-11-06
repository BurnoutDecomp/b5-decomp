#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle.h"
void TriangleEdges(const CgsGeometric::Triangle& t, Vector3& a, Vector3& b) { a=t.CalcEdge01(); b=t.CalcEdge12(); }
