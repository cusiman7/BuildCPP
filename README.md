
# Build CPP 

Build CPP is an open source build system written in C++ and written in C++. Why configure your C++ project in any other language? Build CPP is both fast to generate your project and fast to compile it because it uses [Ninja](https://ninja-build.org/) as its backend for building your C++ projects.

## Building Your C++ Project With Build CPP

1. Describe your project in C++ in a file called `build.cpp` at the root of your project directory.
1. Run `buildcpp build` to generate a ./build/build.ninja file.
1. Run `ninja -C build/build.ninja` to compile and link your project.

## Example

Build CPP is of course built with Build CPP and its build.cpp file serves as a good starting example.

## How Does Build CPP Work?

Build CPP compiles your project definition file, build.cpp, with your system's C++ compiler into a dynamic library that it then loads and executes to generate a build.ninja file for you. This is only done once during initial project generation or whenever you change build.cpp or any headers it includes. This way, incremental builds with Ninja stay fast.

## Bootstrapping Build CPP

Build CPP ships with a build.ninja file that is both a valid Ninja file and a source file that includes buildcpp's entire single header source when compiled. Simply run `ninja` to bootstrap buildcpp itself. If you would like to trial buildcpp in your project or with your team just copy ./build.ninja and single\_include to your project's root and run `ninja` to build buildcpp. You can then use buildcpp as described above. Platforms other than MacOS will be supported in the future but for now check build.ninja for Build CPP's compiler and linker flags and requirements as they are non-trivial. BuildCPP has no external C++ dependencies and only requires Ninja to be installed on your system.

