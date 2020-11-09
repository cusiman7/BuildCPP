
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getcwd
#include <fcntl.h> // open
#include <dlfcn.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unordered_map>
#include <stdexcept>

#include <mobius/mobius.h>

void* VirtualAlloc(size_t len);
struct StringArena {
    StringArena() = default;
    StringArena(const StringArena&) = delete;
    StringArena(StringArena&&) = delete;

    char* Alloc(size_t len) {
        if (used_ + len > size_) {
            size_ = 1u << 30;
            buf_ = static_cast<char*>(VirtualAlloc(size_));
            used_ = 0;
        }
        char* ret = buf_ + used_; 
        used_ += len;
        return ret;
    }

private:
    char* buf_ = nullptr;
    size_t size_ = 0;
    size_t used_ = 0;
};
static StringArena gStringArena;

struct String {
    String() = default;
    String(const String&) = default;
    String(String&&) = default;
    String& operator=(const String&) = default;
    String& operator=(String&&) = default;
    explicit String(const char* str) : buf_(str), len_(strlen(str)) {}
    String(const char* str, size_t len) : buf_(str), len_(len) {}

    bool Empty() const {
        return buf_ == nullptr;
    }

    size_t Len() const {
        return len_;
    }

    const char* CStr() const {
        return buf_;
    }

    char operator[](size_t i) {
        return buf_[i];
    }
private:
    const char* buf_ = nullptr;
    size_t len_ = 0;
};
String operator "" _s(const char* str, size_t len) {
    return String(str, len);
}

String NewString(const char* str) {
    size_t len = strlen(str);
    char* buf = gStringArena.Alloc(len+1);
    strcpy(buf, str);
    return String(buf, len); 
}

String NewString(const char* str, int len) {
    char* buf = gStringArena.Alloc(len+1);
    strncpy(buf, str, len+1);
    buf[len] = '\0';
    return String(buf, len);
}

String NewString(const std::string& str) {
    size_t len = str.size();
    char* buf = gStringArena.Alloc(len+1);
    strcpy(buf, str.c_str());
    return String(buf, len);
}

String ConcatStrings(const String& a, const String& b) {
    size_t len = a.Len() + b.Len();
    char* buf = gStringArena.Alloc(len+1);
    memcpy(buf, a.CStr(), a.Len());
    strcpy(buf + a.Len(), b.CStr());
    return String(buf, len);
}

__attribute__((__format__ (__printf__, 1, 2)))
String FormatString(const char* fmt, ...) {
    va_list args1;
    va_start(args1, fmt);
    va_list args2;
    va_copy(args2, args1);
    size_t len = vsnprintf(nullptr, 0, fmt, args1);
    va_end(args1);
    char* buf = gStringArena.Alloc(len+1);
    vsnprintf(buf, len+1, fmt, args2); 
    va_end(args2);
    return String(buf, len);
}

namespace mobius {
std::string BuildDir() {
    return "$builddir";
}
} // namespace mobius

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
    char buf[4096];
    if (!getcwd(buf, 4096)) {
        Fatal("Failed to getcwd.\n");
    }
    return NewString(buf);
}

String GetEnv(const String& name, const String& def = ""_s) {
    auto v = getenv(name.CStr());
    if (!v) return def;
    return NewString(v);
}

void ChangeDir(const String& path) {
    if (chdir(path.CStr()) != 0) {
        Fatal("Failed to chdir to %s\n", path.CStr());
    }
}

void* VirtualAlloc(size_t len) {
    return mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
}

int Run(const String& cmd) {
    char buf[4096];
    FILE* f = popen(cmd.CStr(), "r");

    while (fgets(buf, 4096, f)) {
        printf("%s", buf);
    }
    if (ferror(f)) {
        Fatal("Error encountered running command: \"%s\"\n", cmd.CStr());
    }
    return pclose(f);
}

void AppendStandard(std::vector<String>& cflags, Standard standard) {
    switch (standard) {
        case Standard::CPP_98:
            cflags.emplace_back("-std=c++98"_s);
            break;
        case Standard::CPP_03:
            cflags.emplace_back("-std=c++03"_s);
            break;
        case Standard::CPP_11:
            cflags.emplace_back("-std=c++11"_s);
            break;
        case Standard::CPP_14:
            cflags.emplace_back("-std=c++14"_s);
            break;
        case Standard::CPP_17:
            cflags.emplace_back("-std=c++17"_s);
            break;
        case Standard::CPP_20:
            cflags.emplace_back("-std=c++20"_s);
            break;
        default:
            break;
    }
}

void AppendBuildType(std::vector<String>& cflags, BuildType buildType) {
    switch (buildType) {
        case BuildType::Debug:
            cflags.emplace_back("-g -O0"_s);
            break;
        case BuildType::Release:
            cflags.emplace_back("-O3"_s);
            break;
        case BuildType::MinSize:
            cflags.emplace_back("-Os"_s);
            break;
        default:
            break;
    }
}

void AppendFlag(std::vector<String>& cflags, Flag flag, const String& flagName) {
    if (flag == Flag::On) {
        cflags.emplace_back(ConcatStrings("-f"_s, flagName));
    } else if (flag == Flag::Off) {
        cflags.emplace_back(ConcatStrings("-fno-"_s, flagName));
    }
}

void AppendIncludeDirectory(std::vector<String>& cflags, const String& directory) {
    cflags.emplace_back(ConcatStrings("-I$root/"_s, directory));
}

void AppendLinkDirectory(std::vector<String>& ldflags, const String& directory) {
    ldflags.emplace_back(ConcatStrings("-L"_s, directory));
}

