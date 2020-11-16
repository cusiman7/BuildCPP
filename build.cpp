
#define BUILDCPP_ENTRY
#include <buildcpp/buildcpp.h>

using namespace bcpp;

Project Generate(Toolchain toolchain) {
    toolchain.compiler.standard = Standard::CPP_17;
    toolchain.compiler.buildType = BuildType::Release; 
    toolchain.compiler.rtti = Flag::Off;
    toolchain.compiler.exceptions = Flag::Off;

    Project project(toolchain); 

    Target buildcpp("buildcpp", TargetType::Executable, {"src/buildcpp.cpp"});
    buildcpp.includeDirectories = {"include"};
    buildcpp.linkDirectories = { bcpp::BuildDir() };
    buildcpp.install = true;
    buildcpp.linkFlags = {"-export_dynamic"};

    project.targets.emplace_back(std::move(buildcpp));
    
    InstallHeaders buildcppHeaders("buildcpp",
        {"include/buildcpp/buildcpp.h", "include/buildcpp/string.h"});
    project.installHeaders.emplace_back(std::move(buildcppHeaders)); 

    return project;
}
