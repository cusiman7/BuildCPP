
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getcwd
#include <fcntl.h> // open
#include <dlfcn.h>
#include <sys/stat.h>
#include <unordered_map>
#include <stdexcept>

#include <mobius/mobius.h>

void Fatal(const char* fmt, ...) {
    va_list arglist;
    va_start(arglist, fmt);
    vprintf(fmt, arglist);
    va_end(arglist);
    exit(1);
}

bool IsDir(const std::string& path) {
    struct stat sb;
    if (stat(path.c_str(), &sb) == 0) {
        return S_ISDIR(sb.st_mode); 
    }
    return false;
}

bool IsFile(const std::string& path) {
    struct stat sb;
    if (stat(path.c_str(), &sb) == 0) {
        return S_ISREG(sb.st_mode); 
    }
    return false;
}

bool MakeDir(const std::string& dir, bool existsOk = false) {
    if (IsDir(dir) && existsOk) {
        return true;
    }
    return mkdir(dir.c_str(), 0777) == 0;
}

std::string GetCwd() {
    char buf[4096];
    if (!getcwd(buf, 4096)) {
        Fatal("Failed to getcwd.\n");
    }
    return buf;
}

std::string GetEnv(const std::string& name, const std::string& def = "") {
    auto v = getenv(name.c_str());
    if (!v) return def;
    return v;
}

void ChangeDir(const std::string& path) {
    if (chdir(path.c_str()) != 0) {
        Fatal("Failed to chdir to %s\n", path.c_str());
    }
}

int Run(const std::vector<std::string>& cmd, std::string* output = nullptr) {
    char buf[4096];

    int cmdSize = 0;
    for (const auto& s : cmd) {
        cmdSize += s.size() + 1;
    }
    std::string runCmd;
    runCmd.reserve(cmdSize);
    for (const auto& s : cmd) {
        runCmd.append(s);
        runCmd.append(" ");
    }
    FILE* f = popen(runCmd.c_str(), "r");

    while (fgets(buf, 4096, f)) {
        if (output) {
            output->append(buf);
        } else {
            printf("%s", buf);
        }
    }
    if (ferror(f)) {
        Fatal("Error encountered running command: \"%s\"\n", runCmd.c_str());
    }
    return pclose(f);
}

void AppendStandard(std::vector<std::string>& cflags, Standard standard) {
    switch (standard) {
        case Standard::CPP_98:
            cflags.emplace_back("-std=c++98");
            break;
        case Standard::CPP_03:
            cflags.emplace_back("-std=c++03");
            break;
        case Standard::CPP_11:
            cflags.emplace_back("-std=c++11");
            break;
        case Standard::CPP_14:
            cflags.emplace_back("-std=c++14");
            break;
        case Standard::CPP_17:
            cflags.emplace_back("-std=c++17");
            break;
        case Standard::CPP_20:
            cflags.emplace_back("-std=c++20");
            break;
        default:
            break;
    }
}

void AppendBuildType(std::vector<std::string>& cflags, BuildType buildType) {
    switch (buildType) {
        case BuildType::Debug:
            cflags.emplace_back("-g -O0");
            break;
        case BuildType::Release:
            cflags.emplace_back("-O3");
            break;
        case BuildType::MinSize:
            cflags.emplace_back("-Os");
            break;
        default:
            break;
    }
}

void AppendFlag(std::vector<std::string>& cflags, Flag flag, const std::string& flagName) {
    if (flag == Flag::On) {
        cflags.emplace_back("-f" + flagName);
    } else if (flag == Flag::Off) {
        cflags.emplace_back("-fno-" + flagName);
    }
}

void AppendIncludeDirectory(std::vector<std::string>& cflags, const std::string& directory) {
    cflags.emplace_back("-I$root/" + directory);
}

void AppendLinkDirectory(std::vector<std::string>& ldflags, const std::string& directory) {
    ldflags.emplace_back("-L" + directory);
}

void AppendLinkFlag(std::vector<std::string>& cflags, const std::string& flag) {
    cflags.emplace_back("-Wl," + flag);
}

std::pair<std::string, std::string> SplitExt(const std::string& path) {
    auto startPos = path.find_first_not_of('.');
    auto extPos = path.find_first_of('.', startPos);
    return { path.substr(0, extPos), path.substr(extPos) };
}

