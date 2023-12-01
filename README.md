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
- [`include/wil/registry.h`](include/wil/registry.h): Type-safe functions to read from, write to,
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

* A recent version of [Visual Studio](https://visualstudio.microsoft.com/downloads/)
* The most recent [Windows SDK](https://developer.microsoft.com/windows/downloads/windows-sdk)
* [Nuget](https://www.nuget.org/downloads) downloaded and added to `PATH`
  * (`winget install nuget`; see [Install NuGet client tools](https://learn.microsoft.com/nuget/install-nuget-client-tools))

If you are doing any non-trivial work, also be sure to have:

* A recent version of [Clang](http://releases.llvm.org/download.html)
  * (`winget install -i llvm.llvm` and select `Add LLVM to the system path for all users`)

## Initial configuration

Once everything is installed (you'll need to restart Terminal if you updated `PATH` and don't have [this 2023 fix](https://github.com/microsoft/terminal/pull/14999)), open a VS native command window (e.g. `x64 Native Tools Command Prompt for VS 2022` \[_not_ `Developer Command Prompt for VS2022`]).

* If you are familiar with CMake you can get started building normally.
* Otherwise, or if you prefer to skip all of the boilerplate, you can use one of the scripts in the [scripts](scripts) directory, like `scripts\init.cmd [optional arguments]`
  * For example:
    ```cmd
    C:\wil> scripts\init.cmd -c clang -g ninja -b debug
    ```

To set up IDEs with IntelliSense, see below.

You can execute `init.cmd --help` for a summary of available options.

### Visual Studio setup

To generate a Visual Studio solution with IntelliSense:
```cmd
C:\wil> scripts\init.cmd -c msvc -g msbuild
```

That will create a `.sln` file in the corresponding`build/` subdirectory. You can also invoke MSBuild directly to build.

You can also get decent IntelliSense just by opening the repo directory in Visual Studio; VS should auto-detect CMake. You'll have to compile and run tests in a terminal window, though.

## Inner loop

The scripts use a common directory pattern of `build/$(compiler)$(arch)$(type)` for the build output root. E.g. `build/clang64debug` when using Clang as the compiler, x64 as the architecture, and Debug as the build type. It is this directory where you will want to build from.

For example, if you initialized using the command above (`scripts\init.cmd -c clang -g ninja -b debug`), you can build the tests like so:
```cmd
C:\wil\build\clang64debug> ninja
```
Or, if you want to only build a single test (e.g. for improved compile times):
```cmd
C:\wil\build\clang64debug> ninja witest.noexcept
```

The output is a number of test executables. If you used the initialization script(s) mentioned above, or if you followed
the same directory naming convention of those scripts, you can use the [runtests.cmd](scripts/runtests.cmd) script,
which will execute any test executables that have been built, erroring out - and preserving the exit code - if any test
fails. Note that MSBuild will modify the output directory names, so this script is only compatible with using Ninja as the
generator.

## Build everything

If you are at the tail end of of a change, you can execute the following to get a wide range of coverage:
```cmd
C:\wil> scripts\init_all.cmd
C:\wil> scripts\build_all.cmd
C:\wil> scripts\runtests.cmd
```
Note that this will only test for the architecture that corresponds to the command window you opened. You will want to
repeat this process for the other architecture (e.g. by using the `x86 Native Tools Command Prompt for VS 2022` in addition to `x64`).

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
