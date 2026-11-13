#pragma once
// Крошечный каркас модульных тестов без внешних зависимостей.
//
//   TEST(имя) { CHECK(условие); CHECK_CONTAINS(строка, подстрока); }
//
// Тесты регистрируются сами; test_main.cpp запускает их по очереди и
// возвращает ненулевой код, если хоть одна проверка не прошла.

#include <cstdio>
#include <string>
#include <vector>

namespace testing {

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline void fail(const char* file, int line, const std::string& what) {
    ++failures();
    std::printf("    FAIL %s:%d: %s\n", file, line, what.c_str());
}

inline bool contains(const std::string& s, const std::string& part) {
    return s.find(part) != std::string::npos;
}

} // namespace testing

#define TEST(name)                                                        \
    static void test_##name();                                            \
    static testing::Registrar registrar_##name(#name, &test_##name);      \
    static void test_##name()

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) testing::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (0)

#define CHECK_CONTAINS(str, part)                                         \
    do {                                                                  \
        const std::string s_ = (str);                                     \
        const std::string p_ = (part);                                    \
        if (!testing::contains(s_, p_))                                   \
            testing::fail(__FILE__, __LINE__,                             \
                          "expected to contain \"" + p_ + "\", got: " + s_); \
    } while (0)

#define CHECK_NOT_CONTAINS(str, part)                                     \
    do {                                                                  \
        const std::string s_ = (str);                                     \
        const std::string p_ = (part);                                    \
        if (testing::contains(s_, p_))                                    \
            testing::fail(__FILE__, __LINE__,                             \
                          "expected NOT to contain \"" + p_ + "\", got: " + s_); \
    } while (0)
