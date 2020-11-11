
#pragma once

#include <vector>
#include <mobius/string.h>

namespace mobius {

enum class TargetType {
    Executable,
    StaticLibrary,
    SharedLibrary,
    MacOSBundle,
};

enum class BuildType {
    Default,
    Debug,
    Release,
    MinSize,
};

enum class Standard {
    Default,
    CPP_98, 
    CPP_03,
    CPP_11,
    CPP_14,
    CPP_17,
    CPP_20,
};

enum class Flag {
    Default,
    On,
    Off,
};

struct Compiler {
    Standard standard   = Standard::Default;
    BuildType buildType = BuildType::Default;
    Flag exceptions     = Flag::Default;
    Flag rtti           = Flag::Default;
};

struct Toolchain {
    Compiler compiler;
};

struct Target {
    Target(String name, TargetType type) 
    : name(name), type(type) {}
    Target(String name, TargetType type, std::vector<String> inputs) 
    : name(name), type(type), inputs(std::move(inputs)) {}

    String name;
    TargetType type;
    std::vector<String> inputs;

    std::vector<String> compileFlags;
    std::vector<String> linkFlags;
};

struct Project {
    // TODO: Meta info Ninja won't care about like name & version
    explicit Project(Toolchain toolchain) : toolchain(toolchain) {}

    const Toolchain toolchain;
    std::vector<Target> targets;
    
    std::vector<String> includeDirectories;
    std::vector<String> linkDirectories;
};

String BuildDir();

using ProjectFn = Project (*)(Toolchain toolchain);
struct MobiusEntry {
    ProjectFn genProject;
};
} // namespace mobius

#ifdef MOBIUS_ENTRY
mobius::Project GenProject(mobius::Toolchain toolchain);
mobius::MobiusEntry mobiusEntry{ GenProject };
#endif

