from libc.stdint cimport uint8_t
from libc.stddef cimport size_t

cdef extern from "Protocol.h":
    cdef struct ProjectDescription:
        pass

    ctypedef enum DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE:
        DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION,
        DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_REQUEST,
        DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_RESPONSE,
        DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_FORCE_DEBUGGER_START,
        DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_FORCE_DEBUGGER_STOP,
        DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_INCOMPLETE,
        DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN

    cdef size_t PACKET_HEADER_SIZE

    DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE DecodePacket(const uint8_t* packet, size_t packet_size, size_t* json_part_offset)
    void MakeRequestSubscriptionPacket(uint8_t** packet, size_t* packet_size)
    void MakeProjectDescriptionPacket(const char* proejct_description_json_string, uint8_t** packet, size_t* packet_size)
    void MakeForceStartDebuggerPacket(uint8_t** packet, size_t* packet_size)
    void MakeForceStopDebuggerPacket(uint8_t** packet, size_t* packet_size)
    int FindNullTerminator(const uint8_t* packet, size_t packet_size, size_t* position)