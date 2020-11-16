
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getcwd
#include <fcntl.h> // open
#include <dlfcn.h>
#include <mach-o/dyld.h> // _NSGetExecutablePath
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

// include/buildcpp/string.h

namespace bcpp {

/*
 * Unlike std::string String is:
 *   1. Immutable
 *   2. Does not own its underlying memory
 *   3. All Strings share one underlying memory pool (String creation is not thread-safe)
 *
 * Use NewString and friends to create new strings
 */
struct String {
    String() = default;
    String(const String&) = default;
    String(String&&) = default;
    String& operator=(const String&) = default;
    String& operator=(String&&) = default;
    String(const char* str);
    String(const char* str, size_t len);

    bool Empty() const;
    size_t Len() const;
    const char* CStr() const;
    const char& operator[](size_t i) const;

    const char* begin() const { return buf_; }
    const char* end() const { return buf_ + len_ + 1; }

private:
    const char* buf_ = "";
    size_t len_ = 0;
};

struct StringArena;
struct TempStringArena;

// Strings allocated for program lifetime
String NewString(const char* str);
String NewString(const char* str, int len);
String ConcatStrings(const String& a, const String& b);
__attribute__((__format__ (__printf__, 1, 2)))
String FormatString(const char* fmt, ...);
String Substring(const String& a, size_t startPos, size_t len = -1);

// Strings allocated temporarily or to a specific arena of another lifetime
String NewString(StringArena* arena, const char* str);
String NewString(StringArena* arena, const char* str, int len);
String ConcatStrings(StringArena* arena, const String& a, const String& b);
__attribute__((__format__ (__printf__, 2, 3)))
String FormatString(StringArena* arena, const char* fmt, ...);
String Substring(StringArena* arena, const String& a, size_t startPos, size_t len = -1);

} // namespace bcpp 

// include/buildcpp/buildcpp.h

#include <vector>

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

// src/buildcpp.cpp

namespace bcpp {
String::String(const char* str) : buf_(str), len_(strlen(str)) {}
String::String(const char* str, size_t len) : buf_(str), len_(len) {}

bool String::Empty() const {
    return len_ == 0;
}

size_t String::Len() const {
    return len_;
}

const char* String::CStr() const {
    return buf_;
}

const char& String::operator[](size_t i) const {
    return buf_[i];
}

struct StringArena {
    char* buf = nullptr;
    size_t size = 0;
    size_t used = 0;
    int tempCount = 0;
};

struct TempStringArena {
    StringArena* arena;
    size_t used;

    ~TempStringArena() {
        assert(arena->used >= used);
        arena->used = used;
        arena->tempCount--;
        assert(arena->tempCount >= 0);
    }
};
} // namespace bcpp

namespace {
static bcpp::StringArena stringArena;
static bcpp::StringArena tempStringArena;

void* VirtualAlloc(size_t len) {
    return mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
}

__attribute__((__format__ (__printf__, 1, 2)))
void Fatal(const char* fmt, ...) {
    va_list arglist;
    va_start(arglist, fmt);
    vprintf(fmt, arglist);
    va_end(arglist);
    exit(1);
}

bcpp::StringArena AllocStringArena(size_t len) {
    bcpp::StringArena arena;
    arena.size = len;
    arena.buf = static_cast<char*>(VirtualAlloc(len));
    arena.tempCount = 0;
    return arena;
}

bcpp::TempStringArena BeginTempStringArena() {
    bcpp::TempStringArena temp;
    temp.arena = &tempStringArena;
    temp.used = temp.arena->used;
    temp.arena->tempCount++;
    return temp;
}

void InitBuildCpp() {
    static bool bcppInit = false;
    if (!bcppInit) {
        bcppInit = true;
        // >1GB of strings ought to be enough for anybody
        stringArena = AllocStringArena(1024 * 1024 * 1024); // 1GB
        tempStringArena = AllocStringArena(1024 * 1024 * 64); // 64MB
    }
}

char* AllocString(bcpp::StringArena* arena, size_t len) {
    assert(arena->used + len <= arena->size);
    char* ret = arena->buf + arena->used; 
    arena->used += len;
    return ret;
}
} // namespace

