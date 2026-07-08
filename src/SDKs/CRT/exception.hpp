#pragma once
//
// std::exception — the C++ runtime exception base shipped in the X360 build's
// Microsoft/EA CRT.  Reconstructed from the X360 ARTIST asm (0x82C08C08 ctor,
// 0x82C08CA0 what, 0x82C08CB8 [vector] deleting destructor).
//
// X360 (32-bit) object layout:
//   +0x00  vtable            (virtual ~exception, virtual what)
//   +0x04  const char* mpWhat    (MS CRT _mywhat)   — the message string
//   +0x08  int          miDoFree (MS CRT _mydofree) — non-zero => mpWhat is owned
//
// This is a self-contained reconstruction of the CRT base; it deliberately does
// NOT include the host toolchain's <exception>, which would provide a competing
// definition of std::exception.
//
namespace std {

class exception {
public:
    exception(const exception& rOther) noexcept;
    virtual ~exception() noexcept;

    virtual const char* what() const noexcept;

private:
    const char* mpWhat;
    int         miDoFree;
};

} // namespace std
