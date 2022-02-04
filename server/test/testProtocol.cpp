#include <gtest/gtest.h>

#include <limits>

extern "C" {
#include "../../protocol/protocol.h"
#include "../ProjectDescription.h"
#include "../ProjectDescription_json.h"
}

TEST(testProtocol, DivideAndCombineValueUsingLowerAndUpper) {
    const uint16_t given_value = std::numeric_limits<uint16_t>::max();
    uint8_t created_lower, created_upper;
    DivideIntoLowerAndUpperBytes(given_value, &created_lower, &created_upper);
    EXPECT_EQ(255, created_lower);
    EXPECT_EQ(255, created_upper);
    const uint16_t created_value = CombineIntoValue(created_lower, created_upper);
    EXPECT_EQ(std::numeric_limits<uint16_t>::max(), created_value);
}

TEST(testProtocol, MakeProjectDescriptionPacket) {

    ProjectDescription given_description;

    ProjectDescriptionInit(&given_description, "DebuggerBootstrap");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "first.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "second.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "third.so");

    char* given_description_json = ProjectDescriptionDumpToJSON(&given_description);
    const size_t given_json_size = strlen(given_description_json);

    char* packet;
    size_t packet_size;

    MakeProjectDescriptionPacket(&given_description, &packet, &packet_size);
    ProjectDescriptionDeinit(&given_description);

    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION, packet[0]);
    EXPECT_EQ(given_json_size, CombineIntoValue(packet[1], packet[2]));
    EXPECT_EQ(std::string(given_description_json), std::string(&packet[3], given_json_size));

    free(given_description_json);
    free(packet);
}