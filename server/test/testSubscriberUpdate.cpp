#include <gtest/gtest.h>

extern "C" {
#include "../SubscriberUpdate.h"
}

TEST(testSubscriberUpdate, ctor) {
    auto* created_update = EncodeSubscriberUpdateMessage("TESTTAG", "This is a test message");
    EXPECT_EQ(std::string(R"({ "tag": "TESTTAG", "message": "This is a test message" })"), created_update);
    free(created_update);
}