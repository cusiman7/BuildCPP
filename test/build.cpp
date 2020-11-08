
#define MOBIUS_ENTRY
#include "mobius/mobius.h"

Project GenProject(Toolchain toolchain) {
    toolchain.compiler.standard = Standard::CPP_17;
    toolchain.compiler.buildType = BuildType::Release; 
    toolchain.compiler.rtti = Flag::Off;
    toolchain.compiler.exceptions = Flag::Off;

    Project p(toolchain); 

    Target hello("hello", TargetType::Executable, {"main.cpp"});
    p.targets.emplace_back(std::move(hello));

    return p;
}
