#include "Protocol.h"

#include <stdlib.h>
#include <string.h>

#include "../ProjectDescription.h"

void MakeProjectDescriptionPacket(const char* project_description_json_string, uint8_t** packet, size_t* packet_size) {
    const uint8_t version = DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION;

    size_t json_length = strlen(project_description_json_string) + 1;
    *packet_size = PACKET_HEADER_SIZE * sizeof(uint8_t) + json_length;

    *packet = (uint8_t*)malloc(*packet_size);
    uint8_t* packet_content = *packet;
    packet_content[0] = version;
    packet_content[1] = DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION;
    memcpy(packet_content + PACKET_HEADER_SIZE, project_description_json_string, json_length);
}

DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE DecodePacket(const uint8_t* packet, size_t packet_size,
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

static void MakeHeaderOnlyPacket(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE type, uint8_t** packet, size_t* packet_size) {
    const uint8_t version = DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION;
    *packet_size = PACKET_HEADER_SIZE;
    *packet = (uint8_t*)malloc(*packet_size);
    uint8_t* packet_content = *packet;
    packet_content[0] = version;
    packet_content[1] = type;
}

void MakeRequestSubscriptionPacket(uint8_t** packet, size_t* packet_size) {
    MakeHeaderOnlyPacket(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_REQUEST, packet, packet_size);
}

void MakeSubscriptionResponsePacketHeader(uint8_t** packet, size_t* packet_size) {
    MakeHeaderOnlyPacket(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_RESPONSE, packet, packet_size);
}

void MakeForceStartDebuggerPacket(uint8_t** packet, size_t* packet_size) {
    MakeHeaderOnlyPacket(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_FORCE_DEBUGGER_START, packet, packet_size);
}

void MakeForceStopDebuggerPacket(uint8_t** packet, size_t* packet_size) {
    MakeHeaderOnlyPacket(DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_FORCE_DEBUGGER_STOP, packet, packet_size);
}

int FindNullTerminator(const uint8_t* packet, size_t packet_size, size_t* position) {
    for (size_t i = 0; i < packet_size; ++i) {
        if (packet[i] == '\0') {
            *position = i;
            return 1;
        }
    }
    return 0;
}