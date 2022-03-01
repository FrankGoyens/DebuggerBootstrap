from libc.stddef cimport size_t
from libc.stdint cimport uint8_t
from libc.stdlib cimport free
cimport cprotocol

def make_project_description_packet(description_json):
    cdef bytes json_bytes = description_json.encode("UTF-8")
    cdef uint8_t* packet
    cdef size_t packet_size
    cprotocol.MakeProjectDescriptionPacket(json_bytes, &packet, &packet_size)
    cdef bytes py_packet = bytes([packet[i] for i in range(0, packet_size)])
    free(packet)
    return py_packet

def make_subscribe_request_packet():
    cdef uint8_t* packet
    cdef size_t packet_size
    cprotocol.MakeRequestSubscriptionPacket(&packet, &packet_size)
    cdef bytes py_packet = bytes([packet[i] for i in range(0, packet_size)])
    free(packet)
    return py_packet