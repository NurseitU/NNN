#include "check.hpp"

int main() {
    int failedCases = 0;
    for (const testing::Case& c : testing::registry()) {
        const int before = testing::failures();
        std::printf("[ RUN  ] %s\n", c.name);
        c.fn();
        if (testing::failures() == before) {
            std::printf("[  OK  ] %s\n", c.name);
        } else {
            std::printf("[ FAIL ] %s\n", c.name);
            ++failedCases;
        }
    }
    std::printf("\n%zu tests, %d failed\n", testing::registry().size(), failedCases);
    return failedCases == 0 ? 0 : 1;
}
