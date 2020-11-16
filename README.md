
# Build CPP 

Build CPP is an open source build system written in C++ and written in C++. Why configure your C++ project in any other language? Build CPP is both fast to generate your project and fast to compile it because it uses Ninja as its backend for building your C++ projects.

## Building Your C++ Project With Build CPP

1. Describe your project in C++ in a file called `build.cpp` at the root of your project directory.
1. Run `buildcpp build` to generate a ./build/build.ninja file.
1. Run `ninja -C build/build.ninja` to compile and link your project.

## Example

Build CPP is of course built with Build CPP and its build.cpp file serves as a good starting example.

## How Does Build CPP Work?

Build CPP compiles your project definition file, build.cpp, with your system's C++ compiler into a dynamic library that it then loads and executes to generate a build.ninja file for you. This is only done once, during initial project generation, or whenever you change build.cpp or any headers it includes. This way, incremental builds with Ninja stay fast.

## Bootstrapping & Development

As Build CPP is not yet available on any platforms you'll need to build it manually for now (ironic, I know). Build CPP's build.ninja file is committed to this project repo to support project bootstrapping on MacOS. Run `ninja -C build` to build buildcpp itself. Other platforms will be supported in the future, but for now check build.ninja for Build CPP's compiler and linker flags and requirements as they are non-trivial. BuildCPP has no external dependencies.

