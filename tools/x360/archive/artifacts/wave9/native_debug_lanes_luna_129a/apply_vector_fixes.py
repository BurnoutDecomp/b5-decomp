from pathlib import Path
import hashlib, json

repo = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a")
manifest_path = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a\preflight-input-binding.json")
preflight = json.loads(manifest_path.read_text(encoding="utf-8"))
if preflight["head"] != "129afef95856e383366a55fb8f30a66ceb74de42":
    raise SystemExit("preflight base mismatch")
if preflight["header_count"] < 1000 or preflight["source_count"] != 9:
    raise SystemExit("preflight input snapshot is incomplete")

def sha(data):
    return hashlib.sha256(data).hexdigest()
def edit(rel, replacements):
    path = repo / rel
    raw = path.read_bytes()
    has_bom = raw.startswith(b"\xef\xbb\xbf")
    text = raw.decode("utf-8-sig" if has_bom else "utf-8")
    newline = "\r\n" if "\r\n" in text else "\n"
    text = text.replace("\r\n", "\n")
    before = text
    for old, new, count in replacements:
        actual = text.count(old)
        if actual != count:
            raise SystemExit(f"{rel}: expected {count} match(es), found {actual} for {old[:100]!r}")
        text = text.replace(old, new, count)
    if text == before:
        raise SystemExit(f"{rel}: no changes applied")
    output = (b"\xef\xbb\xbf" if has_bom else b"") + text.replace("\n", newline).encode("utf-8")
    path.write_bytes(output)
    print(f"edited {rel}: {sha(raw)} -> {sha(output)}")

edit("src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp", [
("""        const Vector2 lv2TitlePos = { lrMetrics.mfScreenBorderLeft,
                                     lrMetrics.mfScreenBorderTop - lfRowHeight * KF_TABLETITLEHEIGHT,
                                     0.0f, 0.0f };""",
 """        const Vector2 lv2TitlePos(lrMetrics.mfScreenBorderLeft,
                                  lrMetrics.mfScreenBorderTop - lfRowHeight * KF_TABLETITLEHEIGHT);""", 1),
("""            const Vector2 lv2Min = { lfGridX - KF_GRAPHGRIDHALF, lfGridY, 0.0f, 0.0f };
            const Vector2 lv2Size = { (lfGridX + KF_GRAPHGRIDHALF) - lv2Min.x,
                                      (KF_GRAPHORIGINY + KF_GRAPHGRIDEND) - lv2Min.y,
                                      0.0f, 0.0f };""",
 """            const f32 lfGridLeft = lfGridX - KF_GRAPHGRIDHALF;
            const Vector2 lv2Min(lfGridLeft, lfGridY);
            const Vector2 lv2Size((lfGridX + KF_GRAPHGRIDHALF) - lfGridLeft,
                                  (KF_GRAPHORIGINY + KF_GRAPHGRIDEND) - lfGridY);""", 1),
("""            const Vector2 lv2Pos = { lfLabelX, KF_GRAPHORIGINY + KF_GRAPHLABELDROP, 0.0f, 0.0f };""",
 """            const Vector2 lv2Pos(lfLabelX, KF_GRAPHORIGINY + KF_GRAPHLABELDROP);""", 1),
("""            const Vector2 lv2Min = { KF_GRAPHORIGINX, KF_GRAPHORIGINY,                     0.0f, 0.0f };
            const Vector2 lv2Size = { lfBarRight - lv2Min.x,
                                      (KF_GRAPHORIGINY + KF_GRAPHBARHEIGHT) - lv2Min.y, 0.0f, 0.0f };""",
 """            const Vector2 lv2Min(KF_GRAPHORIGINX, KF_GRAPHORIGINY);
            const Vector2 lv2Size(lfBarRight - KF_GRAPHORIGINX,
                                  (KF_GRAPHORIGINY + KF_GRAPHBARHEIGHT) - KF_GRAPHORIGINY);""", 1),
("""        const Vector2 lv2NamePos = { lpParameter->mfDrawX, lpParameter->mfDrawY, 0.0f, 0.0f };""",
 """        const Vector2 lv2NamePos(lpParameter->mfDrawX, lpParameter->mfDrawY);""", 1),
("""        const Vector2 lv2ValuePos = { lpParameter->mfDrawX + KF_TABLEVALUEOFFSET,
                                      lpParameter->mfDrawY, 0.0f, 0.0f };""",
 """        const Vector2 lv2ValuePos(lpParameter->mfDrawX + KF_TABLEVALUEOFFSET,
                                  lpParameter->mfDrawY);""", 1),
("""        const Vector2 lv2Min = { lfX,               lfY,                         0.0f, 0.0f };
        const Vector2 lv2Size = { (lfX + lfBarLength) - lfX,
                                  (lfY + KF_GRAPHBARHEIGHT) - lfY, 0.0f, 0.0f };""",
 """        const Vector2 lv2Min(lfX, lfY);
        const Vector2 lv2Size((lfX + lfBarLength) - lfX,
                              (lfY + KF_GRAPHBARHEIGHT) - lfY);""", 1),
])

