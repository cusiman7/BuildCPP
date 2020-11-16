
#pragma once

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