namespace bcpp {

String NewString(StringArena* arena, const char* str) {
    size_t len = strlen(str);
    char* buf = AllocString(arena, len+1);
    strcpy(buf, str);
    return String(buf, len); 
}

String NewString(const char* str) {
    return NewString(&stringArena, str);
}

String NewString(StringArena* arena, const char* str, int len) {
    char* buf = AllocString(arena, len+1);
    strncpy(buf, str, len+1);
    buf[len] = '\0';
    return String(buf, len);
}

String NewString(const char* str, int len) {
    return NewString(&stringArena, str, len);
}

String ConcatStrings(StringArena* arena, const String& a, const String& b) {
    size_t len = a.Len() + b.Len();
    char* buf = AllocString(arena, len+1);
    memcpy(buf, a.CStr(), a.Len());
    memcpy(buf + a.Len(), b.CStr(), b.Len());
    buf[len] = '\0';
    return String(buf, len);
}

String ConcatStrings(const String& a, const String& b) {
    return ConcatStrings(&stringArena, a, b);
}

String VFormatString(StringArena* arena, const char* fmt, va_list args1) {
    va_list args2;
    va_copy(args2, args1);
    size_t len = vsnprintf(nullptr, 0, fmt, args1);
    va_end(args1);
    char* buf = AllocString(arena, len+1);
    vsnprintf(buf, len+1, fmt, args2); 
    va_end(args2);
    return String(buf, len);
}

String FormatString(StringArena* arena, const char* fmt, ...) {
    va_list args1;
    va_start(args1, fmt);
    String ret = VFormatString(arena, fmt, args1);
    va_end(args1);
    return ret;
}

String FormatString(const char* fmt, ...) {
    va_list args1;
    va_start(args1, fmt);
    String ret = VFormatString(&stringArena, fmt, args1);
    va_end(args1);
    return ret;
}

String Substring(StringArena* arena, const String& a, size_t startPos, size_t len) {
    len = (len == size_t(-1)) ? a.Len() - startPos : len;
    char* buf = AllocString(arena, len+1);
    memcpy(buf, a.CStr() + startPos, len);
    buf[len] = '\0'; 
    String ret(buf, len);
    return ret; 
}

String Substring(const String& a, size_t startPos, size_t len) {
    return Substring(&stringArena, a, startPos, len);
}

String CopyString(StringArena* arena, const String& a) {
    return NewString(arena, a.CStr(), a.Len());
}

String CopyString(const String& a) {
    return NewString(a.CStr(), a.Len());
}

String BuildDir() {
    return "$builddir";
}

String InstallationPrefix() {
    return "$prefix";
}

} // namespace bcpp
using namespace bcpp;

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

String GetCwd(StringArena* arena) {
    char buf[1024];
    if (!getcwd(buf, 1024)) {
        Fatal("Failed to getcwd.\n");
    }
    return NewString(arena, buf);
}

String GetCwd() {
    return GetCwd(&stringArena);
}

String GetEnv(const String& name, const String& def = "") {
    auto v = getenv(name.CStr());
    if (!v) return def;
    return NewString(v);
}

String BaseName(const String& path) {
    if (path.Empty()) {
        return "";
    }
    // "Remove" trailing slashes
    size_t endPos = path.Len() - 1;
    while (endPos > 0 && path[endPos] == '/') {
        endPos--;
    }
    // Look for the last slash before endPos
    size_t lastPathSep = 0;
    for (size_t i = 0; i < endPos; i++) {
        if (path[i] == '/') lastPathSep = i;
    }
    auto ret = Substring(path, lastPathSep+1, endPos - lastPathSep);
    return ret;
}

String DirName(const String& path) {
    size_t lastPathSep = 0;
    for (size_t i = 0; i < path.Len(); i++) {
        if (path[i] == '/') lastPathSep = i;
    }
    return Substring(path, 0, lastPathSep);
}

String RealPath(StringArena* arena, const String& path) {
    char realpathBuf[PATH_MAX];
    realpath(path.CStr(), realpathBuf);
    return NewString(arena, realpathBuf);
}

String RealPath(const String& path) {
    return RealPath(&stringArena, path);
}

