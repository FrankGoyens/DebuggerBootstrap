from libc.stdint cimport uint8_t

cdef extern from "Protocol.h":
    cdef struct ProjectDescription:
        pass

    void MakeProjectDescriptionPacket(const char* proejct_description_json_string, uint8_t** packet, size_t* packet_size)