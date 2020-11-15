
#define MOBIUS_ENTRY
#include <mobius/mobius.h>

using namespace mobius;

Project Generate(Toolchain toolchain) {
    toolchain.compiler.standard = Standard::CPP_17;
    toolchain.compiler.buildType = BuildType::Release; 
    toolchain.compiler.rtti = Flag::Off;
    toolchain.compiler.exceptions = Flag::Off;

    Project project(toolchain); 

    Target mobius("mobius", TargetType::Executable, {"src/mobius.cpp"});
    mobius.includeDirectories = {"include"};
    mobius.linkDirectories = { mobius::BuildDir() };
    mobius.install = true;
    mobius.linkFlags = {"-export_dynamic"};

    project.targets.emplace_back(std::move(mobius));
    
    InstallHeaders mobiusHeaders("mobius",
        {"include/mobius/mobius.h", "include/mobius/string.h"});
    project.installHeaders.emplace_back(std::move(mobiusHeaders)); 

    return project;
}
