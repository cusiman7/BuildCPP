
#define MOBIUS_ENTRY
#include "include/mobius/mobius.h"

Project GenProject(Toolchain toolchain) {
    toolchain.compiler.standard = Standard::CPP_17;
    toolchain.compiler.buildType = BuildType::Release; 
    toolchain.compiler.rtti = Flag::Off;
    toolchain.compiler.exceptions = Flag::Off;

    Project project(toolchain); 
    project.includeDirectories = {"include"};
    project.linkDirectories = { mobius::BuildDir() };

    Target mobius("mobius", TargetType::Executable);
    mobius.inputs = {"src/mobius.cpp"};
    mobius.linkFlags = {"-export_dynamic"};
    project.targets.emplace_back(std::move(mobius));

    return project;
}