String RelativePath(const String& toPath, String start) {
    auto tempMem = BeginTempStringArena();

    if (start.Empty()) {
        start = GetCwd(tempMem.arena);
    }
    String absToPath = RealPath(tempMem.arena, toPath);
    String absStart = RealPath(tempMem.arena, start);
    
    int absStartSepCount = 0;
    for (auto c : absStart) {
        if (c == '/') absStartSepCount++;
    }

    size_t commonPathLen = 0;
    int commonSepCount = 0;
    while (commonPathLen < absToPath.Len() && commonPathLen < absStart.Len()) {
        if (absToPath[commonPathLen] != absStart[commonPathLen]) {
            break;
        }
        if (absToPath[commonPathLen] == '/') commonSepCount++;
        commonPathLen++;
    }
    if (commonPathLen == absToPath.Len() && commonPathLen == absStart.Len()) {
        return ".";
    } 
    if (commonPathLen < absToPath.Len()) commonPathLen++;
    String relPath = Substring(tempMem.arena, absToPath, commonPathLen);
    const int backSteps = absStartSepCount - commonSepCount;
    if (backSteps > 0) {
        relPath = ConcatStrings(tempMem.arena, relPath, "..");
    }
    for (int i = 1; i < backSteps; ++i) {
        relPath = ConcatStrings(tempMem.arena, relPath, "/..");
    }
    return CopyString(relPath);
}

std::pair<String, String> SplitExt(StringArena* arena, const String& path) {
    size_t startPos = 0;
    while (startPos <= path.Len() && path[startPos] == '.') ++startPos;
    size_t extPos = startPos;
    while(extPos <= path.Len() && path[extPos] != '.') ++extPos;
    return { Substring(arena, path, 0, extPos), Substring(arena, path, extPos) };
}

std::pair<String, String> SplitExt(const String& path) {
    return SplitExt(&stringArena, path);
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

String GetExecutablePath() {
    uint32_t bufsize = 1024;
    char buf[bufsize];
    if (_NSGetExecutablePath(buf, &bufsize) != 0) {
        Fatal("Can't get executable path\n");
    }
    return RealPath(buf);
}

// Build tooling helpers
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

void AppendCompileFlag(std::vector<String>& cflags, const String& flag) {
    cflags.emplace_back(flag);
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
        if (lineLen + v.Len() + 1 > 80) {
            lineLen = fprintf(f, " $\n    ");
        }
        lineLen += fprintf(f, " %s", v.CStr());
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
        if (lineLen + i.Len() + 1 > 80) {
            lineLen = fprintf(f, " $\n    ");
        }
        lineLen += fprintf(f, " %s", i.CStr());
    }
    fprintf(f, "\n");
    if (!variables.empty()) {
        for (const auto& v : variables) {
            NinjaVariable(f, v.name, v.value, "  ");
        }
    }
}

void NinjaDefault(FILE* f, const String& value) {
    fprintf(f, "default %s\n", value.CStr());
}

void Usage() {
    Fatal(
R"(usage: buildcpp [options] [builddir]

options:

  -C DIR             change to DIR before doing anything else 
  --prefix PREFIX    installation prefix
)");
}

bool IsArg(const char* arg, const char* name, const char* altName = nullptr) {
    return strcmp(arg, name) == 0 || (altName && strcmp(arg, altName) == 0);
}

String ConsumeOneArg(int* i, int argc, const char** argv) {
    if (*i + 1 == argc || *argv[*i + 1] == '-') {
        Fatal("Expected one value after %s\n", argv[*i]);
    }
    return NewString(argv[++(*i)]);
}

#ifdef BUILDCPP_MAIN

