
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getcwd
#include <fcntl.h> // open
#include <dlfcn.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <mobius/mobius.h>
#include <mobius/string.h>

namespace {
void* VirtualAlloc(size_t len) {
    return mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
}

struct StringArena {
    char* buf = nullptr;
    size_t size = 0;
    size_t used = 0;
};

char* AllocString(size_t len) {
    static StringArena arena;
    if (arena.used + len > arena.size) {
        arena.size = 1u << 30;
        arena.buf = static_cast<char*>(VirtualAlloc(arena.size));
        arena.used = 0;
    }
    char* ret = arena.buf + arena.used; 
    arena.used += len;
    return ret;
}
} // namespace

namespace mobius {

String NewString(const char* str) {
    size_t len = strlen(str);
    char* buf = AllocString(len+1);
    strcpy(buf, str);
    return String(buf, len); 
}

String NewString(const char* str, int len) {
    char* buf = AllocString(len+1);
    strncpy(buf, str, len+1);
    buf[len] = '\0';
    return String(buf, len);
}

String ConcatStrings(const String& a, const String& b) {
    size_t len = a.Len() + b.Len();
    char* buf = AllocString(len+1);
    memcpy(buf, a.CStr(), a.Len());
    strcpy(buf + a.Len(), b.CStr());
    return String(buf, len);
}

String FormatString(const char* fmt, ...) {
    va_list args1;
    va_start(args1, fmt);
    va_list args2;
    va_copy(args2, args1);
    size_t len = vsnprintf(nullptr, 0, fmt, args1);
    va_end(args1);
    char* buf = AllocString(len+1);
    vsnprintf(buf, len+1, fmt, args2); 
    va_end(args2);
    return String(buf, len);
}

String Substring(const String& a, size_t startPos, size_t len) {
    len = (len == size_t(-1)) ? a.Len() - startPos : len;
    char* buf = AllocString(len+1);
    memcpy(buf, a.CStr() + startPos, len);
    buf[len] = '\0'; 
    String ret(buf, len);
    return ret; 
}

String BuildDir() {
    return "$builddir";
}

} // namespace mobius
using namespace mobius;

__attribute__((__format__ (__printf__, 1, 2)))
void Fatal(const char* fmt, ...) {
    va_list arglist;
    va_start(arglist, fmt);
    vprintf(fmt, arglist);
    va_end(arglist);
    exit(1);
}

bool IsDir(const String& path) {
    struct stat sb;
    if (stat(path.CStr(), &sb) == 0) {
        return S_ISDIR(sb.st_mode); 
    }
    return false;
}

bool IsFile(const String& path) {
    struct stat sb;
    if (stat(path.CStr(), &sb) == 0) {
        return S_ISREG(sb.st_mode); 
    }
    return false;
}

bool MakeDir(const String& dir, bool existsOk = false) {
    if (IsDir(dir) && existsOk) {
        return true;
    }
    return mkdir(dir.CStr(), 0777) == 0;
}

String GetCwd() {
    char buf[1024];
    if (!getcwd(buf, 1024)) {
        Fatal("Failed to getcwd.\n");
    }
    return NewString(buf);
}

String GetEnv(const String& name, const String& def = "") {
    auto v = getenv(name.CStr());
    if (!v) return def;
    return NewString(v);
}

void ChangeDir(const String& path) {
    if (chdir(path.CStr()) != 0) {
        Fatal("Failed to chdir to %s\n", path.CStr());
    }
}

int Run(const String& cmd) {
    char buf[4096];
    FILE* f = popen(cmd.CStr(), "r");
    if (!f) {
        Fatal("Error encountered running command: \"%s\"\n", cmd.CStr());
    }

    while (fgets(buf, 4096, f)) {
        printf("%s", buf);
    }
    if (ferror(f)) {
        Fatal("Error encountered running command: \"%s\"\n", cmd.CStr());
    }
    return pclose(f);
}

int RunInDir(const String& cmd, const String& dir) {
    int cwd = open(".", O_RDONLY); 
    ChangeDir(dir);
    int ret = Run(cmd);
    fchdir(cwd); 
    close(cwd);
    return ret;
};

