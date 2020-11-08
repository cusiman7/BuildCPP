
# Möbius

Möbius is an open source build system written in C++ and written in C++. Why configure your project in any other language? Möbius is both fast to generate your project and fast to compile it because it uses ninja as its backend for building your C++ projects.

## Building Your C++ Project With Möbius 

1. Describe your project in C++ in a file called `build.cpp` at the root of your project directory.
1. Run `mobius builddir` to generate a build.ninja file that will build to builddir.
1. Run `ninja` to compile and link your project.

## Example

Möbius is of course built with Möbius and its build.cpp file serves as a good starting example.

## How Does Möbius Work?

Möbius compiles your project definition file, build.cpp, with your system's C++ compiler into a dynamic library that it then loads and executes to generate a build.ninja file for you.

## Bootstrapping & Development

As Möbius is not yet available on any platforms you'll need to build it manually for now (ironic, I know). Möbius' build.ninja file is committed to this project repo to support project bootstrapping on MacOS. Run `ninja` to build mobius itself. Other platforms will be supported in the future, but for now check build.ninja for Möbius compiler and linker flags and requirements as they are non-trivial. Möbius has no external dependencies.