edit("src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.cpp", [
("""    Im2dTransform Im2dTransform::mgDefault =
    {
        { -1.0f, 1.0f, 0.0f, 0.0f },
        { 2.0f, 0.0f, 0.0f, -2.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f }
    };""",
 """    namespace
    {
        Im2dTransform CreateDefaultTransform()
        {
            Im2dTransform lTransform;
            lTransform.mOriginXYZ = rw::math::vpu::Vector4(2.0f, 0.0f, 0.0f, -2.0f);
            lTransform.mRightUp = rw::math::vpu::Vector4(-1.0f, 1.0f, 0.0f, 0.0f);
            lTransform.mColourShift = rw::math::vpu::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            lTransform.mColourScale = rw::math::vpu::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            return lTransform;
        }
    }
    Im2dTransform Im2dTransform::mgDefault = CreateDefaultTransform();""", 1),
("""        Matrix33Template<float> source =
        {
            mgDefault.mRightUp.x, mgDefault.mRightUp.z, 0.0f,
            mgDefault.mRightUp.y, mgDefault.mRightUp.w, 0.0f,
            mgDefault.mOriginXYZ.x, mgDefault.mOriginXYZ.y, 1.0f
        };""",
 """        Matrix33Template<float> source;
        source.mRow0X = mgDefault.mRightUp.GetX().GetFloat();
        source.mRow0Y = mgDefault.mRightUp.GetZ().GetFloat();
        source.mRow0Z = 0.0f;
        source.mRow1X = mgDefault.mRightUp.GetY().GetFloat();
        source.mRow1Y = mgDefault.mRightUp.GetW().GetFloat();
        source.mRow1Z = 0.0f;
        source.mRow2X = mgDefault.mOriginXYZ.GetX().GetFloat();
        source.mRow2Y = mgDefault.mOriginXYZ.GetY().GetFloat();
        source.mRow2Z = 1.0f;""", 1),
("mgAspectCorrected.mRightUp.x = product.mRow0X;", "mgAspectCorrected.mRightUp.SetX(product.mRow0X);", 1),
("mgAspectCorrected.mRightUp.y = product.mRow0Y;", "mgAspectCorrected.mRightUp.SetY(product.mRow0Y);", 1),
("mgAspectCorrected.mRightUp.z = product.mRow1X;", "mgAspectCorrected.mRightUp.SetZ(product.mRow1X);", 1),
("mgAspectCorrected.mRightUp.w = product.mRow1Y;", "mgAspectCorrected.mRightUp.SetW(product.mRow1Y);", 1),
("mgAspectCorrected.mOriginXYZ.x = product.mRow2X;", "mgAspectCorrected.mOriginXYZ.SetX(product.mRow2X);", 1),
("mgAspectCorrected.mOriginXYZ.y = product.mRow2Y;", "mgAspectCorrected.mOriginXYZ.SetY(product.mRow2Y);", 1),
("mgAspectCorrected.mOriginXYZ.w = 0.0f;", "mgAspectCorrected.mOriginXYZ.SetW(0.0f);", 1),
("lTransform.mRow0X = mRightUp.x;", "lTransform.mRow0X = mRightUp.GetX().GetFloat();", 1),
("lTransform.mRow0Y = mRightUp.z;", "lTransform.mRow0Y = mRightUp.GetZ().GetFloat();", 1),
("lTransform.mRow1X = mRightUp.y;", "lTransform.mRow1X = mRightUp.GetY().GetFloat();", 1),
("lTransform.mRow1Y = mRightUp.w;", "lTransform.mRow1Y = mRightUp.GetW().GetFloat();", 1),
("lTransform.mRow2X = mOriginXYZ.x;", "lTransform.mRow2X = mOriginXYZ.GetX().GetFloat();", 1),
("lTransform.mRow2Y = mOriginXYZ.y;", "lTransform.mRow2Y = mOriginXYZ.GetY().GetFloat();", 1),
("mRightUp.x = lSrc.mRow0X;", "mRightUp.SetX(lSrc.mRow0X);", 1),
("mRightUp.y = lSrc.mRow0Y;", "mRightUp.SetY(lSrc.mRow0Y);", 1),
("mRightUp.z = lSrc.mRow1X;", "mRightUp.SetZ(lSrc.mRow1X);", 1),
("mRightUp.w = lSrc.mRow1Y;", "mRightUp.SetW(lSrc.mRow1Y);", 1),
("mOriginXYZ.x = lSrc.mRow2X;", "mOriginXYZ.SetX(lSrc.mRow2X);", 1),
("mOriginXYZ.y = lSrc.mRow2Y;", "mOriginXYZ.SetY(lSrc.mRow2Y);", 1),
("mOriginXYZ.w = 0.0f;", "mOriginXYZ.SetW(0.0f);", 1),
])

