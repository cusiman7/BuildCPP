
#pragma once

#include <string>
#include <vector>

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
    Target(std::string name, TargetType type) 
    : name(name), type(type) {}
    Target(std::string name, TargetType type, std::vector<std::string> inputs) 
    : name(name), type(type), inputs(std::move(inputs)) {}

    std::string name;
    TargetType type;
    std::vector<std::string> inputs;

    std::vector<std::string> linkFlags;
};

struct Project {
    // TODO: Meta info Ninja won't care about like name & version
    explicit Project(Toolchain toolchain) : toolchain(toolchain) {}

    const Toolchain toolchain;
    std::vector<Target> targets;
    
    std::vector<std::string> includeDirectories;
    std::vector<std::string> linkDirectories;
};

// Mobius API
namespace mobius {
std::string BuildDir();
} // namespace mobius

using ProjectFn = Project (*)(Toolchain toolchain);
struct MobiusEntry {
    ProjectFn genProject;
};

#ifdef MOBIUS_ENTRY
Project GenProject(Toolchain toolchain);
MobiusEntry mobiusEntry{ GenProject };
#endif

