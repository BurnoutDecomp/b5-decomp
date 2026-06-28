#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptXml.
//
// The ActionScript XML (document) object value. In ECMAScript/AS2 the XML
// document class extends XMLNode (`XML : XMLNode`), so AptXml derives from
// AptXmlNode: it is the SAME property-bearing AS object (attributes /
// childNodes / nodeName / nodeValue all live in the inherited property hash)
// and adds NO data members of its own -- only its own type tag + vtable. The
// XML-document behaviour (parseXML / load / send / the doctype/xmlDecl
// accessors) is driven by the ActionScript interpreter; in the X360 ARTIST
// build none of those bodies are emitted out-of-line for this class, so this is
// an honest minimal owning header (see "SHAPE" below).
//
// AptActionInterpreter::_createObject @0x82B08088 materialises one for AS
// `new XML(...)`: it allocates via the inherited AptXmlNode::operator new(32),
// runs the AptXmlNode base ctor, then patches in the AptXml type tag
// (AptVFT_Xml) and vtable -- exactly the codegen of `class AptXml : AptXmlNode`
// with a trivial ctor + destructor.
//
// SHAPE: no Feb-2007 / DecFIGS header is in scope for this class, so everything
// is recovered from the X360 ARTIST.XEX. The X360 ledger attests exactly ONE
// AptXml function -- the compiler-generated deleting-destructor thunk -- plus
// the construction site in _createObject:
//     AptXml::`vector deleting destructor'   @ 0x82AF8E80  (compiler thunk -- dropped)
//     (construction inlined into _createObject @0x82B08088:
//        AptXmlNode::operator new(32)  ->  base ctor  ->  set AptVFT_Xml + vtable)
//
// LAYOUT (sizeof = 32 / 0x20, pinned by `operator delete(this, 32)` / `li r4,
// 0x20` in the deleting-destructor thunk and `li r3, 0x20` before the
// operator new in _createObject):
//   AptXmlNode base .................. 32 bytes (AptValueWithHash + mClassFlags)
//   (no AptXml members)
//
// vtable object-type index = AptVFT_Xml (25), confirmed by `li r4, 0x19` (25) on
// the construction path; the distinct AptXml vtable is off_82145F88 (vs the
// AptXmlNode base vtable off_82145E54). AptXml therefore has its OWN vtable
// (given by the out-of-line virtual destructor below).
//
// GC: AptXml inherits the GC pool new/delete from AptXmlNode -- the
// construction site calls AptXmlNode::operator new(32) and the deleting
// destructor calls AptXmlNode::operator delete(this, 32), so AptXml declares
// NEITHER operator (no AptXml-specific allocator exists in the X360 ledger).
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptXml, the AptVFT_* index)
// are an external/middleware API and kept verbatim.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptXmlNode.h"   // AptXmlNode base + AptVFT_Xml
                                                  // (transitively: AptValueWithHash,
                                                  //  AptValue, AptNativeString)

class AptXml : public AptXmlNode
{
public:
    // @ inlined into AptActionInterpreter::_createObject (no standalone X360
    // symbol) -- construct an empty XML document. Runs the AptXmlNode base ctor
    // (which sets the property-hash capacity + clears the class-flags word) and
    // then re-tags this value as AptVFT_Xml (the base ctor tags it AptVFT_XmlNode;
    // the X360 patches the type index + the AptXml vtable after the base call).
    AptXml();

protected:
    // The `vector deleting destructor' thunk @0x82AF8E80 stores the (folded base)
    // vtable, chains to the base teardown, then -- with the delete flag --
    // AptXmlNode::operator delete(this, 32). It is a compiler artifact and is
    // dropped; this is the real (empty) destructor body. AptXml adds nothing over
    // the base destructor (the embedded AptNativeHash in the AptValueWithHash base
    // destroys itself), which is exactly why the thunk needs only the base
    // teardown. Declared out-of-line so AptXml gets its own vtable (off_82145F88).
    virtual ~AptXml();
};
