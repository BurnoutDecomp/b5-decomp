#ifndef BRN_DIRECTOR_MODULE_H
#define BRN_DIRECTOR_MODULE_H

#include "types.hpp"

// BrnDirector::DirectorModule — the top-level director engine module.
//
// It derives from CgsModule::ModuleSingleBuffered (as declared at the BrnGameModule member
// site) and owns a DirectorResourceManager, a MainDirector and a ReplayDirector sub-object,
// plus several intrusive list/tree heads and handle fields. The full multi-megabyte layout
// is not yet modelled: only the constructor (BrnDirectorModule.cpp) is reconstructed so far,
// and it addresses fields by byte offset rather than as named members. This declaration
// exposes just the constructor; grow the class body (real bases + named members) here as more
// DirectorModule functions are reconstructed.
//
// NOTE: BrnGameModule.hpp currently carries an empty placeholder
//   `namespace BrnDirector { class DirectorModule : public CgsModule::ModuleSingleBuffered {}; }`
// as its mDirectorModule member type. That placeholder should be replaced with an include of
// this header once the layout here is fleshed out — left untouched for now (another effort
// owns BrnGameModule).

namespace BrnDirector
{

class DirectorModule
{
public:
    DirectorModule();
};

}

#endif
