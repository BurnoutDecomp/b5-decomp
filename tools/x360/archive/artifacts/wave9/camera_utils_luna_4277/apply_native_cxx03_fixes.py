from pathlib import Path
import hashlib,json
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277")
manifest=json.loads(Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277\preflight-input-binding.json").read_text(encoding="utf-8"))
if manifest["head"]!="42772cbe0386462d1c32c8442e9f58c9e534e75b" or manifest["source_count"]!=5:
    raise SystemExit("preflight snapshot does not match frozen R8 camera-utils base")
def edit(rel,replacements):
    p=repo/rel;raw=p.read_bytes();bom=raw.startswith(b"\xef\xbb\xbf")
    text=raw.decode("utf-8-sig" if bom else "utf-8");nl="\r\n" if "\r\n" in text else "\n";text=text.replace("\r\n","\n");before=text
    for old,new,count in replacements:
        found=text.count(old)
        if found!=count:raise SystemExit(f"{rel}: expected {count} matches, got {found}: {old[:80]!r}")
        text=text.replace(old,new,count)
    if text==before:raise SystemExit(f"no changes to {rel}")
    out=(b"\xef\xbb\xbf" if bom else b"")+text.replace("\n",nl).encode("utf-8");p.write_bytes(out)
    print(f"edited {rel}: {hashlib.sha256(raw).hexdigest()} -> {hashlib.sha256(out).hexdigest()}")

edit("src/GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.cpp",[
("inline Lanes Load(const Vector3& arV)             { return Make(arV.x, arV.y, arV.z, arV.w); }",
 "inline Lanes Load(const Vector3& arV)             { return Make(arV.mV.v[0], arV.mV.v[1], arV.mV.v[2], arV.mV.v[3]); }",1),
])
edit("src/GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.cpp",[
("""        f32& lrLane = (INDEX == 0) ? lrVector.x : (INDEX == 1) ? lrVector.y : lrVector.z;

        char lacBuffer[KI_CHARBUFFERLENGTH];
        f32  lfTemp = lrLane;
        std::fscanf(mpFile, "%s : %f\\n", lacBuffer, &lfTemp);
        lrLane = lfTemp;""",
 """        char lacBuffer[KI_CHARBUFFERLENGTH];
        f32 lfTemp = lrVector.GetComponent(INDEX).GetFloat();
        std::fscanf(mpFile, "%s : %f\\n", lacBuffer, &lfTemp);
        lrVector.SetComponent(INDEX, lfTemp);""",1),
])
edit("src/GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.cpp",[
("    const f32* const  lapfLane[3] = { &lrValue.x, &lrValue.y, &lrValue.z };\n", "",1),
("""std::fprintf(mpFile, "%s : %f\\n", lacBuffer, *lapfLane[liAxis]);""",
 """std::fprintf(mpFile, "%s : %f\\n", lacBuffer, lrValue.GetComponent(liAxis).GetFloat());""",1),
])
edit("src/GameSource/Director/Camera/Utils/BrnPositionLag.cpp",[
("""    using namespace rw::math::vpu;

    CGS_ASSERT(mbConstructed, "mbConstructed");

    // (1) Inverse of the orthonormal affine: transpose the 3x3, inverse-transform the position.
    const Matrix44Affine lInverseTransform = InverseOfMatrixWithOrthonormal3x3(lrTransform);

    // (2) First frame: seed the carried world position and the inverse-transformed offset.
    if (mbFirstFrame)
    {
        mLastWorldPosition = lrTransform.Pos();
        mLastLocalOffset   = TransformPoint(lInverseTransform, lrTransform.Pos());
        mbFirstFrame       = false;
    }

    // (3) Project the previous world position into the current inverted local frame, then
    //     swap the carried world position to the current Pos (asm reads the old value first).
    const Vector3 lLocalOffset = TransformPoint(lInverseTransform, mLastWorldPosition);
    mLastWorldPosition = lrTransform.Pos();

    // Ease the carried local offset toward the freshly projected one by (1 - mfSmoothing).
    mLastLocalOffset = Lerp(mLastLocalOffset, lLocalOffset, 1.0f - lrParameters.mfSmoothing);

    // (4) Scale by the per-axis response, rotate the eased offset back to world space, and
    //     add it onto the transform's translation.
    const Vector3 lResponse{ lrParameters.mfXResponse, lrParameters.mfYResponse, lrParameters.mfZResponse, 0.0f };
    const Vector3 lScaledOffset = Mult(mLastLocalOffset, lResponse);
    lrTransform.Pos() = lrTransform.Pos() + TransformVector(lrTransform, lScaledOffset);""",
 """    CGS_ASSERT(mbConstructed, "mbConstructed");

    // (1) Inverse of the orthonormal affine: transpose the 3x3, inverse-transform the position.
    const rw::math::vpu::Matrix44Affine lInverseTransform =
        rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lrTransform);

    // (2) First frame: seed the carried world position and the inverse-transformed offset.
    if (mbFirstFrame)
    {
        mLastWorldPosition = lrTransform.Pos();
        mLastLocalOffset   = rw::math::vpu::TransformPoint(lInverseTransform, lrTransform.Pos());
        mbFirstFrame       = false;
    }

    // (3) Project the previous world position into the current inverted local frame, then
    //     swap the carried world position to the current Pos (asm reads the old value first).
    const rw::math::vpu::Vector3 lLocalOffset =
        rw::math::vpu::TransformPoint(lInverseTransform, mLastWorldPosition);
    mLastWorldPosition = lrTransform.Pos();

    // Ease the carried local offset toward the freshly projected one by (1 - mfSmoothing).
    mLastLocalOffset = rw::math::vpu::Lerp(mLastLocalOffset, lLocalOffset, 1.0f - lrParameters.mfSmoothing);

    // (4) Scale by the per-axis response, rotate the eased offset back to world space, and
    //     add it onto the transform's translation. The float3 constructor writes W=0.
    const rw::math::vpu::Vector3 lResponse(
        lrParameters.mfXResponse, lrParameters.mfYResponse, lrParameters.mfZResponse);
    const rw::math::vpu::Vector3 lScaledOffset = rw::math::vpu::Mult(mLastLocalOffset, lResponse);
    lrTransform.Pos() = lrTransform.Pos() + rw::math::vpu::TransformVector(lrTransform, lScaledOffset);""",1),
])

edit("src/GameSource/Director/Camera/Utils/BrnCameraShakeICEController.cpp",[
("""    Vector3 lTargetCameraAnglesRandom;
    lTargetCameraAnglesRandom.x =
        mfBumpValue * 0.01f * (1.0f / kfShakeFocalDistance) * lfShakeAmplitude;
    lTargetCameraAnglesRandom.y = 0.0f;
    lTargetCameraAnglesRandom.z = 0.0f;
    lTargetCameraAnglesRandom.w = 0.0f;""",
 """    Vector3 lTargetCameraAnglesRandom;
    lTargetCameraAnglesRandom.SetX(
        mfBumpValue * 0.01f * (1.0f / kfShakeFocalDistance) * lfShakeAmplitude);
    lTargetCameraAnglesRandom.SetY(0.0f);
    lTargetCameraAnglesRandom.SetZ(0.0f);""",1),
("""    const f32 frame = lfShakeFrequency * mfShotRunningTime * 30.0f;
    const s32 wholeFrame = static_cast<s32>(frame);
    const s32 count = mShakeTake.GetNumKeys(11);
    auto key = [count](s32 value) -> u16 {
        if (count <= 0) return 0;
        while (value < 0) value += count;
        return static_cast<u16>(value % count);
    };
    const u16 current = key(wholeFrame), next = key(wholeFrame + 1);
    const f32 t = frame - static_cast<f32>(wholeFrame);
    using namespace rw::math::vpu;
    auto sampleRotation = [&](u16 k) {
        Vector3 v = {mShakeTake.GetValueFloat(22,k), mShakeTake.GetValueFloat(23,k), mShakeTake.GetValueFloat(24,k),0};
        v = v * (lfShakeAmplitude / kfShakeFocalDistance);
        const f32 remainder = 1.0f - Magnitude(v);
        { const Quaternion lResult = {v.x,v.y,v.z,remainder == 0.0f ? 0.0f : std::sqrt(remainder)}; return lResult; }
    };
    Quaternion a = sampleRotation(current), b = sampleRotation(next);""",
 """    const f32 frame = lfShakeFrequency * mfShotRunningTime * 30.0f;
    const s32 wholeFrame = static_cast<s32>(frame);
    const s32 count = mShakeTake.GetNumKeys(11);

    s32 currentFrame = wholeFrame;
    u16 current = 0;
    if (count > 0)
    {
        while (currentFrame < 0) currentFrame += count;
        current = static_cast<u16>(currentFrame % count);
    }

    s32 nextFrame = wholeFrame + 1;
    u16 next = 0;
    if (count > 0)
    {
        while (nextFrame < 0) nextFrame += count;
        next = static_cast<u16>(nextFrame % count);
    }

    const f32 t = frame - static_cast<f32>(wholeFrame);
    using namespace rw::math::vpu;

    // The two C++11 closures are expanded in their original evaluation order: key 22, 23, 24;
    // native float3 construction (W=0); scalar broadcast multiply; then xyz magnitude.
    const f32 lfCurrentX = mShakeTake.GetValueFloat(22, current);
    const f32 lfCurrentY = mShakeTake.GetValueFloat(23, current);
    const f32 lfCurrentZ = mShakeTake.GetValueFloat(24, current);
    Vector3 lvCurrentRotation(lfCurrentX, lfCurrentY, lfCurrentZ);
    lvCurrentRotation = lvCurrentRotation * (lfShakeAmplitude / kfShakeFocalDistance);
    const f32 lfCurrentRemainder = 1.0f - Magnitude(lvCurrentRotation);
    Quaternion a = {lvCurrentRotation.GetX().GetFloat(),
                    lvCurrentRotation.GetY().GetFloat(),
                    lvCurrentRotation.GetZ().GetFloat(),
                    lfCurrentRemainder == 0.0f ? 0.0f : std::sqrt(lfCurrentRemainder)};

    const f32 lfNextX = mShakeTake.GetValueFloat(22, next);
    const f32 lfNextY = mShakeTake.GetValueFloat(23, next);
    const f32 lfNextZ = mShakeTake.GetValueFloat(24, next);
    Vector3 lvNextRotation(lfNextX, lfNextY, lfNextZ);
    lvNextRotation = lvNextRotation * (lfShakeAmplitude / kfShakeFocalDistance);
    const f32 lfNextRemainder = 1.0f - Magnitude(lvNextRotation);
    Quaternion b = {lvNextRotation.GetX().GetFloat(),
                    lvNextRotation.GetY().GetFloat(),
                    lvNextRotation.GetZ().GetFloat(),
                    lfNextRemainder == 0.0f ? 0.0f : std::sqrt(lfNextRemainder)};""",1),
("""    if (dot < 0.0f) { a = {-a.x,-a.y,-a.z,-a.w}; dot = -dot; }
    Quaternion rotation;
    if (dot > std::cos(0.087266468f))
    {
        rotation = {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
        const f32 inverseLength = 1.0f / std::sqrt(rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z+rotation.w*rotation.w);
        rotation = {rotation.x*inverseLength,rotation.y*inverseLength,rotation.z*inverseLength,rotation.w*inverseLength};
    }
    else
    {
        const f32 angle = std::acos(dot);
        const f32 from = std::sin((1.0f-t)*angle) / std::sin(angle);
        const f32 to = std::sin(t*angle) / std::sin(angle);
        rotation = {a.x*from+b.x*to,a.y*from+b.y*to,a.z*from+b.z*to,a.w*from+b.w*to};
    }""",
 """    if (dot < 0.0f)
    {
        a.x = -a.x;
        a.y = -a.y;
        a.z = -a.z;
        a.w = -a.w;
        dot = -dot;
    }
    Quaternion rotation;
    if (dot > std::cos(0.087266468f))
    {
        rotation.x = a.x + (b.x - a.x) * t;
        rotation.y = a.y + (b.y - a.y) * t;
        rotation.z = a.z + (b.z - a.z) * t;
        rotation.w = a.w + (b.w - a.w) * t;
        const f32 inverseLength = 1.0f / std::sqrt(rotation.x * rotation.x + rotation.y * rotation.y
                                                  + rotation.z * rotation.z + rotation.w * rotation.w);
        rotation.x = rotation.x * inverseLength;
        rotation.y = rotation.y * inverseLength;
        rotation.z = rotation.z * inverseLength;
        rotation.w = rotation.w * inverseLength;
    }
    else
    {
        const f32 angle = std::acos(dot);
        const f32 from = std::sin((1.0f - t) * angle) / std::sin(angle);
        const f32 to = std::sin(t * angle) / std::sin(angle);
        rotation.x = a.x * from + b.x * to;
        rotation.y = a.y * from + b.y * to;
        rotation.z = a.z * from + b.z * to;
        rotation.w = a.w * from + b.w * to;
    }""",1),
("""    mMatrix.Pos().y -= std::tan(angles.x) * kfShakeFocalDistance;
    mu8ActiveShake = lu8ShakeType;
    static u32 lines = 0;
    if (trace && lfShakeAmplitude > 0.1f && lines++ < 32 && CgsDev::Log::gpDebugPrint)
        *CgsDev::Log::gpDebugPrint << "[camera-rig] boost-shake type=" << static_cast<u32>(lu8ShakeType)
            << " keys=" << count << " frame=" << frame << " angles=" << angles.x << "," << angles.y << "," << angles.z
            << " lift=" << mMatrix.Pos().y << "\\n";""",
 """    mMatrix.Pos().Y() -= std::tan(angles.GetX().GetFloat()) * kfShakeFocalDistance;
    mu8ActiveShake = lu8ShakeType;
    static u32 lines = 0;
    if (trace && lfShakeAmplitude > 0.1f && lines++ < 32 && CgsDev::Log::gpDebugPrint)
        *CgsDev::Log::gpDebugPrint << "[camera-rig] boost-shake type=" << static_cast<u32>(lu8ShakeType)
            << " keys=" << count << " frame=" << frame << " angles=" << angles.GetX().GetFloat() << ","
            << angles.GetY().GetFloat() << "," << angles.GetZ().GetFloat()
            << " lift=" << mMatrix.Pos().GetY().GetFloat() << "\\n";""",1),
])