struct NinjaVar {
    std::string name;
    std::vector<std::string> value;
};

void NinjaNewline(FILE* f) {
    fprintf(f, "\n");
}

void NinjaComment(FILE* f, const std::string& comment) {
    fprintf(f, "# %s\n", comment.c_str());
}

void NinjaVariable(FILE* f, const std::string& name, const std::string& value, 
                   const std::string& prefix = "") {
    fprintf(f, "%s%s = %s\n", prefix.c_str(), name.c_str(), value.c_str());
}

void NinjaVariable(FILE* f, const std::string& name, const std::vector<std::string>& value,
                   const std::string& prefix = "") {
    int lineLen = 0;
    lineLen += fprintf(f, "%s%s =", prefix.c_str(), name.c_str());
    for (const auto& v : value) {
        lineLen += fprintf(f, " %s", v.c_str());
        if (lineLen > 80) {
            lineLen = fprintf(f, " $\n    ");
        }
    }
    fprintf(f, "\n");
}

void NinjaRule(FILE* f, const std::string& name, const std::string& command,
                   std::vector<NinjaVar> variables = {}) {
    fprintf(f, "rule %s\n", name.c_str());
    fprintf(f, "  command = %s\n", command.c_str());
    for (const auto& v : variables) {
        NinjaVariable(f, v.name, v.value, "  ");
    }
}

void NinjaBuild(FILE* f, const std::string& output, const std::string& rule,
                    const std::vector<std::string>& inputs, const std::vector<NinjaVar> variables = {}) {
    int lineLen = 0;
    lineLen += fprintf(f, "build %s: %s", output.c_str(), rule.c_str());
    for (const auto& i : inputs) {
        lineLen += fprintf(f, " %s", i.c_str());
        if (lineLen > 80) {
            lineLen = fprintf(f, " $\n    ");
        }
    }
    fprintf(f, "\n");
    if (!variables.empty()) {
        for (const auto& v : variables) {
            NinjaVariable(f, v.name, v.value, "  ");
        }
    }
}

struct Mobius {
    std::string buildDir;
};
static Mobius gMobius;

namespace mobius {
std::string BuildDir() {
    return "$builddir";
}
} // namespace mobius

void Usage() {
    Fatal(
R"(usage: mobius [options] [builddir]

options:

  -C DIR  change to DIR before doing anything else 
)");
}

bool IsArg(const char* arg, const char* name, const char* altName = nullptr) {
    return strcmp(arg, name) == 0 || (altName && strcmp(arg, altName) == 0);
}

