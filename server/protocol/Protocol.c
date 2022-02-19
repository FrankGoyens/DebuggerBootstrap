#include "Protocol.h"

#include <stdlib.h>
#include <string.h>

#include "../ProjectDescription.h"

void MakeProjectDescriptionPacket(const char* project_description_json_string, char** packet, size_t* packet_size) {
    const uint8_t version = DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION;

    size_t json_length = strlen(project_description_json_string) + 1;
    *packet_size = PACKET_HEADER_SIZE * sizeof(uint8_t) + json_length;

    *packet = (char*)malloc(*packet_size);
    char* packet_content = *packet;
    packet_content[0] = version;
    packet_content[1] = DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION;
    memcpy(packet_content + PACKET_HEADER_SIZE, project_description_json_string, json_length);
}

enum DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE DecodePacket(const char* packet, size_t packet_size,
                                                          size_t* json_part_offset) {
    if (packet_size < PACKET_HEADER_SIZE)
        return DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_INCOMPLETE;
    if (packet[0] != DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION)
        return DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN;
    *json_part_offset = PACKET_HEADER_SIZE;
    if (packet[1] >= DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN)
        return DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN;
    return packet[1];
}

void MakeRequestSubscriptionPacket(char** packet, size_t* packet_size) {
    const uint8_t version = DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION;
    *packet_size = 4;
    *packet = (char*)malloc(*packet_size);
    char* packet_content = *packet;
    packet_content[0] = version;
    packet_content[1] = DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_REQUEST;
}

void MakeSubscriptionResponsePacketHeader(char** packet, size_t* packet_size) {
    const uint8_t version = DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION;
    *packet = (char*)malloc(*packet_size);
    char* packet_content = *packet;
    packet_content[0] = version;
    packet_content[1] = DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_RESPONSE;
}

size_t FindNullTerminator(const char* packet, size_t packet_size) {
    for (size_t i = 0; i < packet_size; ++i) {
        if (packet[i] == '\0')
            return i;
    }
    return packet_size;
}