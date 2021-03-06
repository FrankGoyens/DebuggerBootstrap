#include <gtest/gtest.h>

extern "C" {
#include "../../protocol/Protocol.h"
#include "../ProjectDescription.h"
#include "../ProjectDescription_json.h"
}

TEST(testProtocol, MakeAndDecodeProjectDescriptionPacket) {

    ProjectDescription given_description;

    ProjectDescriptionInit(&given_description, "DebuggerBootstrap", "abcd");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "first.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "second.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "third.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable_hashes, "defg");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable_hashes, "hijk");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable_hashes, "lmno");

    char* given_description_json = ProjectDescriptionDumpToJSON(&given_description);
    const size_t given_json_size = strlen(given_description_json) + 1;

    uint8_t* packet;
    size_t packet_size;

    char* given_project_description_json_string = ProjectDescriptionDumpToJSON(&given_description);
    MakeProjectDescriptionPacket(given_project_description_json_string, &packet, &packet_size);
    free(given_project_description_json_string);
    ProjectDescriptionDeinit(&given_description);

    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION, packet[0]);
    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION, packet[1]);
    EXPECT_EQ(std::string(given_description_json) + '\0', std::string((char*)&packet[2], given_json_size));

    free(given_description_json);

    // Decoding starts here

    size_t created_json_part_offset;
    EXPECT_EQ(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION,
              DecodePacket(packet, packet_size, &created_json_part_offset));

    ProjectDescription created_decoded_description;
    EXPECT_TRUE(
        ProjectDescriptionLoadFromJSON((char*)(packet + created_json_part_offset), &created_decoded_description));
    EXPECT_EQ(std::string("DebuggerBootstrap"), created_decoded_description.executable_name);
    ASSERT_EQ(3u, created_decoded_description.link_dependencies_for_executable.size);
    EXPECT_EQ(std::string("first.so"), created_decoded_description.link_dependencies_for_executable.data[0]);
    EXPECT_EQ(std::string("second.so"), created_decoded_description.link_dependencies_for_executable.data[1]);
    EXPECT_EQ(std::string("third.so"), created_decoded_description.link_dependencies_for_executable.data[2]);

    ProjectDescriptionDeinit(&created_decoded_description);
    free(packet);
}

TEST(testProtocol, MakeAndDecodeSubscriptionResponsePacket) {
    uint8_t* created_packet;
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
    uint8_t given_packet[128];
    memset(given_packet, 'a', 128);

    size_t position;
    ASSERT_FALSE(FindNullTerminator(given_packet, 128, &position));

    given_packet[23] = '\0';
    ASSERT_TRUE(FindNullTerminator(given_packet, 128, &position));
    EXPECT_EQ(23, position);

    given_packet[0] = '\0';
    ASSERT_TRUE(FindNullTerminator(given_packet, 128, &position));
    EXPECT_EQ(0, position);
}