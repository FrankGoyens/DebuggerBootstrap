#include "protocol.h"

#include <stdlib.h>
#include <string.h>

#include "../ProjectDescription.h"
#include "../ProjectDescription_json.h"

void DivideIntoLowerAndUpperBytes(size_t value, uint8_t* lower, uint8_t* upper) {
    *lower = (uint8_t)value;
    *upper = (uint8_t)(value >> 8);
}

size_t CombineIntoValue(uint8_t lower, uint8_t upper) { return lower + upper * 256; }

void MakeProjectDescriptionPacket(struct ProjectDescription* description, char** packet, size_t* packet_size) {
    uint8_t version = DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION;

    char* project_description_json_string = ProjectDescriptionDumpToJSON(description);

    size_t json_length = strlen(project_description_json_string) + 1;
    uint8_t lower_json_length_byte, upper_json_length_byte;
    DivideIntoLowerAndUpperBytes(json_length, &lower_json_length_byte, &upper_json_length_byte);
    *packet_size = 4u * sizeof(uint8_t) + json_length;

    *packet = (char*)malloc(*packet_size);
    char* packet_content = *packet;
    packet_content[0] = version;
    packet_content[1] = DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION;
    packet_content[2] = lower_json_length_byte;
    packet_content[3] = upper_json_length_byte;
    memcpy(packet_content + 4, project_description_json_string, json_length);

    free(project_description_json_string);
}

enum DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE DecodePacket(const char* packet, size_t packet_size,
                                                          size_t* json_part_offset, size_t* json_part_size) {
    if (packet_size < 5) // Smaller than the header size
        return DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN;
    if (packet[0] != DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION)
        return DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN;
    *json_part_size = CombineIntoValue(packet[2], packet[3]);
    *json_part_offset = 4;
    return packet[1];
}