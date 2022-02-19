cdef extern from "Protocol.h":
    cdef struct ProjectDescription:
        pass

    void MakeProjectDescriptionPacket(const char* proejct_description_json_string, char** packet, size_t* packet_size)