#pragma once

// A deliberately tiny test harness: TEST registers a function, CHECK and
// CHECK_EQ record failures without stopping the test, and runAll() returns a
// process exit code. Each test binary includes this header in exactly one
// translation unit that defines CHECK_MAIN.

#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace check {

struct Case {
    const char *name;
    std::function<void()> body;
};

inline std::vector<Case> &cases()
{
    static std::vector<Case> all;
    return all;
}

inline int &failures()
{
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char *name, std::function<void()> body)
    {
        cases().push_back({name, std::move(body)});
    }
};

template <typename T>
std::string show(const T &value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

inline std::string show(const std::string &value)
{
    return "\"" + value + "\"";
}

inline std::string show(const char *value)
{
    return show(std::string(value));
}

inline std::string show(bool value)
{
    return value ? "true" : "false";
}

inline void fail(const char *file, int line, const std::string &message)
{
    ++failures();
    std::fprintf(stderr, "  %s:%d: %s\n", file, line, message.c_str());
}

inline int runAll(const char *filter)
{
    int ran = 0;
    for (const Case &testCase : cases()) {
        if (filter != nullptr &&
            std::string(testCase.name).find(filter) == std::string::npos) {
            continue;
        }
        const int before = failures();
        testCase.body();
        ++ran;
        std::printf("%s %s\n", failures() == before ? "ok  " : "FAIL",
                    testCase.name);
    }
    std::printf("\n%d test(s), %d failed check(s)\n", ran, failures());
    return failures() == 0 && ran > 0 ? 0 : 1;
}

} // namespace check

#define CHECK_CONCAT_INNER(a, b) a##b
#define CHECK_CONCAT(a, b) CHECK_CONCAT_INNER(a, b)

#define TEST(name)                                                         \
    static void CHECK_CONCAT(test_, __LINE__)();                           \
    static check::Registrar CHECK_CONCAT(registrar_, __LINE__)(            \
        name, CHECK_CONCAT(test_, __LINE__));                              \
    static void CHECK_CONCAT(test_, __LINE__)()

#define CHECK(condition)                                                   \
    do {                                                                   \
        if (!(condition)) {                                                \
            check::fail(__FILE__, __LINE__, "CHECK(" #condition ")");      \
        }                                                                  \
    } while (0)

#define CHECK_EQ(actual, expected)                                         \
    do {                                                                   \
        const auto &checkActual = (actual);                                \
        const auto &checkExpected = (expected);                            \
        if (!(checkActual == checkExpected)) {                             \
            check::fail(__FILE__, __LINE__,                                \
                        std::string(#actual " == " #expected ": got ") +   \
                            check::show(checkActual) + ", expected " +     \
                            check::show(checkExpected));                   \
        }                                                                  \
    } while (0)

#ifdef CHECK_MAIN
int main(int argc, char **argv)
{
    return check::runAll(argc > 1 ? argv[1] : nullptr);
}
#endif
