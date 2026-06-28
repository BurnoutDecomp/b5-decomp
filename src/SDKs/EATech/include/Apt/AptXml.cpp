// ===========================================================================
// EATech Apt -- AptXml bodies.   Reconstructed from the X360 ARTIST.XEX.
//   ctor   (inlined into AptActionInterpreter::_createObject @0x82B08088)
//   `vector deleting destructor'  @ 0x82AF8E80  (compiler thunk -- dropped;
//                                                ~AptXml below is the real
//                                                destructor body)
//
// The ActionScript XML (document) object: a trivial subclass of AptXmlNode
// (`XML : XMLNode`) that adds no data members -- only its own AptVFT_Xml type tag
// and vtable. It is a garbage-collected value (AptValueGC lineage via
// AptXmlNode/AptValueWithHash), allocated from the GC value pool through the
// inherited AptXmlNode::operator new / operator delete, and it participates in
// the base's GC mark/teardown unchanged.
//
// The X360 ledger attests only the deleting-destructor thunk for this class; the
// constructor is inlined at the single construction site (_createObject), where
// the asm is: AptXmlNode::operator new(32) -> AptXmlNode base ctor -> store
// AptVFT_Xml (li r4,0x19) + the AptXml vtable (off_82145F88). That is the codegen
// of the trivial ctor below; nothing AptXml-specific beyond the re-tag exists.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptXml.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // setVtblIndex / AptVFT_Xml

// ---------------------------------------------------------------------------
// ctor (inlined into _createObject @0x82B08088)
//
// X360 (the XML branch of _createObject):
//   v = AptXmlNode::operator new(32);          // li r3, 0x20 ; bl AptXmlNode__operator_new
//   AptXmlNode::AptXmlNode(v);                  // base ctor (sets AptVFT_XmlNode, hash cap 8,
//                                              //   clears the class-flags word, base vtable)
//   v->setVtblIndex(AptVFT_Xml);               // li r4, 0x19 (25) -- re-tag as XML
//   *v = off_82145F88;                          // the AptXml vtable
//
// The base ctor tags the value AptVFT_XmlNode (24); the most-derived AptXml ctor
// then overwrites the type index with AptVFT_Xml (25). The vtable store the X360
// emits (off_82145F88) is compiler-generated as part of entering the most-derived
// ctor body and is not hand-written. AptXml owns no fields, so there is nothing
// else to initialise.
// ---------------------------------------------------------------------------
AptXml::AptXml()
    : AptXmlNode(AptVFT_Xml, 0)
{
    // The X360 builds the XML object by constructing the AptXmlNode base with the
    // XML vtable index (li r4, 0x19 = AptVFT_Xml) and hash capacity 0 (li r5, 0),
    // then storing the AptXml vtable as the most-derived ctor (compiler-emitted).
    // No re-tag is needed -- the base is built with AptVFT_Xml directly.
}

// ---------------------------------------------------------------------------
// ~AptXml -- empty.
//
// AptXml adds no members over AptXmlNode, so its destructor does only the base
// teardown: the embedded AptNativeHash (in the AptValueWithHash base) tears
// itself down via the base destructor chain. The `vector deleting destructor'
// thunk @0x82AF8E80 confirms this -- it stores the (folded base) vtable, chains
// to the base teardown, then runs AptXmlNode::operator delete(this, 32) under the
// delete flag (pinning sizeof == 0x20). The thunk is the compiler's codegen of
// `delete` and is intentionally not hand-written.
// ---------------------------------------------------------------------------
AptXml::~AptXml()
{
}
