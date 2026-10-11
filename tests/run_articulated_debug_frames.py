"""Prepare/run the grouped production-body fixture; root coordinates execution.

--prepare-only writes the extracted include and SHA256 map without compiling or running.
The regular invocation compiles one executable with the canonical MSVC flags/includes.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import sys

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
from fxgs_common import REPO, WORKFLOW, definition, compile_and_run

POOL = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.cpp"
JOINT = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJoint.cpp"
DEBUG = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerDebugComponent.cpp"
INVERSE = "vendor/renderware/src/rw/math/vpu/Matrix44Operation.cpp"
FRAMES = "src/vendor/renderware/physics/JointFrames.cpp"
FRAMES_HEADER = "src/vendor/renderware/physics/JointFrames.hpp"
RENDER = "src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.cpp"
CREATE = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_Create.cpp"
MANAGER = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.cpp"
EVENT = "src/GameSource/Physics/VehicleManager/SharedIO/CreateArticulatedTrafficEvent.cpp"
RGBA = "src/GameShared/GameClasses/RenderWare/RwRGBA.cpp"

# The setter's original register stream and immutable constants are embedded
# below so this test remains independent of private IDA exports at runtime.
# BSS sign masks use their real 82C74000/30/60 startup writers, not image zeros.
QUATERNION_ASSEMBLY = """0x8259B580  lis       r11, unk_8327F100@ha
0x8259B584  vspltisw  v0, 1
0x8259B588  li        r10, 0x10
0x8259B58C  lvx128    v10, r0, r4
0x8259B590  addi      r11, r11, unk_8327F100@l
0x8259B594  vspltw    v6, v10, 0
0x8259B598  li        r9, 0x20 # ' '
0x8259B59C  vcfsx     v7, v0, 0
0x8259B5A0  vcfsx     v0, v0, 1
0x8259B5A4  lvx128    v9, r4, r10
0x8259B5A8  lis       r10, unk_82CDA3F0@ha
0x8259B5AC  lvx128    v12, r0, r11
0x8259B5B0  lis       r11, unk_8327F120@ha
0x8259B5B4  lvx128    v8, r4, r9
0x8259B5B8  vspltw    v5, v9, 1
0x8259B5BC  addi      r11, r11, unk_8327F120@l
0x8259B5C0  vspltw    v13, v8, 2
0x8259B5C4  addi      r10, r10, unk_82CDA3F0@l
0x8259B5C8  vxor      v12, v5, v12
0x8259B5CC  lvx128    v11, r0, r11
0x8259B5D0  lis       r11, unk_8327F0F0@ha
0x8259B5D4  vxor      v11, v6, v11
0x8259B5D8  addi      r11, r11, unk_8327F0F0@l
0x8259B5DC  lvx128    v4, r0, r11
0x8259B5E0  lis       r11, unk_82CDA3D0@ha
0x8259B5E4  vxor      v1, v13, v4
0x8259B5E8  vaddfp    v4, v11, v12
0x8259B5EC  addi      r11, r11, unk_82CDA3D0@l
0x8259B5F0  vaddfp    v12, v1, v7
0x8259B5F4  lvx128    v3, r0, r11
0x8259B5F8  lis       r11, unk_82CDA370@ha
0x8259B5FC  vperm     v1, v10, v0, v3
0x8259B600  addi      r11, r11, unk_82CDA370@l
0x8259B604  lvx128    v2, r0, r11
0x8259B608  lis       r11, unk_8208C24C@ha
0x8259B60C  vperm     v2, v9, v8, v2
0x8259B610  addi      r11, r11, unk_8208C24C@l
0x8259B614  vaddfp    v12, v4, v12
0x8259B618  lvlx      v31, r0, r11
0x8259B61C  lis       r11, unk_82CDA360@ha
0x8259B620  addi      r11, r11, unk_82CDA360@l
0x8259B624  vrsqrtefp v11, v12
0x8259B628  vmulfp128 v30, v11, v11
0x8259B62C  vmulfp128 v29, v11, v0
0x8259B630  vnmsubfp  v3, v12, v7, v30
0x8259B634  vmaddfp   v11, v29, v11, v3
0x8259B638  vmulfp128 v3, v11, v11
0x8259B63C  vmulfp128 v30, v11, v0
0x8259B640  vnmsubfp  v7, v12, v7, v3
0x8259B644  vmaddfp   v11, v30, v11, v7
0x8259B648  lvx128    v7, r0, r10
0x8259B64C  lis       r10, unk_82CDA3A0@ha
0x8259B650  addi      r10, r10, unk_82CDA3A0@l
0x8259B654  lvx128    v3, r0, r10
0x8259B658  vperm     v10, v8, v10, v3
0x8259B65C  vspltw    v8, v31, 0
0x8259B660  vcmpgtfp  v31, v6, v13
0x8259B664  vmulfp128 v12, v12, v11
0x8259B668  vmulfp128 v0, v0, v11
0x8259B66C  vspltisw  v11, 0
0x8259B670  vperm     v9, v9, v11, v7
0x8259B674  lvx128    v7, r0, r11
0x8259B678  lis       r11, unk_82CDA3B0@ha
0x8259B67C  vsldoi    v11, v2, v1, v8
0x8259B680  vaddfp    v2, v4, v13
0x8259B684  addi      r11, r11, unk_82CDA3B0@l
0x8259B688  vsldoi    v10, v10, v9, v8
0x8259B68C  lvx128    v3, r0, r11
0x8259B690  lis       r11, unk_82CDA420@ha
0x8259B694  vperm     v9, v12, v0, v7
0x8259B698  addi      r11, r11, unk_82CDA420@l
0x8259B69C  vspltw    v30, v2, 0
0x8259B6A0  lvx128    v4, r0, r11
0x8259B6A4  lis       r11, unk_82CDA380@ha
0x8259B6A8  vperm     v1, v12, v0, v4
0x8259B6AC  addi      r11, r11, unk_82CDA380@l
0x8259B6B0  lvx128    v7, r0, r11
0x8259B6B4  lis       r11, unk_82CDA430@ha
0x8259B6B8  addi      r11, r11, unk_82CDA430@l
0x8259B6BC  lvx128    v4, r0, r11
0x8259B6C0  lis       r11, unk_82CDA3E0@ha
0x8259B6C4  addi      r11, r11, unk_82CDA3E0@l
0x8259B6C8  vcmpgtfp  v29, v6, v5
0x8259B6CC  vperm     v4, v12, v0, v4
0x8259B6D0  vcmpgtfp  v5, v5, v13
0x8259B6D4  vaddfp    v13, v11, v10
0x8259B6D8  vcmpgtfp  v8, v30, v8
0x8259B6DC  lvx128    v2, r0, r11
0x8259B6E0  lis       r11, unk_82CDA390@ha
0x8259B6E4  addi      r11, r11, unk_82CDA390@l
0x8259B6E8  lvx128    v6, r0, r11
0x8259B6EC  vperm     v12, v12, v0, v6
0x8259B6F0  vsubfp    v0, v11, v10
0x8259B6F4  vand      v6, v29, v31
0x8259B6F8  vperm     v11, v0, v13, v3
0x8259B6FC  vperm     v10, v0, v13, v7
0x8259B700  vperm     v13, v0, v13, v2
0x8259B704  vmulfp128 v0, v0, v12
0x8259B708  vmulfp128 v11, v11, v9
0x8259B70C  vmulfp128 v12, v10, v1
0x8259B710  vmulfp128 v13, v13, v4
0x8259B714  vsel      v12, v12, v11, v5
0x8259B718  vsel      v13, v12, v13, v6
0x8259B71C  vsel      v0, v13, v0, v8
0x8259B720  stvx128   v0, r0, r3
0x8259B724  blr"""
QUATERNION_CONSTANTS = {
    "0x8327F100": [
        0,
        2147483648,
        0,
        2147483648
    ],
    "0x8327F120": [
        0,
        0,
        2147483648,
        2147483648
    ],
    "0x8327F0F0": [
        0,
        2147483648,
        2147483648,
        0
    ],
    "0x82CDA3F0": [
        66051,
        269554195,
        66051,
        66051
    ],
    "0x82CDA3D0": [
        67438087,
        269554195,
        66051,
        66051
    ],
    "0x82CDA370": [
        66051,
        66051,
        134810123,
        269554195
    ],
    "0x8208C24C": [
        0,
        1035122883,
        1024391733,
        1065336439
    ],
    "0x82CDA360": [
        404298267,
        134810123,
        404298267,
        404298267
    ],
    "0x82CDA3A0": [
        66051,
        66051,
        67438087,
        404298267
    ],
    "0x82CDA3B0": [
        404298267,
        202182159,
        269554195,
        67438087
    ],
    "0x82CDA420": [
        471670303,
        471670303,
        202182159,
        471670303
    ],
    "0x82CDA380": [
        336926231,
        269554195,
        202182159,
        134810123
    ],
    "0x82CDA430": [
        67438087,
        336926231,
        336926231,
        336926231
    ],
    "0x82CDA3E0": [
        202182159,
        404298267,
        336926231,
        66051
    ],
    "0x82CDA390": [
        269554195,
        269554195,
        269554195,
        66051
    ]
}


def quaternion_program():
    """Lower the saved setter instructions, independently of helper algebra."""
    lines = []
    gpr = {"r0": 0, "r3": "result", "r4": "matrix"}
    for line in QUATERNION_ASSEMBLY.splitlines():
        address, op, *tail = line.split(None, 2)
        args = [arg.strip() for arg in (tail[0] if tail else "").split("#", 1)[0].split(",")]
        regs = [int(arg[1:]) for arg in args if re.fullmatch(r"v\d+", arg)]
        if op == "lis":
            value = int(re.search(r"unk_([0-9A-F]+)", args[1])[1], 16)
            gpr[args[0]] = ((value + 0x8000) >> 16) << 16
            continue
        if op == "li":
            gpr[args[0]] = int(args[1], 0)
            continue
        if op == "addi":
            value = int(re.search(r"unk_([0-9A-F]+)", args[2])[1], 16) & 0xFFFF
            gpr[args[0]] = gpr[args[1]] + (value - 0x10000 if value & 0x8000 else value)
            continue
        if op == "blr":
            continue
        if op in {"lvx128", "lvlx"}:
            dest = regs[0]
            left, right = gpr[args[1]], gpr[args[2]]
            if left == "matrix" or right == "matrix":
                offset = right if left == "matrix" else left
                code = f"for(unsigned i=0;i<4;++i)v[{dest}][i]=OracleBits(input[{offset//4}+i]);"
            else:
                words = QUATERNION_CONSTANTS[f"0x{left+right:08X}"]
                code = f"v[{dest}] = {{{','.join(f'0x{word:08X}u' for word in words)}}};"
        elif op == "stvx128":
            code = f"output=v[{regs[0]}];"
        elif op == "vspltisw":
            code = f"v[{regs[0]}].fill(static_cast<unsigned int>({args[1]}));"
        elif op == "vspltw":
            code = f"v[{regs[0]}].fill(v[{regs[1]}][{args[2]}]);"
        elif op == "vcfsx":
            d,a=regs; scale=int(args[2], 0)
            code = f"{{const auto a=v[{a}];for(unsigned i=0;i<4;++i)v[{d}][i]=OracleBits(std::ldexp(static_cast<float>(static_cast<int>(a[i])),-{scale}));}}"
        elif op == "vperm":
            d,a,b,c=regs
            code=f"v[{d}]=OraclePermute(v[{a}],v[{b}],v[{c}]);"
        elif op == "vsldoi":
            d,a,b=regs[:3]; shift=int(args[3].removeprefix("v"), 0)
            assert shift == 8
            code=f"{{const auto a=v[{a}],b=v[{b}];v[{d}]={{a[2],a[3],b[0],b[1]}};}}"
        elif op == "vrsqrtefp":
            d,a=regs
            code=f"{{const auto a=v[{a}];for(unsigned i=0;i<4;++i)v[{d}][i]=OracleBits(OracleVrsqrtefp(OracleFloat(a[i])));}}"
        elif op in {"vmaddfp", "vnmsubfp"}:
            d,a,b,c=regs
            function="OracleFma" if op=="vmaddfp" else "OracleNmsub"
            code=f"{{const auto a=v[{a}],b=v[{b}],c=v[{c}];for(unsigned i=0;i<4;++i)v[{d}][i]=OracleBits({function}(OracleFloat(a[i]),OracleFloat(c[i]),OracleFloat(b[i])));}}"
        elif op in {"vaddfp", "vsubfp", "vmulfp128", "vcmpgtfp", "vxor", "vand", "vsel"}:
            d,a,b=regs[:3]
            if op == "vsel":
                c=regs[3]
                code=f"{{const auto a=v[{a}],b=v[{b}],c=v[{c}];for(unsigned i=0;i<4;++i)v[{d}][i]=(a[i]&~c[i])|(b[i]&c[i]);}}"
            else:
                if op in {"vxor", "vand"}:
                    expression="a[i]"+("^" if op=="vxor" else "&")+"b[i]"
                elif op == "vcmpgtfp":
                    expression="OracleFlush(OracleFloat(a[i]))>OracleFlush(OracleFloat(b[i]))?0xFFFFFFFFu:0u"
                else:
                    operator={"vaddfp":"+", "vsubfp":"-", "vmulfp128":"*"}[op]
                    expression=f"OracleBits(OracleFlush(OracleFlush(OracleFloat(a[i])){operator}OracleFlush(OracleFloat(b[i]))))"
                code=f"{{const auto a=v[{a}],b=v[{b}];for(unsigned i=0;i<4;++i)v[{d}][i]={expression};}}"
        else:
            raise ValueError("Unmodelled ARTIST quaternion instruction: " + line)
        lines.append("    " + code + " // " + address + " " + op)
    return "\n".join(lines)


def inverse_program():
    """Emit the saved ARTIST register program, independently of production algebra."""
    assembly = (HERE / "fixtures" / "ArtistMatrix44Inverse.asm").read_text(encoding="utf-8")
    lines = []
    for line in assembly.splitlines():
        fields = line.split(None, 2)
        address, op = fields[:2]
        operands = fields[2] if len(fields) == 3 else ""
        args = [arg.strip() for arg in operands.split("#", 1)[0].split(",")]
        regs = [int(arg[1:]) for arg in args if re.fullmatch(r"v\d+", arg)]
        if op in {"lis", "li", "addi", "blr"}:
            continue
        if op == "lvx128":
            dest = regs[0]
            if args[1:] == ["r0", "r11"]:
                code = f"v[{dest}] = {{0x04050607u,0x08090A0Bu,0x0C0D0E0Fu,0x00010203u}};"
            else:
                row = {"r4": 0, "r9": 2, "r10": 1, "r8": 3}[args[2]]
                code = f"for(unsigned i=0;i<4;++i)v[{dest}][i]=OracleBits(input[{4*row}+i]);"
        elif op == "stvx128":
            if args[2] == "r5":
                code = f"det = v[{regs[0]}];"
            else:
                row = {"r3": 0, "r10": 1, "r9": 2, "r8": 3}[args[2]]
                code = f"for(unsigned i=0;i<4;++i)output[{4*row}+i]=OracleFloat(v[{regs[0]}][i]);"
        elif op == "vspltisw":
            code = f"v[{regs[0]}].fill(static_cast<unsigned int>({args[1]}));"
        elif op == "vperm":
            d,a,b,c = regs
            code = f"v[{d}]=OraclePermute(v[{a}],v[{b}],v[{c}]);"
        elif op == "vrlimi128":
            d,a=regs;mask=int(args[2],0);rotate=int(args[3],0)
            code = f"{{const auto a=v[{a}];for(unsigned i=0;i<4;++i)if({mask}u & (8u>>i))v[{d}][i]=a[(i+{rotate})&3];}}"
        elif op == "vmr":
            code = f"v[{regs[0]}]=v[{regs[1]}];"
        elif op in {"vmrghw", "vmrglw"}:
            d,a,b=regs;base=0 if op=="vmrghw" else 2
            code = f"{{const auto a=v[{a}],b=v[{b}];v[{d}]={{a[{base}],b[{base}],a[{base+1}],b[{base+1}]}};}}"
        elif op == "vmsum4fp128":
            d,a,b=regs
            code = f"v[{d}].fill(OracleBits(OracleDot4(v[{a}],v[{b}])));"
        elif op == "vcfsx":
            d,a=regs;scale=int(args[2],0)
            code = f"{{const auto a=v[{a}];for(unsigned i=0;i<4;++i)v[{d}][i]=OracleBits(std::ldexp(static_cast<float>(static_cast<int>(a[i])),-{scale}));}}"
        elif op == "vrefp":
            d,a=regs
            code = f"{{const auto a=v[{a}];for(unsigned i=0;i<4;++i)v[{d}][i]=OracleBits(OracleVrefp(OracleFloat(a[i])));}}"
        elif op in {"vmaddfp", "vnmsubfp", "vmulfp128", "vslw", "vxor"}:
            d,a,b=regs[:3]
            if op in {"vmaddfp", "vnmsubfp"}:
                c=regs[3]
                expression = ("OracleFma" if op=="vmaddfp" else "OracleNmsub")+"(OracleFloat(a[i]),OracleFloat(c[i]),OracleFloat(b[i]))"
                code=f"{{const auto a=v[{a}],b=v[{b}],c=v[{c}];for(unsigned i=0;i<4;++i)v[{d}][i]=OracleBits({expression});}}"
            elif op=="vmulfp128":
                code=f"{{const auto a=v[{a}],b=v[{b}];for(unsigned i=0;i<4;++i)v[{d}][i]=OracleBits(OracleFlush(OracleFlush(OracleFloat(a[i]))*OracleFlush(OracleFloat(b[i]))));}}"
            else:
                expression="a[i]<<(b[i]&31)" if op=="vslw" else "a[i]^b[i]"
                code=f"{{const auto a=v[{a}],b=v[{b}];for(unsigned i=0;i<4;++i)v[{d}][i]={expression};}}"
        else:
            raise ValueError("Unmodelled ARTIST inverse instruction: " + line)
        lines.append("    " + code + " // " + address + " " + op)
    program = "\n".join(lines)
    return program


def prepare(output, program, quaternion):
    sources = {path: (REPO / path).read_text(encoding="utf-8-sig")
               for path in [POOL, JOINT, DEBUG, INVERSE, FRAMES, FRAMES_HEADER, RENDER, CREATE, MANAGER, EVENT, RGBA]}
    parts = [sources[INVERSE], "namespace rw {",
             definition(sources[RGBA], "RGBA::RGBA(uint8_t"), "}",
             "namespace BrnPhysics { namespace Vehicle {",
             definition(sources[POOL], "namespace\n{"),
             definition(sources[POOL], "void ArticulatedJointPool::Construct()"),
             definition(sources[POOL], "void ArticulatedJointPool::ConstructArticulatedJoint("),
             definition(sources[POOL], "ArticulatedJoint* ArticulatedJointPool::GetJoint("),
             definition(sources[POOL], "bool ArticulatedJointPool::IsJointInUse("),
             definition(sources[JOINT], "void ArticulatedJointId::Construct()"),
             definition(sources[JOINT], "void ArticulatedJoint::Construct()"),
             definition(sources[JOINT], "void ArticulatedJoint::Set("),
             definition(sources[JOINT], "rw::math::vpu::Matrix44Affine ArticulatedJoint::GetParentToJointTransform() const"),
             definition(sources[DEBUG], "namespace\n    {"),
             definition(sources[DEBUG], "void PhysicalTrafficManagerDebugComponent::RenderControls("),
             definition(sources[DEBUG], "void PhysicalTrafficManagerDebugComponent::RenderAirRam("),
             definition(sources[DEBUG], "void PhysicalTrafficManagerDebugComponent::RenderCone("),
             definition(sources[DEBUG], "void PhysicalTrafficManagerDebugComponent::DrawCatchupTargets("),
             definition(sources[DEBUG], "void PhysicalTrafficManagerDebugComponent::ResetTrafficAboveWater("),
             definition(sources[DEBUG], "void PhysicalTrafficManagerDebugComponent::DrawTrafficBox("),
             definition(sources[DEBUG], "void PhysicalTrafficManagerDebugComponent::DrawPhysicalTraffic("),
             definition(sources[MANAGER], "PhysicalTrafficVehicle* PhysicalTrafficManager::GetTrafficVehicle("),
             definition(sources[MANAGER], "VehicleDriver* PhysicalTrafficManager::GetTrafficDriver("),
             definition(sources[MANAGER], "PhysicalTrafficVehicle::EArticulatedVehicleType PhysicalTrafficVehicle::GetArticulatedVehicleType() const"),
             definition(sources[MANAGER], "bool PhysicalTrafficVehicle::HasNonBrokenJoint() const"),
             definition(sources[CREATE], "namespace\n{"),
             definition(sources[CREATE], "void PhysicalTrafficManager::ProcessCreateArticulatedTrafficEvents("),
             definition(sources[EVENT], "void CreateArticulatedTrafficEvent::GetCreateCabEvent("),
             definition(sources[EVENT], "void CreateArticulatedTrafficEvent::GetCreateTrailerEvent("),
             "} }", "namespace rw { namespace physics {"]
    for name in ["SetChildAngularFrame", "SetParentAngularFrame", "SetParentLinearFrame"]:
        parts.append(definition(sources[FRAMES], "void JointFrames::" + name + "("))
    parts += ["} }", "namespace CgsDev {",
              definition(sources[RENDER], "rw::math::vpu::Vector2 Debug3DImmediateRender::GetVirtualScreenSize() const"),
              definition(sources[RENDER], "const rw::math::vpu::Matrix44 Debug3DImmediateRender::GetViewProjectionMatrix() const"),
              "}"]
    include = "\n\n".join(parts)
    (output / "production_bodies.inc").write_text(include, encoding="utf-8")
    hashes = {path: hashlib.sha256((REPO / path).read_bytes()).hexdigest() for path in sources}
    hashes["fixture/ArticulatedDebugFrames.cpp"] = hashlib.sha256(
        (HERE / "ArticulatedDebugFrames.cpp").read_bytes()).hexdigest()
    hashes["fixture/production_bodies.inc"] = hashlib.sha256(include.encode()).hexdigest()
    hashes["fixture/artist_inverse_program.inc"] = hashlib.sha256(
        program.encode("utf-8")).hexdigest()
    hashes["fixture/run_articulated_debug_frames.py"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    hashes["fixture/ArtistMatrix44Inverse.asm"] = hashlib.sha256(
        (HERE / "fixtures" / "ArtistMatrix44Inverse.asm").read_bytes()).hexdigest()
    hashes["fixture/artist_quaternion_program.inc"] = hashlib.sha256(quaternion.encode("utf-8")).hexdigest()
    hashes["fixture/original_quaternion_assembly"] = hashlib.sha256(QUATERNION_ASSEMBLY.encode("utf-8")).hexdigest()
    (output / "fixture_hashes.json").write_text(json.dumps(hashes, indent=2), encoding="utf-8")
    return include


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=WORKFLOW / "scratch" / "articulated_debug_frames")
    args = parser.parse_args()
    program = inverse_program()
    quaternion = quaternion_program()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "artist_inverse_program.inc").write_text(program, encoding="utf-8")
    (output / "artist_quaternion_program.inc").write_text(quaternion, encoding="utf-8")
    include = prepare(output, program, quaternion)
    if args.prepare_only:
        print("Prepared production_bodies.inc and fixture_hashes.json; no compiler/test/game invoked.")
        return 0
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    result = compile_and_run(HERE / "ArticulatedDebugFrames.cpp", "production_bodies.inc", include,
                             "ArticulatedDebugFrames", extra_files={"artist_inverse_program.inc": program,
                                                                   "artist_quaternion_program.inc": quaternion})
    (output / "fixture_result.json").write_text(json.dumps(
        {"checks": None if result is None else result[0],
         "failures": None if result is None else result[1],
         "passed": result is not None and result[1] == 0}, indent=2), encoding="utf-8")
    return 1 if result is None or result[1] else 0


if __name__ == "__main__":
    sys.exit(main())