int main(int argc, const char** argv) {
    std::string changeDir;
    std::string buildDir;
    for (int i = 1; i < argc; i++) {
        if (*argv[i] == '-') {
            if (IsArg(argv[i], "-C")) {
                if (i + 1 == argc) Fatal("Expected directory argument after -C\n");
                changeDir = argv[i+1]; 
                i++;
            } else if (IsArg(argv[i], "-h", "--help")) {
                Usage();
            } else {
                Fatal("Unknown option %s\n", argv[i]);
            }
        } else {
            buildDir = argv[i];
        }
    }
    if (buildDir.empty()) {
        Usage();
    }
    if (!changeDir.empty()) {
        printf("mobius: Entering directory '%s'\n", changeDir.c_str()); 
        ChangeDir(changeDir);
    }
    if (!IsFile("build.cpp")) {
        Fatal("No build.cpp file in current directory\n");
    }
    if (!MakeDir(buildDir, true)) {
        Fatal("Failed to make directory \"%s\"\n", argv[1]);
    }
    gMobius.buildDir = buildDir;

    auto cxx = GetEnv("CXX", "c++");
    std::string root = GetCwd();
    std::string buildLib = buildDir + "/build.so";
    int cwd = open(".", O_RDONLY); 
    ChangeDir(buildDir);
    if (Run({cxx,
            "-std=c++17",
            "-O2",
            "-shared",
            "-Wl,-undefined,dynamic_lookup",
            "-MD", "-MF", "build.so.d",
            "../build.cpp", "-o", "build.so"}) != 0) {
        Fatal("Failed to compile build.cpp\n");
    }
    fchdir(cwd); 
    close(cwd);

    void* buildHandle = dlopen(buildLib.c_str(), RTLD_LAZY);
    if (!buildHandle) {
        Fatal("Failed to load \"%s\"\n", buildLib.c_str());
    }
    auto mobiusEntry = static_cast<MobiusEntry*>(dlsym(buildHandle, "mobiusEntry"));
    if (!mobiusEntry) {
        Fatal("Failed to find symbol \"mobiusEntry\" in %s", buildLib.c_str());
    }

    Toolchain toolchain;
    Project project = mobiusEntry->genProject(toolchain);
    const Compiler& comp = project.toolchain.compiler;

    std::string ninjaFile = buildDir + "/build.ninja";
    FILE* ninja = fopen(ninjaFile.c_str(), "w");
    NinjaComment(ninja, "This file was generated by mobius.");
    NinjaNewline(ninja);
    // Ninja globals
    NinjaVariable(ninja, "ninja_required_version", "1.3");

    NinjaVariable(ninja, "root", "..");
    NinjaVariable(ninja, "builddir", "mobiusout");

    // Compiler and Linker 
    NinjaVariable(ninja, "cxx", "c++");
    NinjaVariable(ninja, "ar", "ar");

    // Compiler and Linker Flags and Options
    std::vector<std::string> cflags;
    std::vector<std::string> ldflags;

    AppendStandard(cflags, comp.standard);
    AppendBuildType(cflags, comp.buildType);
    AppendFlag(cflags, comp.exceptions, "exceptions");
    AppendFlag(cflags, comp.rtti, "rtti");

    for (const auto& dir : project.includeDirectories) {
        AppendIncludeDirectory(cflags, dir);
    }    

    for (const auto& dir : project.linkDirectories) {
        AppendLinkDirectory(ldflags, dir);
    }

    NinjaVariable(ninja, "cflags", cflags);
    NinjaVariable(ninja, "ldflags", ldflags);
    
    NinjaNewline(ninja);

    // Compiler and Linker rules 
    NinjaRule(ninja, "cxx", "$cxx -MD -MF $out.d $cflags -c $in -o $out", 
                  {{"description", {"CXX $out"}}, {"depfile", {"$out.d"}}, {"deps", {"gcc"}}});
    NinjaNewline(ninja);

    NinjaRule(ninja, "ar", "rm -f $out && $ar crs $out $in", {{"description", {"AR $out"}}}); 
    NinjaNewline(ninja);

    NinjaRule(ninja, "link", "$cxx $ldflags -o $out $in $libs", {{"description", {"LINK $out"}}});
    NinjaNewline(ninja);

    for (const auto& target : project.targets) {
        std::vector<std::string> objectFiles;
        objectFiles.reserve(target.inputs.size());
        for (const auto& i : target.inputs) {
            auto pair = SplitExt(i);
            objectFiles.emplace_back("$builddir/" + pair.first + ".o");
            NinjaBuild(ninja, objectFiles.back(), "cxx", {"$root/" + i});
        }
        std::vector<NinjaVar> extraBuildVars;
        if (!target.linkFlags.empty()) {
            std::vector<std::string> targetLdFlags;
            targetLdFlags.reserve(target.linkFlags.size() + 1);
            targetLdFlags.emplace_back("$ldflags");
            for (const auto& linkFlag : target.linkFlags) {
                AppendLinkFlag(targetLdFlags, linkFlag); 
            }
            extraBuildVars.push_back(NinjaVar{"ldflags", targetLdFlags});
        }
        if (target.type == TargetType::Executable) {
            NinjaBuild(ninja, target.name, "link", objectFiles, extraBuildVars);
        } else if (target.type == TargetType::StaticLibrary) {
            NinjaBuild(ninja, target.name + ".a", "ar", objectFiles, extraBuildVars);
        } else if (target.type == TargetType::SharedLibrary) {
            NinjaBuild(ninja, target.name + ".so", "link", objectFiles, extraBuildVars);
        }
        NinjaNewline(ninja);
    }

    // #SoMeta
    NinjaRule(ninja, "mobius", "mobius -C $root " + buildDir, {{"generator", {"1"}},
             {"depfile", {"build.so.d"}}, {"deps", {"gcc"}}});
    NinjaBuild(ninja, "build.so", "mobius", {"$root/build.cpp"});

    fprintf(ninja, "\n");
    fclose(ninja);

    printf("Wrote %s\n", ninjaFile.c_str());
}
