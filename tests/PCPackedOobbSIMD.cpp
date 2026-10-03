#include "GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.h"
#include "pc/geometric/PackedOobbSIMDPCLeaf.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <vector>

using Matrix = rw::math::vpu::Matrix44;
using Box = CgsGraphics::PackedOobb;
namespace CgsGraphics { namespace Reference {
struct PackedOobb
{
    CgsGraphics::PackedOobb::PackedRegister mPackedBB;
    void ToMatrix(Matrix&) const;
    void MultiplyByMatrix(const Matrix&, Matrix&) const;
};
} }
#include "pc_packed_oobb_scalar.inc"
using ReferenceBox = CgsGraphics::Reference::PackedOobb;
struct Case { Box box; Matrix input; };
static unsigned checks, failures;
static void Check(bool ok, const char* name)
{ ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", name); } }
static u32 Bits(float value) { u32 bits; std::memcpy(&bits, &value, 4); return bits; }
static bool Equal(const Matrix& a, const Matrix& b)
{
    float x[16], y[16]; std::memcpy(x, &a, 64); std::memcpy(y, &b, 64);
    for (unsigned i = 0; i != 16; ++i)
        if (Bits(x[i]) != Bits(y[i]) && !(std::isnan(x[i]) && std::isnan(y[i]))) return false;
    return true;
}
static ReferenceBox Reference(const Box& box) { return { box.mPackedBB }; }
__declspec(noinline) static void Scalar(const Box& box, Matrix& out) { Reference(box).ToMatrix(out); }
__declspec(noinline) static void Simd(const Box& box, Matrix& out) { CgsGraphics::PackedOobbPC::ToMatrix(box.mPackedBB, out); }
__declspec(noinline) static void Public(const Box& box, Matrix& out) { box.ToMatrix(out); }
__declspec(noinline) static void ScalarProduct(const Case& c, Matrix& out)
{ Reference(c.box).MultiplyByMatrix(c.input, out); }
__declspec(noinline) static void SimdProduct(const Case& c, Matrix& out)
{ Matrix box; Simd(c.box, box); CgsGraphics::PackedOobbPC::Multiply(box, c.input, out); }
__declspec(noinline) static void PublicProduct(const Case& c, Matrix& out)
{ c.box.MultiplyByMatrix(c.input, out); }
static Matrix Input()
{
    // Projective components deliberately exercise the complete four-row product.
    Matrix m{};
    m.xAxis = { 2, 3, 4, .25f }; m.yAxis = { -3, 1, 5, .5f };
    m.zAxis = { 1, -4, 2, .75f }; m.wAxis = { 10, 20, 30, 2 };
    return m;
}
static u32 state = 0x8134abcd;
static u32 Next() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
static float Random() { return (float(Next() & 65535) / 32768.0f - 1.0f) * 8; }

int main()
{
#ifdef PC_PACKED_SCALAR_CONTROL
    Check(!CgsGraphics::PackedOobbPC::Enabled(), "startup scalar comparison control is active");
#else
    Check(CgsGraphics::PackedOobbPC::Enabled(), "shipping default selects the vector path");
#endif
    Box box{{0, 0, 0x7f804020u, 0x0000007fu}};
    Matrix matrix, reference;
    Simd(box, matrix);
    const float sx = float(0x808080) / 16777216.0f;
    const float sy = float(0x404040) / 16777216.0f;
    const float sz = float(0x202020) / 16777216.0f;
    Check(matrix.xAxis.x == sx && matrix.yAxis.y == sy && matrix.zAxis.z == sz
          && matrix.xAxis.y == 0 && matrix.yAxis.z == 0 && matrix.zAxis.x == 0,
          "identity quaternion and independent scale bytes decode analytically");
    box.mPackedBB.mauLane[3] = 0x7f000000;
    Simd(box, matrix);
    Check(matrix.xAxis.x == sx && matrix.yAxis.y == -sy && matrix.zAxis.z == -sz,
          "quaternion byte order preserves a half-turn about X");
    box.mPackedBB.mauLane[0] = 0x40808000;
    box.mPackedBB.mauLane[1] = 0x4000c000;
    Simd(box, matrix);
    Check(matrix.wAxis.x == -3.99993896484375f && matrix.wAxis.y == 2.000030517578125f
          && matrix.wAxis.z == -1.999908447265625f && matrix.wAxis.w == 1,
          "signed repeated halfwords use their authored position exponent");
    Matrix input = Input();
    Reference(box).MultiplyByMatrix(input, reference);
    Matrix alias = input;
    box.MultiplyByMatrix(alias, alias);
    Check(Equal(reference, alias), "public multiply preserves original in-place input snapshot");
    CgsGraphics::PackedOobbPC::Multiply(matrix, input, input);
    Check(Equal(reference, input), "SIMD multiply snapshots all four input rows before writing");
    for (unsigned special = 0; special != 2; ++special)
    {
        input = Input();
        if (special == 0) input.wAxis.x = std::numeric_limits<float>::infinity();
        else input.zAxis.y = std::numeric_limits<float>::quiet_NaN();
        Reference(box).MultiplyByMatrix(input, reference);
        Matrix simd, actual;
        CgsGraphics::PackedOobbPC::Multiply(matrix, input, simd);
        box.MultiplyByMatrix(input, actual);
        Check(Equal(reference, simd) && Equal(reference, actual)
              && (special == 0 ? std::isnan(actual.xAxis.x) : std::isnan(actual.xAxis.y)),
              "zero-weight products retain non-finite input propagation");
    }
    box = {};
    Simd(box, matrix); Scalar(box, reference);
    Check(Equal(reference, matrix) && std::isnan(matrix.xAxis.x)
          && Bits(matrix.xAxis.w) == 0 && Bits(matrix.yAxis.w) == 0
          && Bits(matrix.zAxis.w) == 0 && matrix.wAxis.w == 1,
          "zero quaternion remains unguarded with exact homogeneous output lanes");

    std::vector<Case> cases; cases.reserve(40000);
    for (unsigned n = 0; n != 40000; ++n)
    {
        Case c{};
        for (auto& word : c.box.mPackedBB.mauLane) word = Next();
        if (n & 1)
        {
            c.box.mPackedBB.mauLane[0] = (c.box.mPackedBB.mauLane[0] & 0x807fffffu)
                | ((125u + Next() % 16u) << 23);
            c.box.mPackedBB.mauLane[2] = (c.box.mPackedBB.mauLane[2] & 0xffffffu)
                | ((124u + Next() % 13u) << 24);
        }
        if (n % 128 == 0) c.box.mPackedBB.mauLane[3] = 0;
        float fields[16]; for (auto& field : fields) field = Random();
        std::memcpy(&c.input, fields, sizeof(fields));
        cases.push_back(c);
    }
    const unsigned oldCsr = _mm_getcsr();
    for (unsigned mode : {0x1f80u, 0x9fc0u})
    {
        _mm_setcsr(mode);
        unsigned decodeMismatches = 0, multiplyMismatches = 0;
        for (const auto& c : cases)
        {
            Matrix expected, simd, actual;
            Scalar(c.box, expected); Simd(c.box, simd); Public(c.box, actual);
            decodeMismatches += !Equal(expected, simd) || !Equal(expected, actual);
            Matrix expectedProduct, simdProduct, publicProduct;
            Reference(c.box).MultiplyByMatrix(c.input, expectedProduct);
            CgsGraphics::PackedOobbPC::Multiply(simd, c.input, simdProduct);
            c.box.MultiplyByMatrix(c.input, publicProduct);
            Matrix inPlace = c.input;
            c.box.MultiplyByMatrix(inPlace, inPlace);
            multiplyMismatches += !Equal(expectedProduct, simdProduct)
                || !Equal(expectedProduct, publicProduct) || !Equal(expectedProduct, inPlace);
        }
        std::printf("mxcsr=%04x cases=%zu decode_mismatches=%u multiply_mismatches=%u\n",
                    mode, cases.size(), decodeMismatches, multiplyMismatches);
        Check(decodeMismatches == 0, "packed decode preserves finite bits and NaN classification");
        Check(multiplyMismatches == 0, "full projective multiply and in-place calls preserve output bits");
    }
    _mm_setcsr(oldCsr);
#ifdef PC_PACKED_BENCHMARK
    for (unsigned repeat = 0; repeat != 3; ++repeat)
        for (auto run : {Scalar, Simd, Public})
        {
            volatile u32 checksum = 2166136261u;
            const auto begin = std::chrono::steady_clock::now();
            Matrix out;
            for (unsigned n = 0; n != 1000000; ++n)
            {
                run(cases[n % cases.size()].box, out);
                checksum = (checksum ^ Bits(out.xAxis.x) ^ Bits(out.wAxis.y)) * 16777619u;
            }
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
            std::printf("decode %s repeat=%u ms=%.3f checksum=%u\n",
                        run == Scalar ? "scalar" : run == Simd ? "simd" : "public", repeat, ms, unsigned(checksum));
        }
    for (unsigned repeat = 0; repeat != 3; ++repeat)
        for (auto run : {ScalarProduct, SimdProduct, PublicProduct})
        {
            volatile u32 checksum = 2166136261u;
            const auto begin = std::chrono::steady_clock::now();
            Matrix out;
            for (unsigned n = 0; n != 1000000; ++n)
            {
                run(cases[n % cases.size()], out);
                checksum = (checksum ^ Bits(out.xAxis.x) ^ Bits(out.wAxis.y)) * 16777619u;
            }
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
            std::printf("product %s repeat=%u ms=%.3f checksum=%u\n",
                        run == ScalarProduct ? "scalar" : run == SimdProduct ? "simd" : "public", repeat, ms, unsigned(checksum));
        }
#endif
    std::printf("PCPackedOobbSIMD: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
