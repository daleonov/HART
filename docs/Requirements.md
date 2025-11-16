@page Requirements Requirements

The requirements to use this library are following:
    * A C++11-compatible compiler

That's all! 

 * It doesn't have any heavyweight dependencies

 * It doesn't require you you to use any specific framework like JUCE (although it can coexist happily with it)

 * It's a standalone audio test solution, not an extension to frameworks like Google Test (gtest) or Catch2 (but it can coexist with them as well)

 * It's a header-only library, so it doesn't require any complicated build system set up

 * It's platform-independent, and not attached to a specific compiler

There are two libraries it depends on, both are included in the repo:

 * [dr_wav](https://github.com/mackron/dr_libs) for working with wav

 * [CLI11](https://github.com/CLIUtils/CLI11) for parsing CLI arguments

Both are header-only libraries as well, and compatible with C++11.