void AppendStandard(std::vector<String>& cflags, Standard standard) {
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

void AppendBuildType(std::vector<String>& cflags, BuildType buildType) {
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

void AppendFlag(std::vector<String>& cflags, Flag flag, const String& flagName) {
    if (flag == Flag::On) {
        cflags.emplace_back(ConcatStrings("-f", flagName));
    } else if (flag == Flag::Off) {
        cflags.emplace_back(ConcatStrings("-fno-", flagName));
    }
}

void AppendIncludeDirectory(std::vector<String>& cflags, const String& directory) {
    cflags.emplace_back(ConcatStrings("-I$root/", directory));
}

void AppendLinkDirectory(std::vector<String>& ldflags, const String& directory) {
    ldflags.emplace_back(ConcatStrings("-L", directory));
}

void AppendLinkFlag(std::vector<String>& cflags, const String& flag) {
    cflags.emplace_back(ConcatStrings("-Wl,", flag));
}

std::pair<String, String> SplitExt(const String& path) {
    size_t startPos = 0;
    while (startPos <= path.Len() && path[startPos] == '.') ++startPos;
    size_t extPos = startPos;
    while(extPos <= path.Len() && path[extPos] != '.') ++extPos;
    return { Substring(path, 0, extPos), Substring(path, extPos) };
}

struct NinjaVar {
    String name;
    std::vector<String> value;
};

void NinjaNewline(FILE* f) {
    fprintf(f, "\n");
}

void NinjaComment(FILE* f, const String& comment) {
    fprintf(f, "# %s\n", comment.CStr());
}

void NinjaVariable(FILE* f, const String& name, const String& value, 
                   const String& prefix = "") {
    fprintf(f, "%s%s = %s\n", prefix.CStr(), name.CStr(), value.CStr());
}

void NinjaVariable(FILE* f, const String& name, const std::vector<String>& value,
                   const String& prefix = "") {
    int lineLen = 0;
    lineLen += fprintf(f, "%s%s =", prefix.CStr(), name.CStr());
    for (const auto& v : value) {
        lineLen += fprintf(f, " %s", v.CStr());
        if (lineLen > 80) {
            lineLen = fprintf(f, " $\n    ");
        }
    }
    fprintf(f, "\n");
}

void NinjaRule(FILE* f, const String& name, const String& command,
                   std::vector<NinjaVar> variables = {}) {
    fprintf(f, "rule %s\n", name.CStr());
    fprintf(f, "  command = %s\n", command.CStr());
    for (const auto& v : variables) {
        NinjaVariable(f, v.name, v.value, "  ");
    }
}

void NinjaBuild(FILE* f, const String& output, const String& rule,
                    const std::vector<String>& inputs, const std::vector<NinjaVar> variables = {}) {
    int lineLen = 0;
    lineLen += fprintf(f, "build %s: %s", output.CStr(), rule.CStr());
    for (const auto& i : inputs) {
        lineLen += fprintf(f, " %s", i.CStr());
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
    String changeDir;
    String buildDir;
    for (int i = 1; i < argc; i++) {
        if (*argv[i] == '-') {
            if (IsArg(argv[i], "-C")) {
                if (i + 1 == argc || *argv[i+1] == '-') Fatal("Expected directory argument after -C\n");
                changeDir = NewString(argv[i+1]); 
                i++;
            } else if (IsArg(argv[i], "-h", "--help")) {
                Usage();
            } else {
                Fatal("Unknown option %s\n", argv[i]);
            }
        } else {
            buildDir = NewString(argv[i]);
        }
    }
    if (buildDir.Empty()) {
        Usage();
    }
    if (!changeDir.Empty()) {
        printf("mobius: Entering directory '%s'\n", changeDir.CStr()); 
        ChangeDir(changeDir);
    }
    if (!IsFile(String("build.cpp"))) {
        Fatal("No build.cpp file in current directory\n");
    }
    if (!MakeDir(buildDir, true)) {
        Fatal("Failed to make directory \"%s\"\n", argv[1]);
    }

    String cxx = GetEnv("CXX", "c++");
    String root = GetCwd();
    String buildLib = ConcatStrings(buildDir, "/build.so");
    auto cmd = FormatString(
        "%s -std=c++17 -O2 -shared -Wl,-undefined,dynamic_lookup"
        " -I%s/include"
        " -MD -MF build.so.d ../build.cpp -o build.so", cxx.CStr(), root.CStr());
    if (RunInDir(cmd, buildDir) != 0) {
        Fatal("Failed to run %s\n", cmd.CStr());
    }

    void* buildHandle = dlopen(buildLib.CStr(), RTLD_LAZY);
    if (!buildHandle) {
        Fatal("Failed to load \"%s\"\n", buildLib.CStr());
    }
    auto mobiusEntry = static_cast<MobiusEntry*>(dlsym(buildHandle, "mobiusEntry"));
    if (!mobiusEntry) {
        Fatal("Failed to find symbol \"mobiusEntry\" in %s", buildLib.CStr());
    }

    Toolchain toolchain;
    Project project = mobiusEntry->genProject(toolchain);
    const Compiler& comp = project.toolchain.compiler;

    String ninjaFile = ConcatStrings(buildDir, "/build.ninja");
    FILE* ninja = fopen(ninjaFile.CStr(), "w");
    if (!ninja) {
        Fatal("Failed to open %s for writing\n", ninjaFile.CStr());
    }
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
    std::vector<String> cflags;
    std::vector<String> ldflags;

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
        std::vector<String> objectFiles;
        objectFiles.reserve(target.inputs.size());
        for (const auto& i : target.inputs) {
            auto pair = SplitExt(i);
            objectFiles.emplace_back(FormatString("$builddir/%s.o", pair.first.CStr()));
            NinjaBuild(ninja, objectFiles.back(), "cxx", {ConcatStrings("$root/", i)});
        }
        std::vector<NinjaVar> extraBuildVars;
        if (!target.linkFlags.empty()) {
            std::vector<String> targetLdFlags;
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
            NinjaBuild(ninja, ConcatStrings(target.name, ".a"),
                "ar", objectFiles, extraBuildVars);
        } else if (target.type == TargetType::SharedLibrary) {
            NinjaBuild(ninja, ConcatStrings(target.name, ".so"),
                "link", objectFiles, extraBuildVars);
        }
        NinjaNewline(ninja);
    }

    // #SoMeta
    NinjaRule(ninja, "mobius", ConcatStrings("mobius -C $root ", buildDir), {{"generator", {"1"}},
             {"depfile", {"build.so.d"}}, {"deps", {"gcc"}}});
    NinjaBuild(ninja, "build.ninja", "mobius", {"$root/build.cpp"});

    fprintf(ninja, "\n");
    fclose(ninja);

    printf("Wrote %s\n", ninjaFile.CStr());
}
