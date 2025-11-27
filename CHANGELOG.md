# Major HART updates

## 0.2.0

### What's new?

* Ensured support for all major platforms and compilers. GCC, Clang, Apple Clang, MSVC. Windows, macOS, Linux. AMD64 and ARM64. Plus, test build targets for C++ 11, 14, 17 and 20.

* Signals: Added skipTo() for fast-forwarding the signals

* Signals: Added ability to mix multiple signals together with "+" and "-" operators

* Signals: Implemented flipping signal's polarity by using unary "-" and "~" operators

* DSP: Added Mute DSP effect

* DSP: Effects can now be applied to specific channels (apply to all channels by default)

* Matchers: Matchers can now be applied to specific channels (apply to all channels by default)

* Docs: Multiple docs fixes and updates

### Breaking changes

All 3 major classes (DSP, Matcher and Signal) now use CRTP, so require slightly different template args for subclassing.

## 0.1.0

Intitial release
