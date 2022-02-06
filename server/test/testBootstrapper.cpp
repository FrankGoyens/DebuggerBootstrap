#include <gtest/gtest.h>

extern "C" {
#include "../Bootstrapper.h"
}

TEST(testBootstrapper, ctor) {
    struct Bootstrapper given_bootstrapper;
    BootstrapperInit(&given_bootstrapper);
    BootstrapperDeinit(&given_bootstrapper);
}