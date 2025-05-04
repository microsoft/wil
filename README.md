# Windows Implementation Libraries (WIL)

[![Build Status](https://dev.azure.com/msft-wil/Windows%20Implementation%20Library/_apis/build/status/Microsoft.wil?branchName=master)](https://dev.azure.com/msft-wil/Windows%20Implementation%20Library/_build/latest?definitionId=1&branchName=master)

The Windows Implementation Libraries (WIL) is a header-only C++ library created to make life easier
for developers on Windows through readable type-safe C++ interfaces for common Windows coding patterns.

Some things that WIL includes to whet your appetite:

- [`include/wil/resource.h`](include/wil/resource.h)
  ([documentation](https://github.com/Microsoft/wil/wiki/RAII-resource-wrappers)):
  Smart pointers and auto-releasing resource wrappers to let you manage Windows
  API HANDLEs, HWNDs, and other resources and resource handles with
  [RAII](https://en.cppreference.com/w/cpp/language/raii) semantics.
- [`include/wil/win32_helpers.h`](include/wil/win32_helpers.h)
  ([documentation](https://github.com/microsoft/wil/wiki/Win32-helpers)): Wrappers for API functions
  that save you the work of manually specifying buffer sizes, calling a function twice
  to get the needed buffer size and then allocate and pass the right-size buffer,
  casting or converting between types, and so on.
- [`include/wil/registry.h`](include/wil/registry.h) ([documentation](https://github.com/microsoft/wil/wiki/Registry-Helpers)): Type-safe functions to read from, write to,
  and watch the registry. Also, registry watchers that can call a lambda function or a callback function
  you provide whenever a certain tree within the Windows registry changes.
- [`include/wil/result.h`](include/wil/result.h)
  ([documentation](https://github.com/Microsoft/wil/wiki/Error-handling-helpers)):
  Preprocessor macros to help you check for errors from Windows API functions,
  in many of the myriad ways those errors are reported, and surface them as
  error codes or C++ exceptions in your code.
- [`include/wil/Tracelogging.h`](include/wil/Tracelogging.h): This file contains the convenience macros
  that enable developers define and log telemetry. These macros use
  [`TraceLogging API`](https://docs.microsoft.com/en-us/windows/win32/tracelogging/trace-logging-portal)
  to log data. This data can be viewed in tools such as
  [`Windows Performance Analyzer`](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer).

WIL can be used by C++ code that uses C++ exceptions as well as code that uses returned
error codes to report errors. All of WIL can be used from user-space Windows code,
and some (such as the RAII resource wrappers) can even be used in kernel mode.

# Documentation

This project is documented in [its GitHub wiki](https://github.com/Microsoft/wil/wiki). Feel free to contribute to it!

# Consuming WIL
WIL follows the "live at head" philosophy, so you should feel free to consume WIL directly from the GitHub repo however you please: as a GIT submodule, symbolic link, download and copy files, etc. and update to the latest version at your own cadence. Alternatively, WIL is available using a few package managers, mentioned below. These packages will be updated periodically, likely to average around once or twice per month.

## Consuming WIL via NuGet
WIL is available on nuget.org under the name [Microsoft.Windows.ImplementationLibrary](https://www.nuget.org/packages/Microsoft.Windows.ImplementationLibrary/). This package includes the header files under the [include](include) directory as well as a [.targets](packaging/nuget/Microsoft.Windows.ImplementationLibrary.targets) file.

## Consuming WIL via vcpkg
WIL is also available using [vcpkg](https://github.com/microsoft/vcpkg) under the name [wil](https://github.com/microsoft/vcpkg/blob/master/ports/wil/portfile.cmake). Instructions for installing packages can be found in the [vcpkg GitHub docs](https://github.com/microsoft/vcpkg/blob/master/docs/examples/installing-and-using-packages.md). In general, once vcpkg is set up on the system, you can run:
```cmd
C:\vcpkg> vcpkg install wil:x86-windows
C:\vcpkg> vcpkg install wil:x64-windows
```
Note that even though WIL is a header-only library, you still need to install the package for all architectures/platforms you wish to use it with. Otherwise, WIL won't be added to the include path for the missing architectures/platforms. Execute `vcpkg help triplet` for a list of available options.

# Building/Testing

## Prerequisites

To get started contributing to WIL, first make sure that you have:

* The latest version of [Visual Studio](https://visualstudio.microsoft.com/downloads/) or Build Tools for Visual Studio with the latest MSVC C++ build tools and Address Sanitizer components included.
  * (`winget install Microsoft.VisualStudio.2022.Community` for instance)
* The most recent [Windows SDK](https://developer.microsoft.com/windows/downloads/windows-sdk)
  * (`winget search Microsoft.WindowsSDK` and then like `winget install Microsoft.WindowsSDK.10.0.26100`)
* [Nuget](https://www.nuget.org/downloads) downloaded and added to `PATH`
  * (`winget install nuget`; see [Install NuGet client tools](https://learn.microsoft.com/nuget/install-nuget-client-tools))
* [vcpkg](https://vcpkg.io) available on your system. Follow their [getting started](https://vcpkg.io/en/getting-started) guide to get set up.

If you are doing any non-trivial work, also be sure to have:

* A recent version of [Clang](http://releases.llvm.org/download.html)
  * (`winget install -i llvm.llvm` and select `Add LLVM to the system path for all users`)

Once you've installed Visual Studio, make sure you have the needed components:

1. Start "Visual Studio Installer"
2. For your Visual Studio version, click the "More" button
3. Choose "Import Configuration"
4. Select [.vsconfig](./vsconfig) and install all the build tools

### Console Builds

You can build from the console as long as you have the prerequisites installed. See below for how.

## Inner loop

### Visual Studio

Visual Studio 2022 16.x [has direct support for CMake projects.](https://learn.microsoft.com/cpp/build/cmake-projects-in-visual-studio) Use "File > Open Folder" on the `wil` root directory.

Once you've loaded the `wil` directory in Visual Studio, select a build configuration (like `x64-msvc-debug`). Be sure to select an architecture your development machine can run...

*Optional:* Set "Startup Item" to `witest.exe` to get the broadest tests possible.

After selecting the build configuration and startup item, the inner loop looks like:

1. Make change to WIL or test code
2. In VS2022, "Build > Build All"
4. In VS2022, "Debug > Start Debugging"

### Console

You can build a target architecture, compiler, and configuration using the [scripts/build.cmd](./scripts/build.cmd) like this:

```cmd
REM Builds using clang for debug of the current cmd's target architecture
c:\wil> scripts\build.cmd

REM Builds using msvc for release of arm64
c:\wil> scripts\build.cmd -t release -a arm64 -c msvc

REM Builds using clang for debug of x86
c:\wil> scripts\build.cmd -a x86
```

## Building everything

If you are at the tail end of of a change, you can execute the following to get a wide range of coverage:
```cmd
C:\wil> scripts\init_all.cmd
C:\wil> scripts\build_all.cmd
C:\wil> scripts\runtests.cmd
```

To build a specific architecture:
```cmd
C:\wil> scripts\init_all.cmd -a arm64
C:\wil> scripts\build_all.cmd -a arm64
C:\wil> scripts\runtests.cmd
```

## Formatting

This project has adopted `clang-format` as the tool for formatting our code.
Please note that the `.clang-format` at the root of the repo is a copy from the internal Windows repo with few additions.
In general, please do not modify it.
If you find that a macro is causing bad formatting of code, you can add that macro to one of the corresponding arrays in the `.clang-format` file (e.g. `AttributeMacros`, etc.), format the code, and submit a PR.

> _NOTE: Different versions of `clang-format` may format the same code differently.
In an attempt to maintain consistency between changes, we've standardized on using the version of `clang-format` that ships with the latest version of Visual Studio.
If you have LLVM installed and added to your `PATH`, the version of `clang-format` that gets picked up by default may not be the one we expect.
If you leverage the formatting scripts we have provided in the `scripts` directory, these should automatically pick up the proper version provided you are using a Visual Studio command window._

Before submitting a PR to the WIL repo we ask that you first run `clang-format` on your changes.
There is a CI check in place that will fail the build for your PR if you have not run `clang-format`.
There are a few different ways to format your code:

### 1. Formatting with `git clang-format`

> **Important!** Git integration with `clang-format` is only available through the LLVM distribution.
You can install LLVM through their [GibHub releases page](https://github.com/llvm/llvm-project/releases), via `winget install llvm.llvm`, or through the package manager of your choice.

> **Important!** The use of `git clang-format` additionally requires Python to be installed and available on your `PATH`.

The simplest way to format just your changes is to use `clang-format`'s `git` integration.
You have the option to do this continuously as you make changes, or at the very end when you're ready to submit a PR.
To format code continuously as you make changes, you run `git clang-format` after staging your changes.
For example:
```cmd
C:\wil> git add *
C:\wil> git clang-format --style file
```
At this point, the formatted changes will be unstaged.
You can review them, stage them, and then commit.
Please note that this will use whichever version of `clang-format` is configured to run with this command.
You can pass `--binary <path>` to specify the path to `clang-format.exe` you would like the command to use.

If you'd like to format changes at the end of development, you can run `git clang-format` against a specific commit/label.
The simplest is to run against `upstream/master` or `origin/master` depending on whether or not you are developing in a fork.
Please note that you likely want to sync/merge with the master branch prior to doing this step.
You can leverage the `format-changes.cmd` script we provide, which will use the version of `clang-format` that ships with Visual Studio:
```cmd
C:\wil> git fetch upstream
C:\wil> git merge upstream/master
C:\wil> scripts\format-changes.cmd upstream/master
```

### 2. Formatting with `clang-format`

> **Important!** The path to `clang-format.exe` is not added to `PATH` automatically, even when using a Visual Studio command window.
The LLVM installer has the option to add itself to the system or user `PATH` if you'd like.
If you would like the path to the version of `clang-format` that ships with Visual Studio added to your path, you will need to do so manually.
Otherwise, the `run-clang-format.cmd` script mentioned below (or, equivalently, building the `format` target) will manually invoke the `clang-format.exe` under your Visual Studio install path.

An alternative, and generally easier option, is to run `clang-format` either on all source files or on all source files you've modified.
Note, however, that depending on how `clang-format` is invoked, the version used may not be the one that ships with Visual Studio.
Some tools such as Visual Studio Code allow you to specify the path to the version of `clang-format` that you wish to use when formatting code, however this is not always the case.
The `run-clang-format.cmd` script we provide will ensure that the version of `clang-format` used is the version that shipped with your Visual Studio install:
```cmd
C:\wil> scripts\run-clang-format.cmd
```
Additionally, we've added a build target that will invoke this script, named `format`:
```cmd
C:\wil\build\clang64debug> ninja format
```
Please note that this all assumes that your Visual Studio installation is up to date.
If it's out of date, code unrelated to your changes may get formatted unexpectedly.
If that's the case, you may need to manually revert some modifications that are unrelated to your changes.

> _NOTE: Occasionally, Visual Studio will update without us knowing and the version installed for you may be newer than the version installed the last time we ran the format all script. If that's the case, please let us know so that we can re-format the code._

# Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
