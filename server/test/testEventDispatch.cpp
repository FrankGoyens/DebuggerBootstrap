#include <gtest/gtest.h>

#include <string>

#include "../DynamicBuffer.h"

extern "C" {
DynamicBuffer* CombineMessageForFileMismatch(const char* file, const char* wanted_hash, const char* actual_hash);
}

TEST(testEventDispatch, CombineMessageForFileMismatch) {
    auto* createdDynamicBuffer = CombineMessageForFileMismatch("testFile", "abc", "def");
    EXPECT_EQ(std::string("file: \"testFile\" wanted hash: \"abc\" actual hash: \"def\""),
              std::string(createdDynamicBuffer->data));
    free(createdDynamicBuffer);
}