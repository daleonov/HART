@page SettingUpYourProjectToUseHART Setting Up Your Project To Use HART

# Recommended project structure

To use HART for testing your audio, set up a separate console app target in your project.

A separate target is recommended primarily for workflow reasons. In many audio projects, your main application (e.g. a plugin, game, or embedded system) runs inside a host environment or with additional infrastructure such as a GUI or hardware integration.

HART tests, on the other hand, are designed to run as a simple console application, for example, locally during development, or in a CI pipeline.

This approach is also common for unit testing in general. Frameworks like Catch2 or Google Test are typically used in separate test executables, when testing non-audio parts of a project such as networking, file I/O, or business logic.

Keeping them separate also helps keep your test target lightweight. You typically only need to link the DSP code you want to test, without pulling in unrelated parts of your project such as UI, platform-specific code, or host integrations.

For instance, if you're making an audio plugin, your project (solution) may look like this:

```
MyAudioPlugin_SharedCode
MyAudioPlugin_VST3
MyAudioPlugin_AAX
MyAudioPlugin_Standalone
```

You might want to configure it to look like this:

```
MyAudioPlugin_SharedCode
MyAudioPlugin_VST3
MyAudioPlugin_AAX
MyAudioPlugin_Standalone
MyAudioPlugin_TestsAudio  <- Your HART target
```

If you already have some unit tests in your project, using a different framework, it's best keep them separate from your HART target:

```
MyAudioPlugin_SharedCode
MyAudioPlugin_VST3
MyAudioPlugin_AAX
MyAudioPlugin_Standalone
MyAudioPlugin_TestsApplication  <- Your non-audio tests
MyAudioPlugin_TestsAudio  <- Your audio tests with HART
```

While HART has some basic non-audio test capabilities, including macros like `HART_EXPECT_TRUE()`, it's not meant to replace general purpose unit test frameworks like Google Test or Catch2, that are way more powerful at expressing non-audio tests. So, if you want to test both audio and non-audio aspects of your product, I've found it's best to keep those as separate test suites made with different frameworks.

## Setting up audio tests target with Projucer

If you're using Projucer (JUCE) as your build system, you'll need to add it as a separate Projucer project (_File -> New Project... -> Application -> Console_), as Projucer projects configured for plugins cannot include console app targets:

```
MyAudioPlugin.jucer
TestsAudio.jucer  <- Your HART Projucer project
```

Then add your JUCE modules, audio cpp files and include directories, along with the HART's `include/` directory.

## Setting up audio tests target with CMake

If you're using CMake, it's pretty straightforward, as HART natively supports CMake. You can use `FetchContent` so that CMake downloads HART and links it automatically to your target.

```cmake
include(FetchContent)
FetchContent_Declare(
    HART
    GIT_REPOSITORY https://github.com/daleonov/HART.git
    GIT_TAG        develop  # Or "main"
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(HART)
```

Then set up your audio test target like this:

```cmake
add_executable(MyAudioPlugin_TestsAudio
	# Your cpp files with tests go here
)

target_link_libraries(MyAudioPlugin_TestsAudio PRIVATE
    MyAudioPlugin_SharedCode  # Your plugin code
    HART
)
```

And you should be good to go. You may also add this target as a CMake test, along with some CLI arguments via `add_test()` statement, but it's a little beyond the scope of this article.

# Compiler configuration

HART is written in C++11, so it requires your project to be configured for the C++11 standard, or a later one. HART is maintained to support major C++ compilers, namely GCC, Clang and MSVC, so any of those will work.

# Adding HART source files

Once you have your target, add HART to your project or repository. Here's the link to HART repository: https://github.com/daleonov/HART. You can just clone it, and copy into your project folder, you may add it as a submodule if you use git, or use `FetchContent` if you're using CMake as your build system. To use HART in your project, you only need the `include/` directory from HART repo (and `CMakeLists.txt` file if you use CMake). For the latest version of HART, you can use "develop" branch, instead of "main".

Then add HART's `include/` directory to the list of your header include directories. HART is a header-only framework, so you don't need to add any of the .cpp files to your project. Just one header directory, that's it.

If you're using CMake's `FetchContent`, you don't need to download HART yourself, or add its header path, it will do it automatically.

# "Hello world"

You need to define an entry point for your app, aka `main()` function. It's pretty much the same for all use cases, so you can just create a "Main.cpp" file like this:

```cpp
#define HART_IMPLEMENTATION

#include "hart.hpp"

int main (int argc, char** argv)
{
    HART_RUN_ALL_TESTS (argc, argv);
}
```

...then add it to your test target's source file list, and never touch it again. `HART_IMPLEMENTATION` macro must be defined in exactly one translation unit to provide the implementation for one of its components.

Now, it's time to create an actual test case. I'd recommend to make a separate cpp file for each group of tests, so your test source files may look like this:

```
📂 TestsAudio/
├─ 📄 Main.cpp
├─ 📄 CompressorTests.cpp
├─ 📄 SaturationTests.cpp
└─ 📄 SomeOtherDspTests.cpp
```

So, make a cpp file with a dummy test case like this:

```cpp
#include "hart.hpp"

HART_TEST ("Hello world")
{
	HART_EXPECT_TRUE (2 * 2 == 5);
}
```

Get it to build and run, watch the test case fail, fix the `2 * 2 == 5` statement, watch it pass. Once it passes, you're ready to actually start using HART.

# First audio test

Now, to the "Hello world" of actual audio tests. I call this test "Silence in - Silence out", which is pretty self-explanatory, and it applies to most of the audio algorithms. It's a good starting point in just about any audio project.

```cpp
#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;

HART_TEST ("Silence in - Silence out")
{
    processAudioWith (GainDb (0_dB))
        .withInputSignal (Silence())
        .expectFalse (EqualsTo (Silence()))  // Should fail!
        .process();
}
```

Run it, watch it fail, replace `expectFalse()` with `expectTrue()`, watch it pass.

The `HART_DECLARE_ALIASES_FOR_FLOAT` macro is optional, it lets you express your tests in a slightly less verbose way, e.g. use `Silence()` instead of `hart::Silence<float>()`. And there's a similar one for `double` sample type instead of `float`.

And you might have noticed there's only one include file for HART. It's not a single header framework, like some others, and it consists of multiple headers, but you only need to include one.

But obviously, you're not testing your own DSP yet. You're just making sure everything is configured properly here. To start testing your own DSP, see the @ref TestingYourDspInHart page.
