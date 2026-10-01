#pragma once

// FLAG PC-platform leaf: D3D9 instance streams replace Xenos manual vertex
// fetch. The original shader arithmetic is retained; only the source of the
// world matrix and wheel constants changes from uniforms to instance inputs.
#include <d3d9.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>

namespace renderengine { namespace InstancingPC {

struct Program
{
    std::vector<DWORD> words;
    std::vector<D3DVERTEXELEMENT9> elements;
    unsigned worldRegister = 0;
    unsigned wheelRegister = ~0u;
    unsigned fields = 4;
};

inline unsigned RegisterType(DWORD token)
{
    return ((token & D3DSP_REGTYPE_MASK) >> D3DSP_REGTYPE_SHIFT)
        | ((token & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2);
}

inline DWORD RegisterBits(unsigned type, unsigned index)
{
    return 0x80000000u | (index & D3DSP_REGNUM_MASK)
        | ((type << D3DSP_REGTYPE_SHIFT) & D3DSP_REGTYPE_MASK)
        | ((type << D3DSP_REGTYPE_SHIFT2) & D3DSP_REGTYPE_MASK2);
}

inline unsigned MatrixRows(unsigned opcode)
{
    switch (opcode) {
    case D3DSIO_M4x4: case D3DSIO_M3x4: return 4;
    case D3DSIO_M4x3: case D3DSIO_M3x3: return 3;
    case D3DSIO_M3x2: return 2;
    default: return 0;
    }
}

inline bool Constant(const DWORD* words, size_t count, const char* name,
                     unsigned requiredCount, unsigned& result)
{
    for (size_t at = 1; at < count; )
    {
        const DWORD token = words[at];
        const unsigned opcode = token & D3DSI_OPCODE_MASK;
        if (opcode == D3DSIO_END) break;
        const size_t length = opcode == D3DSIO_COMMENT
            ? (token >> 16) & 0x7fffu : (token >> 24) & 0x0fu;
        if (length >= count - at) return false;
        if (opcode == D3DSIO_COMMENT && length >= 8 && words[at + 1] == 0x42415443u)
        {
            const auto* table = reinterpret_cast<const unsigned char*>(words + at + 2);
            const size_t bytes = (length - 1) * sizeof(DWORD);
            DWORD constants, info;
            std::memcpy(&constants, table + 12, 4);
            std::memcpy(&info, table + 16, 4);
            if (info > bytes || constants > (bytes - info) / 20) return false;
            for (DWORD i = 0; i < constants; ++i)
            {
                const auto* entry = table + info + 20 * i;
                DWORD offset;
                WORD set, index, registers;
                std::memcpy(&offset, entry, 4);
                std::memcpy(&set, entry + 4, 2);
                std::memcpy(&index, entry + 6, 2);
                std::memcpy(&registers, entry + 8, 2);
                if (offset >= bytes || !std::memchr(table + offset, 0, bytes - offset)) continue;
                if (set == 2 && registers == requiredCount
                    && std::strcmp(reinterpret_cast<const char*>(table + offset), name) == 0)
                {
                    result = index;
                    return true;
                }
            }
        }
        at += length + 1;
    }
    return false;
}

// The five values occupy free input/temporary registers. Loading inputs into
// temporaries first preserves vs_3_0's one-input-register-per-instruction rule.
inline bool BuildProgram(const DWORD* source, size_t count,
                         const D3DVERTEXELEMENT9* declaration, size_t elements,
                         Program& output)
{
    if (!source || count < 2 || source[0] != D3DVS_VERSION(3, 0)
        || !declaration || elements < 2 || elements > MAXD3DDECLLENGTH + 1)
        return false;
    Program result;
    if (!Constant(source, count, "world", 4, result.worldRegister)) return false;
    if (Constant(source, count, "g_wheelConstants", 1, result.wheelRegister)) result.fields = 5;
    if (result.worldRegister > 252 || (result.wheelRegister != ~0u
        && (result.wheelRegister > 255 || (result.wheelRegister >= result.worldRegister
            && result.wheelRegister < result.worldRegister + 4)))) return false;

    unsigned usedTexcoords = 0;
    for (size_t i = 0; i + 1 < elements; ++i)
    {
        if (declaration[i].Stream != 0) return false;
        if (declaration[i].Usage == D3DDECLUSAGE_TEXCOORD)
            usedTexcoords |= 1u << declaration[i].UsageIndex;
    }
    if (declaration[elements - 1].Stream != 0xff) return false;

    const auto fieldOf = [&result](unsigned index) -> int {
        if (index >= result.worldRegister && index < result.worldRegister + 4)
            return static_cast<int>(index - result.worldRegister);
        return index == result.wheelRegister ? 4 : -1;
    };
    struct MatrixUse { size_t operand; unsigned base, rows, temporary; int firstField; bool packed; };
    std::vector<MatrixUse> matrices;
    unsigned inputs = 0, temps = 0;
    size_t firstInstruction = 0, end = 0;
    for (size_t at = 1; at < count; )
    {
        const DWORD token = source[at];
        const unsigned opcode = token & D3DSI_OPCODE_MASK;
        if (opcode == D3DSIO_END) { end = at; break; }
        const size_t length = opcode == D3DSIO_COMMENT
            ? (token >> 16) & 0x7fffu : (token >> 24) & 0x0fu;
        if (length >= count - at) return false;
        const bool definition = opcode == D3DSIO_DEF || opcode == D3DSIO_DEFI || opcode == D3DSIO_DEFB;
        if (opcode != D3DSIO_COMMENT && opcode != D3DSIO_DCL && !definition && !firstInstruction)
            firstInstruction = at;
        if (opcode == D3DSIO_DCL && length == 2
            && RegisterType(source[at + 2]) == D3DSPR_INPUT
            && (source[at + 1] & D3DSP_DCL_USAGE_MASK) == D3DDECLUSAGE_TEXCOORD)
            usedTexcoords |= 1u << ((source[at + 1] & D3DSP_DCL_USAGEINDEX_MASK) >> D3DSP_DCL_USAGEINDEX_SHIFT);
        if (opcode != D3DSIO_COMMENT)
        {
            for (size_t p = opcode == D3DSIO_DCL ? 2 : 1; p <= length; ++p)
            {
                if (definition && p > 1) break; // immediate words are not register operands
                const DWORD operand = source[at + p];
                const unsigned type = RegisterType(operand), index = operand & D3DSP_REGNUM_MASK;
                if (type == D3DSPR_TEMP && index + 1 > temps) temps = index + 1;
                if (type == D3DSPR_INPUT && index + 1 > inputs) inputs = index + 1;
                // Relative uniform/input addressing cannot be remapped safely.
                if ((type == D3DSPR_CONST || type == D3DSPR_INPUT)
                    && (operand & D3DSHADER_ADDRESSMODE_MASK)) return false;
                if (definition && type == D3DSPR_CONST
                    && ((index >= result.worldRegister && index < result.worldRegister + 4)
                        || index == result.wheelRegister)) return false;
            }
        }
        // Matrix opcodes read consecutive registers beyond their one encoded
        // matrix operand. Include that span in occupancy and remapping.
        const unsigned rows = MatrixRows(opcode);
        if (rows)
        {
            const size_t parameter = 3 + ((token & D3DSHADER_INSTRUCTION_PREDICATED) ? 1 : 0);
            if (parameter > length) return false;
            const DWORD operand = source[at + parameter];
            const unsigned type = RegisterType(operand), base = operand & D3DSP_REGNUM_MASK;
            if (type == D3DSPR_TEMP && base + rows > temps) temps = base + rows;
            if (type == D3DSPR_INPUT && base + rows > inputs) inputs = base + rows;
            if (type == D3DSPR_CONST)
            {
                if (base + rows > 256) return false;
                const int first = fieldOf(base);
                bool affected = false, consecutive = first >= 0;
                for (unsigned row = 0; row < rows; ++row)
                {
                    const int field = fieldOf(base + row);
                    affected = affected || field >= 0;
                    consecutive = consecutive && field == first + static_cast<int>(row);
                }
                if (affected) matrices.push_back({at + parameter, base, rows, 0, first, !consecutive});
            }
        }
        at += length + 1;
    }
    if (!end || !firstInstruction || inputs + result.fields > 16 || temps + result.fields > 32)
        return false;
    unsigned nextTemp = temps + result.fields;
    for (auto& matrix : matrices)
    {
        if (matrix.packed) { matrix.temporary = nextTemp; nextTemp += matrix.rows; }
        else matrix.temporary = temps + static_cast<unsigned>(matrix.firstField);
    }
    if (nextTemp > 32) return false;
    unsigned semantic[5] = {};
    for (unsigned field = 0; field < result.fields; ++field)
    {
        unsigned index = 0;
        while (index < 16 && (usedTexcoords & (1u << index))) ++index;
        if (index == 16) return false;
        semantic[field] = index;
        usedTexcoords |= 1u << index;
    }

    result.words.assign(source, source + firstInstruction);
    for (unsigned field = 0; field < result.fields; ++field)
    {
        result.words.push_back((2u << 24) | D3DSIO_DCL);
        result.words.push_back(0x80000000u | D3DDECLUSAGE_TEXCOORD
            | (semantic[field] << D3DSP_DCL_USAGEINDEX_SHIFT));
        result.words.push_back(RegisterBits(D3DSPR_INPUT, inputs + field) | D3DSP_WRITEMASK_ALL);
    }
    for (unsigned field = 0; field < result.fields; ++field)
    {
        result.words.push_back((2u << 24) | D3DSIO_MOV);
        result.words.push_back(RegisterBits(D3DSPR_TEMP, temps + field) | D3DSP_WRITEMASK_ALL);
        result.words.push_back(RegisterBits(D3DSPR_INPUT, inputs + field) | D3DSP_NOSWIZZLE);
    }
    // Mixed spans (for example c19..c22 when world occupies c20..c23)
    // need a contiguous temporary pack containing both original uniforms and
    // the per-instance fields. Uniforms are immutable during shader execution.
    for (const auto& matrix : matrices)
    {
        if (!matrix.packed) continue;
        for (unsigned row = 0; row < matrix.rows; ++row)
        {
            const int field = fieldOf(matrix.base + row);
            result.words.push_back((2u << 24) | D3DSIO_MOV);
            result.words.push_back(RegisterBits(D3DSPR_TEMP, matrix.temporary + row) | D3DSP_WRITEMASK_ALL);
            result.words.push_back((field >= 0 ? RegisterBits(D3DSPR_TEMP, temps + field)
                : RegisterBits(D3DSPR_CONST, matrix.base + row)) | D3DSP_NOSWIZZLE);
        }
    }
    for (size_t at = firstInstruction; at < end; )
    {
        const unsigned opcode = source[at] & D3DSI_OPCODE_MASK;
        const size_t length = opcode == D3DSIO_COMMENT
            ? (source[at] >> 16) & 0x7fffu : (source[at] >> 24) & 0x0fu;
        result.words.push_back(source[at]);
        for (size_t p = 1; p <= length; ++p)
        {
            DWORD operand = source[at + p];
            if (opcode != D3DSIO_COMMENT && RegisterType(operand) == D3DSPR_CONST)
            {
                const unsigned index = operand & D3DSP_REGNUM_MASK;
                const int field = fieldOf(index);
                unsigned temporary = field >= 0 ? temps + field : ~0u;
                for (const auto& matrix : matrices)
                    if (matrix.operand == at + p) { temporary = matrix.temporary; break; }
                if (temporary != ~0u)
                    operand = (operand & ~(D3DSP_REGTYPE_MASK | D3DSP_REGTYPE_MASK2 | D3DSP_REGNUM_MASK))
                        | RegisterBits(D3DSPR_TEMP, temporary);
            }
            result.words.push_back(operand);
        }
        at += length + 1;
    }
    result.words.push_back(D3DSIO_END);
    result.elements.assign(declaration, declaration + elements - 1);
    for (unsigned field = 0; field < result.fields; ++field)
        result.elements.push_back({1, static_cast<WORD>(field * 16), D3DDECLTYPE_FLOAT4,
            D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, static_cast<BYTE>(semantic[field])});
    result.elements.push_back(D3DDECL_END());
    output = std::move(result);
    return true;
}

class Cache
{
    struct Key { IDirect3DVertexShader9* shader; IDirect3DVertexDeclaration9* declaration;
        bool operator==(const Key& rhs) const { return shader == rhs.shader && declaration == rhs.declaration; } };
    struct Hash { size_t operator()(const Key& key) const {
        return (reinterpret_cast<uintptr_t>(key.shader) >> 4) ^ (reinterpret_cast<uintptr_t>(key.declaration) << 1); } };
    struct Variant
    {
        Key original{};
        Program program;
        IDirect3DVertexShader9* shader = nullptr;
        IDirect3DVertexDeclaration9* declaration = nullptr;
        ~Variant() { if (shader) shader->Release(); if (declaration) declaration->Release();
            if (original.shader) original.shader->Release(); if (original.declaration) original.declaration->Release(); }
    };
    std::unordered_map<Key, std::unique_ptr<Variant>, Hash> variants;
    IDirect3DDevice9* device = nullptr;
    IDirect3DVertexBuffer9* buffer = nullptr;
    Variant* active = nullptr;
    unsigned cursor = 0;
    bool available = false;
    static constexpr unsigned capacity = 65536;

    Variant* Prepare(IDirect3DVertexShader9* shader, IDirect3DVertexDeclaration9* declaration)
    {
        const Key key{shader, declaration};
        auto found = variants.find(key);
        if (found != variants.end()) return found->second.get();
        UINT bytes = 0, elements = 0;
        if (FAILED(shader->GetFunction(nullptr, &bytes)) || bytes < 8 || bytes > 262144 || bytes % 4
            || FAILED(declaration->GetDeclaration(nullptr, &elements)) || elements > MAXD3DDECLLENGTH + 1)
            return nullptr;
        std::vector<DWORD> words(bytes / 4);
        std::vector<D3DVERTEXELEMENT9> layout(elements);
        if (FAILED(shader->GetFunction(words.data(), &bytes))
            || FAILED(declaration->GetDeclaration(layout.data(), &elements))) return nullptr;
        auto variant = std::make_unique<Variant>();
        variant->original = key;
        shader->AddRef(); declaration->AddRef();
        if (BuildProgram(words.data(), words.size(), layout.data(), layout.size(), variant->program))
        {
            if (FAILED(device->CreateVertexShader(variant->program.words.data(), &variant->shader))
                || FAILED(device->CreateVertexDeclaration(variant->program.elements.data(), &variant->declaration)))
                return nullptr; // creation failure may be transient; permit a later retry
        }
        auto* result = variant.get();
        variants.emplace(key, std::move(variant));
        return result;
    }
public:
    Cache() = default;
    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;
    ~Cache() { Release(); }
    void Release()
    {
        End(); variants.clear();
        if (buffer) { buffer->Release(); buffer = nullptr; }
        if (device) { device->Release(); device = nullptr; }
        cursor = 0; available = false;
    }
    void RetireDeclaration(IDirect3DVertexDeclaration9* declaration)
    {
        End();
        for (auto at = variants.begin(); at != variants.end(); )
            if (at->first.declaration == declaration) at = variants.erase(at); else ++at;
    }
    // Caller owns stream1 and enters with both stream frequencies at1. Matrices
    // contain the original five-entry constant6 array; index.w selects its row.
    bool Begin(IDirect3DDevice9* target, IDirect3DVertexShader9* shader,
               IDirect3DVertexDeclaration9* declaration, const float* matrices,
               const float* indices, unsigned count)
    {
        if (!target || !shader || !declaration || !matrices || count < 2 || count > 5 || active) return false;
        if (device != target)
        {
            Release(); device = target; device->AddRef();
            D3DCAPS9 caps{};
            available = SUCCEEDED(device->GetDeviceCaps(&caps)) && caps.VertexShaderVersion >= D3DVS_VERSION(3,0)
                && caps.MaxStreams >= 2 && (caps.DevCaps2 & D3DDEVCAPS2_STREAMOFFSET) != 0;
        }
        if (!available) return false;
        Variant* variant = Prepare(shader, declaration);
        if (!variant || !variant->shader || !variant->declaration) return false;
        float data[5 * 20] = {};
        const unsigned stride = variant->program.fields * 16;
        for (unsigned i = 0; i < count; ++i)
        {
            const float matrixIndex = indices ? indices[i * 4 + 3] : static_cast<float>(i);
            if (!std::isfinite(matrixIndex) || matrixIndex < 0 || matrixIndex >= 5) return false;
            std::memcpy(data + i * variant->program.fields * 4, matrices + static_cast<unsigned>(matrixIndex) * 16, 64);
            if (variant->program.fields == 5 && indices)
                std::memcpy(data + i * 20 + 16, indices + i * 4, 16);
        }
        if (!buffer && FAILED(device->CreateVertexBuffer(capacity, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
            0, D3DPOOL_DEFAULT, &buffer, nullptr))) return false;
        const unsigned bytes = count * stride;
        if (cursor + bytes > capacity) cursor = 0;
        void* destination = nullptr;
        if (FAILED(buffer->Lock(cursor, bytes, &destination, cursor ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD))) return false;
        std::memcpy(destination, data, bytes);
        if (FAILED(buffer->Unlock())) return false;
        active = variant;
        const HRESULT stream = device->SetStreamSource(1, buffer, cursor, stride);
        cursor += bytes;
        if (FAILED(stream) || FAILED(device->SetStreamSourceFreq(0, D3DSTREAMSOURCE_INDEXEDDATA | count))
            || FAILED(device->SetStreamSourceFreq(1, D3DSTREAMSOURCE_INSTANCEDATA | 1))
            || FAILED(device->SetVertexDeclaration(variant->declaration))
            || FAILED(device->SetVertexShader(variant->shader))) { End(); return false; }
        return true;
    }
    void End()
    {
        if (!active || !device) return;
        device->SetStreamSourceFreq(0, 1);
        device->SetStreamSourceFreq(1, 1);
        device->SetStreamSource(1, nullptr, 0, 0);
        device->SetVertexDeclaration(active->original.declaration);
        device->SetVertexShader(active->original.shader);
        active = nullptr;
    }
};

} }
