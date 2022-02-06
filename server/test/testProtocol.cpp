#include <gtest/gtest.h>

#include <limits>

extern "C" {
#include "../../protocol/protocol.h"
#include "../ProjectDescription.h"
#include "../ProjectDescription_json.h"
}

TEST(testProtocol, MakeAndDecodeProjectDescriptionPacket) {

    ProjectDescription given_description;

    ProjectDescriptionInit(&given_description, "DebuggerBootstrap");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "first.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "second.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "third.so");

    char* given_description_json = ProjectDescriptionDumpToJSON(&given_description);
    const size_t given_json_size = strlen(given_description_json) + 1;

    char* packet;
    size_t packet_size;

    MakeProjectDescriptionPacket(&given_description, &packet, &packet_size);
    ProjectDescriptionDeinit(&given_description);

    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION, packet[0]);
    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION, packet[1]);
    EXPECT_EQ(std::string(given_description_json) + '\0', std::string(&packet[2], given_json_size));

    free(given_description_json);

    // Decoding starts here

    size_t created_json_part_offset;
    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION,
              DecodePacket(packet, packet_size, &created_json_part_offset));

    ProjectDescription created_decoded_description;
    EXPECT_TRUE(ProjectDescriptionLoadFromJSON(packet + created_json_part_offset, &created_decoded_description));
    EXPECT_EQ(std::string("DebuggerBootstrap"), created_decoded_description.executable_name);
    ASSERT_EQ(3u, created_decoded_description.link_dependencies_for_executable.size);
    EXPECT_EQ(std::string("first.so"), created_decoded_description.link_dependencies_for_executable.data[0]);
    EXPECT_EQ(std::string("second.so"), created_decoded_description.link_dependencies_for_executable.data[1]);
    EXPECT_EQ(std::string("third.so"), created_decoded_description.link_dependencies_for_executable.data[2]);

    ProjectDescriptionDeinit(&created_decoded_description);
    free(packet);
}

TEST(testProtocol, MakeAndDecodeSubscriptionResponsePacket) {
    char* created_packet;
    size_t created_packet_size;
    MakeRequestSubscriptionPacket(&created_packet, &created_packet_size);

    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION, created_packet[0]);
    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_REQUEST, created_packet[1]);

    // Decoding starts here

    size_t created_header_offset;
    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_REQUEST,
              DecodePacket(created_packet, created_packet_size, &created_header_offset));
    EXPECT_EQ(2u, created_header_offset);

    free(created_packet);
}

TEST(testProtocol, FindNullTerminator) {
    char given_packet[128];
    memset(given_packet, 'a', 128);

    EXPECT_EQ(128, FindNullTerminator(given_packet, 128));

    given_packet[23] = '\0';
    EXPECT_EQ(23, FindNullTerminator(given_packet, 128));

    given_packet[0] = '\0';
    EXPECT_EQ(0, FindNullTerminator(given_packet, 128));
}