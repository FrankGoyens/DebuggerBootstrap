#pragma once

#include <stddef.h>
#include <stdint.h>

struct ProjectDescription;

#define DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION 0x1

void DivideIntoLowerAndUpperBytes(size_t value, uint8_t* lower, uint8_t* upper);
size_t CombineIntoValue(uint8_t lower, uint8_t upper);

void MakeProjectDescriptionPacket(struct ProjectDescription*, char** packet, size_t* packet_size);

enum DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE {
    DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION = 0,
    DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN
};

enum DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE DecodePacket(const char* packet, size_t packet_size,
                                                          size_t* json_part_offset, size_t* json_part_size);