
#pragma once

#include <vector>
#include <buildcpp/string.h>

namespace bcpp {

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

struct Target;
struct Dependency {
    std::vector<String> includeDirectories;
    std::vector<String> libraries; 
};

struct Target {
    Target(String name, TargetType type) 
    : name(name), type(type) {}
    Target(String name, TargetType type, std::vector<String> inputs) 
    : name(name), type(type), inputs(std::move(inputs)) {}

    String name;
    TargetType type;
    bool install = false;
    bool isDefault = true;
    std::vector<String> inputs;
    
    std::vector<String> includeDirectories;
    std::vector<String> linkDirectories;

    std::vector<String> compileFlags;
    std::vector<String> linkFlags;
};

struct InstallHeaders {
    InstallHeaders(String subdir, std::vector<String> headers)
    : subdir(subdir), headers(std::move(headers)) {}

    String subdir;
    std::vector<String> headers;
};

struct Project {
    // TODO: Meta info Ninja won't care about like name & version
    explicit Project(Toolchain toolchain) : toolchain(toolchain) {}

    const Toolchain toolchain;
    std::vector<Target> targets;
    
    std::vector<String> includeDirectories;
    std::vector<String> linkDirectories;

    std::vector<String> compileFlags;
    std::vector<String> linkFlags;
    
    // Installation 
    std::vector<InstallHeaders> installHeaders;
};

String BuildDir();
String InstallationPrefix();

using GenerateFn = Project (*)(Toolchain toolchain);
struct BuildCppEntry {
    GenerateFn generate;
};
} // namespace bcpp 

#ifdef BUILDCPP_ENTRY
bcpp::Project Generate(bcpp::Toolchain toolchain);
bcpp::BuildCppEntry buildCppEntry{ Generate };
#endif