int main(int argc, const char** argv) {
    InitBuildCpp();

    // Command line args
    String changeDir;
    String buildDir;
    String installPrefix = "/usr/local";
    
    String exePath = GetExecutablePath();
    String bcppCommandLine;
    for (int i = 1; i < argc; i++) {
        if (*argv[i] == '-') {
            if (IsArg(argv[i], "-C")) {
                changeDir = ConsumeOneArg(&i, argc, argv);
            } else if (IsArg(argv[i], "--prefix")) {
                installPrefix = ConsumeOneArg(&i, argc, argv);
                bcppCommandLine = FormatString("%s --prefix %s",
                                        bcppCommandLine.CStr(), installPrefix.CStr());
            } else if (IsArg(argv[i], "-h", "--help")) {
                Usage();
            } else {
                Fatal("Unknown option %s\n", argv[i]);
            }
        } else {
            buildDir = NewString(argv[i]);
            bcppCommandLine = FormatString("%s %s", bcppCommandLine.CStr(), buildDir.CStr());
        }
    }

    if (buildDir.Empty()) {
        Usage();
    }
    if (!changeDir.Empty()) {
        printf("bcpp: Entering directory '%s'\n", changeDir.CStr()); 
        ChangeDir(changeDir);
    }
    String root = GetCwd();

    if (!IsFile(String("build.cpp"))) {
        Fatal("No build.cpp file in current directory\n");
    }
    if (!MakeDir(buildDir, true)) {
        Fatal("Failed to make directory \"%s\"\n", argv[1]);
    }

    // Build a relative path from buildDir back to root
    String relativeRoot = RelativePath(root, buildDir);
    bcppCommandLine = FormatString("%s -C %s", bcppCommandLine.CStr(), "$root");
    
    String exeDir = DirName(exePath);
    String cxx = GetEnv("CXX", "c++");

    String buildLib = ConcatStrings(buildDir, "/build.so");
    auto cmd = FormatString(
        "%s -std=c++17 -O2 -shared -Wl,-undefined,dynamic_lookup"
        " -I%s/../include"
        " -MD -MF build.so.d %s/build.cpp -o build.so", cxx.CStr(), exeDir.CStr(), relativeRoot.CStr());
    if (RunInDir(cmd, buildDir) != 0) {
        Fatal("Failed to run %s\n", cmd.CStr());
    }

    void* buildHandle = dlopen(buildLib.CStr(), RTLD_LAZY);
    if (!buildHandle) {
        Fatal("Failed to load \"%s\"\n", buildLib.CStr());
    }
    auto bcppEntry = static_cast<BuildCppEntry*>(dlsym(buildHandle, "buildCppEntry"));
    if (!bcppEntry) {
        Fatal("Failed to find symbol \"bcppEntry\" in %s\n", buildLib.CStr());
    }

    Toolchain toolchain;
    Project project = bcppEntry->generate(toolchain);
    const Compiler& comp = project.toolchain.compiler;

    String ninjaFile = ConcatStrings(buildDir, "/build.ninja");
    FILE* ninja = fopen(ninjaFile.CStr(), "w");
    if (!ninja) {
        Fatal("Failed to open %s for writing\n", ninjaFile.CStr());
    }
    NinjaComment(ninja, "This file was generated by bcpp.");
    NinjaNewline(ninja);
    // Ninja globals
    NinjaVariable(ninja, "ninja_required_version", "1.3");

    NinjaVariable(ninja, "root", relativeRoot);
    NinjaVariable(ninja, "builddir", "bcppout");
    // Command line and args
    NinjaVariable(ninja, "prefix", installPrefix);
    NinjaVariable(ninja, "bcppexe", exePath);
    NinjaVariable(ninja, "bcppcommandline", bcppCommandLine);

    // Compiler and Linker 
    NinjaVariable(ninja, "cxx", "c++");
    NinjaVariable(ninja, "ar", "ar");

    // Install/System tools

    // Compiler and Linker Flags and Options
    std::vector<String> cflags;
    std::vector<String> ldflags;

    AppendStandard(cflags, comp.standard);
    AppendBuildType(cflags, comp.buildType);
    AppendFlag(cflags, comp.exceptions, "exceptions");
    AppendFlag(cflags, comp.rtti, "rtti");

    cflags.reserve(cflags.size() + project.includeDirectories.size() + project.compileFlags.size());
    for (const auto& dir : project.includeDirectories) {
        AppendIncludeDirectory(cflags, dir);
    }
    for (const auto& flag : project.compileFlags) {
        AppendCompileFlag(cflags, flag);
    }

    ldflags.reserve(ldflags.size() + project.linkDirectories.size() + project.linkFlags.size());
    for (const auto& dir : project.linkDirectories) {
        AppendLinkDirectory(ldflags, dir);
    }
    for (const auto& flag : project.linkFlags) {
        AppendLinkFlag(ldflags, flag);
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

    // Install Rules
    NinjaRule(ninja, "cp", "cp -pR $in $out", {{"description", {"INSTALL $out"}}});
    NinjaNewline(ninja);

    // Targets
    std::vector<String> allInstallTargets;
    for (const auto& target : project.targets) {
        // We create a lot of temp strings per target
        auto tempMem = BeginTempStringArena();

        std::vector<NinjaVar> extraCompileVars;
        if (!target.includeDirectories.empty() || !target.compileFlags.empty()) {
            std::vector<String> targetCFlags;
            targetCFlags.reserve(target.includeDirectories.size() + target.compileFlags.size() + 1);
            targetCFlags.emplace_back("$cflags");
            for (const auto& dir : target.includeDirectories) {
                AppendIncludeDirectory(targetCFlags, dir);
            }
            for (const auto& flag : target.compileFlags) {
                AppendCompileFlag(targetCFlags, flag);
            }
            extraCompileVars.push_back(NinjaVar{"cflags", targetCFlags});
        }

        std::vector<String> objectFiles;
        objectFiles.reserve(target.inputs.size());
        for (const auto& i : target.inputs) {
            auto pair = SplitExt(tempMem.arena, i);
            objectFiles.emplace_back(FormatString(tempMem.arena, "$builddir/%s.o", pair.first.CStr()));
            NinjaBuild(ninja, objectFiles.back(), "cxx", {ConcatStrings(tempMem.arena, "$root/", i)}, extraCompileVars);
        }

        std::vector<NinjaVar> extraLinkVars;
        if (!target.linkFlags.empty() || !target.linkDirectories.empty()) {
            std::vector<String> targetLdFlags;
            targetLdFlags.reserve(target.linkFlags.size() + target.linkDirectories.size() + 1);
            targetLdFlags.emplace_back("$ldflags");
            for (const auto& dir : target.linkDirectories) {
                AppendLinkDirectory(targetLdFlags, dir);
            }
            for (const auto& linkFlag : target.linkFlags) {
                AppendLinkFlag(targetLdFlags, linkFlag); 
            }
            extraLinkVars.push_back(NinjaVar{"ldflags", targetLdFlags});
        }
        String targetOut;
        String buildRule;
        String installDir;
        switch (target.type) {
            case TargetType::Executable:
                targetOut = target.name;
                buildRule = "link";
                installDir = "bin";
                break;
            case TargetType::StaticLibrary:
                targetOut = ConcatStrings(tempMem.arena, target.name, ".a");
                buildRule = "ar";
                installDir = "lib";
                break;
            case TargetType::SharedLibrary:
                targetOut = ConcatStrings(tempMem.arena, target.name, ".so");
                buildRule = "link";
                installDir = "lib";
                break;
            case TargetType::MacOSBundle:
                Fatal("MacOSBundle target type not implemented yet\n");
                break;
        } 
        NinjaBuild(ninja, targetOut, buildRule, objectFiles, extraLinkVars);

        if (target.isDefault) {
            NinjaDefault(ninja, target.name);
        }

        if (target.install) {
            String installOut = FormatString("$prefix/%s/%s", installDir.CStr(), targetOut.CStr());
            NinjaBuild(ninja, installOut, "cp", {target.name});
            allInstallTargets.emplace_back(installOut);
        }
        NinjaNewline(ninja);
    }

    // Install
    for (const auto& installHeaders : project.installHeaders) {
        for (const auto& header : installHeaders.headers) {
            String installName = FormatString("$prefix/include/%s/%s",
                                    installHeaders.subdir.CStr(), BaseName(header).CStr());
            NinjaBuild(ninja, installName, "cp", {ConcatStrings("$root/", header)});
            allInstallTargets.emplace_back(installName);
        }
    }
    NinjaNewline(ninja);
    if (!allInstallTargets.empty()) {
        NinjaBuild(ninja, "install", "phony", allInstallTargets);
        NinjaNewline(ninja);
    }

    // #SoMeta
    NinjaRule(ninja, "buildcpp", "$bcppexe $bcppcommandline", {{"generator", {"1"}},
             {"depfile", {"build.so.d"}}, {"deps", {"gcc"}}});
    NinjaBuild(ninja, "build.ninja", "buildcpp", {"$root/build.cpp"});

    fprintf(ninja, "\n");
    fclose(ninja);

    printf("Wrote %s\n", ninjaFile.CStr());
}

#endif

