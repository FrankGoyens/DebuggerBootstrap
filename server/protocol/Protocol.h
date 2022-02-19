#pragma once

#include <stddef.h>
#include <stdint.h>

struct ProjectDescription;

#define DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION 0x1
#define PACKET_HEADER_SIZE 2

void MakeProjectDescriptionPacket(const char* project_description_json_string, char** packet, size_t* packet_size);

enum DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE {
    DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION = 0,
    DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_REQUEST,
    DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_RESPONSE,
    DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_INCOMPLETE,
    DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN
};

enum DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE DecodePacket(const char* packet, size_t packet_size,
                                                          size_t* json_part_offset);

void MakeRequestSubscriptionPacket(char** packet, size_t* packet_size);
// This is just a header, any human readable utf-8 content may be appended to the output stream after putting this
// packet in the outputstream, a '\0' indicates the end of the packet
void MakeSubscriptionResponsePacketHeader(char** packet, size_t* packet_size);

size_t FindNullTerminator(const char* packet, size_t packet_size);