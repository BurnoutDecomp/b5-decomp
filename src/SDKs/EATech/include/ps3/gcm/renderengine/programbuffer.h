#pragma once

#include "types.hpp"

// renderengine::ProgramBuffer and renderengine::ProgramVariableHandle.
//
// Platform-abstracted GCM/Cg shader program buffer from the EATech render engine.
// Source: EARenderWare/stable/platform/include/native/ps3-ppu-gcc/ps3/rw/graphics/core/programbuffer.h
// DWARF: references/DecFIGS/dwarfdump/SDKs/EATech/include/ps3/gcm/renderengine/programbuffer.h
//
// The Feb-2007 partial source uses rw::graphics::core; the X360 ARTIST / FIGS build
// collapsed it to renderengine:: (same type shapes, renamed namespace).
//
// CGprogram: Cg runtime program handle. Treated as void* for the PC compile gate
// (the real Cg library header is not available in the Windows SDK path).

typedef void* CGprogram;

namespace renderengine
{
    struct ParameterOffsetTable;    // opaque — not accessed in current TUs

    // -------------------------------------------------------------------------
    // renderengine::ProgramBuffer
    // Shader program resource object. Members from DWARF (programbuffer.h:181-191).
    // -------------------------------------------------------------------------
    struct ProgramBuffer
    {
        enum Type
        {
            TYPE_NA             = 0,
            TYPE_VERTEX         = 1,
            TYPE_PIXEL          = 2,
            TYPE_FORCEENUMSIZEINT = 0x7FFFFFFF,
        };

        static const u32 FLAG_PS3PATCHABLE = 1u;

        struct Parameters
        {
            void*  cgProgramBuffer;     // pointer to the compiled program binary
            u32    cgProgramSize;       // byte size of the binary
            Type   programType;
            u32    programFlags;        // e.g. FLAG_PS3PATCHABLE

            Parameters()
                : cgProgramBuffer(nullptr), cgProgramSize(0u)
                , programType(TYPE_NA), programFlags(0u)
            {}

            void SetType(Type t)     { programType = t; }
            void SetBuffer(void* b)  { cgProgramBuffer = b; }
            void SetSize(u32 s)      { cgProgramSize = s; }
            void SetFlags(u32 f)     { programFlags = f; }

            Type        GetType()   const { return programType; }
            const void* GetBuffer() const { return cgProgramBuffer; }
            u32         GetSize()   const { return cgProgramSize; }
            u32         GetFlags()  const { return programFlags; }
        };

        // Per-parameter lookup table entry (DWARF ProgramVariableDescriptor, programbuffer.h:164).
        struct ProgramVariableDescriptor
        {
            u16 m_index;
            u16 m_numConstants;
            u16 m_dataType;
            u16 m_cgParamIndex;
        };

    protected:
        void*                   m_programBuffer;        // ptr to compiled program binary
        u32                     m_programSize;
        u32                     m_programOffset;        // PS3 pixel-program uCode offset
        Type                    m_programType;
        u32                     m_programFlags;
        CGprogram               m_cgProgramBuffer;      // Cg program handle (void* on PC)
        u32                     m_cgProgramSize;
        u32                     m_numParameters;
        ParameterOffsetTable*   m_parameterOffsetTable;
    };

    // -------------------------------------------------------------------------
    // renderengine::ProgramVariableHandle
    // Thin handle to a shader variable (constant register or sampler).
    // Members from DWARF (programbuffer.h — ProgramVariableHandle section).
    // -------------------------------------------------------------------------
    class ProgramVariableHandle
    {
    public:
        static const u8 FLAGS_FLOAT         = 0x01u;
        static const u8 FLAGS_SAMPLER       = 0x02u;
        static const u8 FLAGS_BOOL          = 0x04u;
        static const u8 FLAGS_DATATYPEMASK  = 0x07u;
        static const u8 FLAGS_VERTEXPROGRAM = 0x80u;

        typedef u32 DataType;
        static const DataType DATATYPE_NA      = 0u;
        static const DataType DATATYPE_FLOAT   = FLAGS_FLOAT;
        static const DataType DATATYPE_SAMPLER = FLAGS_SAMPLER;
        static const DataType DATATYPE_BOOL    = FLAGS_BOOL;

        ProgramVariableHandle()
            : m_programBuffer(nullptr), m_index(0u)
            , m_numConstants(0u), m_dataFlags(0u)
        {}

        void SetProgramType(ProgramBuffer::Type programType)
        {
            if (programType == ProgramBuffer::TYPE_VERTEX)
                m_dataFlags |= FLAGS_VERTEXPROGRAM;
            else
                m_dataFlags &= static_cast<u8>(~FLAGS_VERTEXPROGRAM);
        }
        void SetDataType(DataType dataType)
        {
            m_dataFlags = (m_dataFlags & static_cast<u8>(~FLAGS_DATATYPEMASK))
                        | static_cast<u8>(dataType);
        }
        void SetNumConstants(u32 numConstants)  { m_numConstants = static_cast<u8>(numConstants); }
        void SetConstantIndex(u32 constantIndex) { m_index = static_cast<u16>(constantIndex); }
        void PS3SetProgramBuffer(const ProgramBuffer* programBuffer) { m_programBuffer = programBuffer; }

        ProgramBuffer::Type GetProgramType() const
        {
            return (m_dataFlags & FLAGS_VERTEXPROGRAM)
                ? ProgramBuffer::TYPE_VERTEX : ProgramBuffer::TYPE_PIXEL;
        }
        DataType GetDataType()     const { return static_cast<DataType>(m_dataFlags & FLAGS_DATATYPEMASK); }
        u32      GetNumConstants() const { return static_cast<u32>(m_numConstants); }
        u32      GetConstantIndex() const { return static_cast<u32>(m_index); }
        const ProgramBuffer* PS3GetProgramBuffer() const { return m_programBuffer; }

    protected:
        const ProgramBuffer* m_programBuffer;
        u16 m_index;
        u8  m_numConstants;
        u8  m_dataFlags;
    };

} // namespace renderengine