void AppendLinkFlag(std::vector<String>& cflags, const String& flag) {
    cflags.emplace_back(ConcatStrings("-Wl,"_s, flag));
}

std::pair<std::string, std::string> SplitExt(const std::string& path) {
    auto startPos = path.find_first_not_of('.');
    auto extPos = path.find_first_of('.', startPos);
    return { path.substr(0, extPos), path.substr(extPos) };
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
                   const String& prefix = ""_s) {
    fprintf(f, "%s%s = %s\n", prefix.CStr(), name.CStr(), value.CStr());
}

void NinjaVariable(FILE* f, const String& name, const std::vector<String>& value,
                   const String& prefix = ""_s) {
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
        NinjaVariable(f, v.name, v.value, "  "_s);
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
            NinjaVariable(f, v.name, v.value, "  "_s);
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

    String cxx = GetEnv("CXX"_s, "c++"_s);
    String root = GetCwd();
    String buildLib = ConcatStrings(buildDir, "/build.so"_s);
    int cwd = open(".", O_RDONLY); 
    ChangeDir(buildDir);
    if (Run(ConcatStrings(cxx,
        " -std=c++17 -O2 -shared -Wl,-undefined,dynamic_lookup "
        "-MD -MF build.so.d ../build.cpp -o build.so"_s)) != 0) {
        Fatal("Failed to compile build.cpp\n");
    }
    fchdir(cwd); 
    close(cwd);

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

    String ninjaFile = ConcatStrings(buildDir, "/build.ninja"_s);
    FILE* ninja = fopen(ninjaFile.CStr(), "w");
    NinjaComment(ninja, "This file was generated by mobius."_s);
    NinjaNewline(ninja);
    // Ninja globals
    NinjaVariable(ninja, "ninja_required_version"_s, "1.3"_s);

    NinjaVariable(ninja, "root"_s, ".."_s);
    NinjaVariable(ninja, "builddir"_s, "mobiusout"_s);

    // Compiler and Linker 
    NinjaVariable(ninja, "cxx"_s, "c++"_s);
    NinjaVariable(ninja, "ar"_s, "ar"_s);

    // Compiler and Linker Flags and Options
    std::vector<String> cflags;
    std::vector<String> ldflags;

    AppendStandard(cflags, comp.standard);
    AppendBuildType(cflags, comp.buildType);
    AppendFlag(cflags, comp.exceptions, "exceptions"_s);
    AppendFlag(cflags, comp.rtti, "rtti"_s);

    for (const auto& dir : project.includeDirectories) {
        AppendIncludeDirectory(cflags, NewString(dir));
    }    

    for (const auto& dir : project.linkDirectories) {
        AppendLinkDirectory(ldflags, NewString(dir));
    }

    NinjaVariable(ninja, "cflags"_s, cflags);
    NinjaVariable(ninja, "ldflags"_s, ldflags);
    
    NinjaNewline(ninja);

    // Compiler and Linker rules 
    NinjaRule(ninja, "cxx"_s, "$cxx -MD -MF $out.d $cflags -c $in -o $out"_s, 
                  {{"description"_s, {"CXX $out"_s}}, {"depfile"_s, {"$out.d"_s}}, {"deps"_s, {"gcc"_s}}});
    NinjaNewline(ninja);

    NinjaRule(ninja, "ar"_s, "rm -f $out && $ar crs $out $in"_s, {{"description"_s, {"AR $out"_s}}}); 
    NinjaNewline(ninja);

    NinjaRule(ninja, "link"_s, "$cxx $ldflags -o $out $in $libs"_s, {{"description"_s, {"LINK $out"_s}}});
    NinjaNewline(ninja);

    for (const auto& target : project.targets) {
        std::vector<String> objectFiles;
        objectFiles.reserve(target.inputs.size());
        for (const auto& i : target.inputs) {
            auto pair = SplitExt(i);
            objectFiles.emplace_back(FormatString("$builddir/%s.o", pair.first.c_str()));
            NinjaBuild(ninja, objectFiles.back(), "cxx"_s, {ConcatStrings("$root/"_s, NewString(i))});
        }
        std::vector<NinjaVar> extraBuildVars;
        if (!target.linkFlags.empty()) {
            std::vector<String> targetLdFlags;
            targetLdFlags.reserve(target.linkFlags.size() + 1);
            targetLdFlags.emplace_back("$ldflags"_s);
            for (const auto& linkFlag : target.linkFlags) {
                AppendLinkFlag(targetLdFlags, NewString(linkFlag)); 
            }
            extraBuildVars.push_back(NinjaVar{"ldflags"_s, targetLdFlags});
        }
        if (target.type == TargetType::Executable) {
            NinjaBuild(ninja, NewString(target.name), "link"_s, objectFiles, extraBuildVars);
        } else if (target.type == TargetType::StaticLibrary) {
            NinjaBuild(ninja, ConcatStrings(NewString(target.name), ".a"_s),
                "ar"_s, objectFiles, extraBuildVars);
        } else if (target.type == TargetType::SharedLibrary) {
            NinjaBuild(ninja, ConcatStrings(NewString(target.name), ".so"_s),
                "link"_s, objectFiles, extraBuildVars);
        }
        NinjaNewline(ninja);
    }

    // #SoMeta
    NinjaRule(ninja, "mobius"_s, ConcatStrings("mobius -C $root "_s, buildDir), {{"generator"_s, {"1"_s}},
             {"depfile"_s, {"build.so.d"_s}}, {"deps"_s, {"gcc"_s}}});
    NinjaBuild(ninja, "build.ninja"_s, "mobius"_s, {"$root/build.cpp"_s});

    fprintf(ninja, "\n");
    fclose(ninja);

    printf("Wrote %s\n", ninjaFile.CStr());
}
