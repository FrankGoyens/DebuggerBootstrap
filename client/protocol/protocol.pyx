from libc.stddef cimport size_t
cimport cprotocol

cdef class ProjectDescription:
    cdef cprotocol.ProjectDescription* description;

    def __cinit__(self):
        pass

    def __init__(self, executable_name, executable_hash, link_dependencies, link_dependencies_hashes):
        pass

    cdef const char* to_json(self):
        pass

def make_project_description_packet(ProjectDescription description):
    cdef char* packet
    cdef size_t packet_size
    cprotocol.MakeProjectDescriptionPacket(description.to_json(), &packet, &packet_size)