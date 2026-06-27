#pragma once

// ===========================================================================
//  MixerMemBase -- the EATech audio-mixer heap-allocation base class shared by
//  the NFS mix system (NFSMixMaster / NFSMixMap / NFSMixMapState) and parts of
//  the Nicotine snapshot mixer.
//
//  ProStreet08Milestone.pdb shows `class MixerMemBase [sizeof = 1] {}` -- a
//  data-less base whose only role is to route operator new/delete through the
//  mixer's own allocator. Modelled as an EMPTY base so every derived class is
//  empty-base-optimised and its first real member lands at +0x00, exactly as the
//  X360 layout (verified: NFSMixMaster's first member m_pMainMixMap is at +0x00
//  with the MixerMemBase base contributing 0 bytes).
//
//  Global type (no namespace), matching the EA mangled names (e.g.
//  ??0NFSMixMaster ... : public MixerMemBase). The allocator-routing operator
//  new/delete are provided by the owning mixer TU; not modelled here (no data).
// ===========================================================================

class MixerMemBase
{
};
