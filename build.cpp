
#define MOBIUS_ENTRY
#include <mobius/mobius.h>

using namespace mobius;

Project Generate(Toolchain toolchain) {
    toolchain.compiler.standard = Standard::CPP_17;
    toolchain.compiler.buildType = BuildType::Release; 
    toolchain.compiler.rtti = Flag::Off;
    toolchain.compiler.exceptions = Flag::Off;

    Project project(toolchain); 
    project.includeDirectories = {"include"};
    project.linkDirectories = { mobius::BuildDir() };

    InstallHeaders mobiusHeaders("mobius",
        {"include/mobius/mobius.h", "include/mobius/string.h"});
    project.installHeaders.emplace_back(std::move(mobiusHeaders)); 

    Target mobius("mobius", TargetType::Executable);
    mobius.install = true;
    mobius.inputs = {"src/mobius.cpp"};
    mobius.linkFlags = {"-export_dynamic"};

    project.targets.emplace_back(std::move(mobius));

    return project;
}
