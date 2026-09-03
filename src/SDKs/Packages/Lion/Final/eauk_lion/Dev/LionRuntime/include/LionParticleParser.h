#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleParser.h
//
// The four Lion member-token tables the particle runtime shares. DecFIGS DWARF
// LionParticleParser.cpp:132-141 names all four and puts them in this file's TU;
// cLionParticleParser::GetpDescriptorTokenTable / GetpBehaviourTokenTable /
// GetpMaterialTokenTable / GetpWaveFormTokenTable (LionParticleParser.h:30-39) are
// the accessors that hand them out.
//
// Each table is a null-terminated sLionMemberToken array giving one Lion struct's
// fields -- value type, flag bit, byte offset and authoring name. They drive both the
// text .lef parser (not reconstructed) and cLionTokenTable::EndianTwiddle, which is
// what cParticleDescriptor / cParticleBehaviour / cParticleMaterial::Delocate use to
// byte-swap a saved effect for a different-endian target.
//
// Bodies (the table data itself, transcribed from the X360 .rdata) live in
// LionParticleParser.cpp; the X360 addresses are in that file's banner.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.h"

// cParticleDescriptor's 22 member tokens   (X360 cLionTokenTable @0x82F36A34)
extern const cLionTokenTable gLionParticleParserDesTokenTable;

// cParticleBehaviour's 88 member tokens    (X360 cLionTokenTable @0x82F36A38)
extern const cLionTokenTable gLionParticleParserBehTokenTable;

// cParticleMaterial's 46 member tokens     (X360 cLionTokenTable @0x82F36A3C)
extern const cLionTokenTable gLionParticleParserMatTokenTable;

// cParticleWaveForm's 13 member tokens     (X360 cLionTokenTable @0x82F36A40)
extern const cLionTokenTable gLionParticleParserWaveTokenTable;