edit("src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d_wBT_01.cpp", [
("""    const Vector4 lvMinPosition = { lrMin.mv2Pos.x, lrMin.mv2Pos.y, 0.0f, 0.0f };
    const Vector4 lvMaxPosition = { lrMax.mv2Pos.x, lrMax.mv2Pos.y, 0.0f, 0.0f };""",
 """    const Vector4 lvMinPosition(lrMin.mv2Pos.x, lrMin.mv2Pos.y, 0.0f, 0.0f);
    const Vector4 lvMaxPosition(lrMax.mv2Pos.x, lrMax.mv2Pos.y, 0.0f, 0.0f);""", 1),
("""    const Vector4 lvExtent = { lvMaxPosition.x - lvMinPosition.x, lvMaxPosition.y - lvMinPosition.y,
                               lvMaxPosition.z - lvMinPosition.z, lvMaxPosition.w - lvMinPosition.w };
    const Vector4 lvReciprocalExtent = { 1.0f / lvExtent.x, 1.0f / lvExtent.y, 0.0f, 0.0f };""",
 """    const Vector4 lvExtent(lvMaxPosition.GetX().GetFloat() - lvMinPosition.GetX().GetFloat(),
                           lvMaxPosition.GetY().GetFloat() - lvMinPosition.GetY().GetFloat(),
                           lvMaxPosition.GetZ().GetFloat() - lvMinPosition.GetZ().GetFloat(),
                           lvMaxPosition.GetW().GetFloat() - lvMinPosition.GetW().GetFloat());
    const Vector4 lvReciprocalExtent(1.0f / lvExtent.GetX().GetFloat(),
                                     1.0f / lvExtent.GetY().GetFloat(), 0.0f, 0.0f);""", 1),
("""        const Vector4 lvPositionMinMax = { lvMinPosition.x, lvMinPosition.y, lvMaxPosition.x, lvMaxPosition.y };
        const Vector4 lvUVStartEnd     = { lrMin.mv2Tex0UV.x, lrMin.mv2Tex0UV.y, lrMax.mv2Tex0UV.x, lrMax.mv2Tex0UV.y };""",
 """        const Vector4 lvPositionMinMax(lvMinPosition.GetX().GetFloat(), lvMinPosition.GetY().GetFloat(),
                                       lvMaxPosition.GetX().GetFloat(), lvMaxPosition.GetY().GetFloat());
        const Vector4 lvUVStartEnd(lrMin.mv2Tex0UV.x, lrMin.mv2Tex0UV.y,
                                   lrMax.mv2Tex0UV.x, lrMax.mv2Tex0UV.y);""", 1),
])

edit("src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.cpp", [
("""    const Vector4 lPosition = { lpVertices[0].mv3Pos.x, lpVertices[0].mv3Pos.y,
                               lpVertices[1].mv3Pos.x, lpVertices[1].mv3Pos.y };
    lrLayer.mvPixelMaskPositionMinMax = lPosition;
    const Vector4 lUV = { lpVertices[0].mv2Tex0UV.x, lpVertices[0].mv2Tex0UV.y,
                         lpVertices[1].mv2Tex0UV.x, lpVertices[1].mv2Tex0UV.y };
    lrLayer.mvPixelMaskUVStartEnd = lUV;
    const Vector4 lDifference = { 1.0f / (lpVertices[1].mv3Pos.x - lpVertices[0].mv3Pos.x),
                                 1.0f / (lpVertices[1].mv3Pos.y - lpVertices[0].mv3Pos.y), 0.0f, 0.0f };""",
 """    const Vector4 lPosition(lpVertices[0].mv3Pos.GetX().GetFloat(), lpVertices[0].mv3Pos.GetY().GetFloat(),
                            lpVertices[1].mv3Pos.GetX().GetFloat(), lpVertices[1].mv3Pos.GetY().GetFloat());
    lrLayer.mvPixelMaskPositionMinMax = lPosition;
    const Vector4 lUV(lpVertices[0].mv2Tex0UV.x, lpVertices[0].mv2Tex0UV.y,
                      lpVertices[1].mv2Tex0UV.x, lpVertices[1].mv2Tex0UV.y);
    lrLayer.mvPixelMaskUVStartEnd = lUV;
    const Vector4 lDifference(1.0f / (lpVertices[1].mv3Pos.GetX().GetFloat() - lpVertices[0].mv3Pos.GetX().GetFloat()),
                              1.0f / (lpVertices[1].mv3Pos.GetY().GetFloat() - lpVertices[0].mv3Pos.GetY().GetFloat()),
                              0.0f, 0.0f);""", 1),
])

edit("src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3dUntex.cpp", [
("lpOutput[luVertex].mPosition.x = lpVertices[luVertex].mv3Pos.x;",
 "lpOutput[luVertex].mPosition.x = lpVertices[luVertex].mv3Pos.GetX().GetFloat();", 1),
("lpOutput[luVertex].mPosition.y = lpVertices[luVertex].mv3Pos.y;",
 "lpOutput[luVertex].mPosition.y = lpVertices[luVertex].mv3Pos.GetY().GetFloat();", 1),
("lpOutput[luVertex].mPosition.z = lpVertices[luVertex].mv3Pos.z;",
 "lpOutput[luVertex].mPosition.z = lpVertices[luVertex].mv3Pos.GetZ().GetFloat();", 1),
])