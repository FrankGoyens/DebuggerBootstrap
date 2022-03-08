from libc.stddef cimport size_t
from libc.stdint cimport uint8_t
from libc.stdlib cimport free
cimport cprotocol
from protocol.MessageDecoder import MessageDecoder

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

def decode_packet(packet_data, message_decoder):
    cdef bytes c_packet_data = packet_data
    cdef size_t json_offset
    cdef cprotocol.DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE packet_type = cprotocol.DecodePacket(c_packet_data, len(packet_data), &json_offset)
    cdef size_t null_terminator_position
    
    if packet_type == cprotocol.DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_RESPONSE:
        message_json_bytes = c_packet_data[json_offset:]
        if cprotocol.FindNullTerminator(message_json_bytes, len(message_json_bytes), &null_terminator_position):
            message_decoder.receive_subscription_response(null_terminator_position, message_json_bytes[json_offset:null_terminator_position])
        else:
            message_decoder.receive_incomplete_response(packet_data[cprotocol.PACKET_HEADER_SIZE:])
    elif packet_type == cprotocol.DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_INCOMPLETE:
        message_decoder.receive_incomplete_response(packet_data[cprotocol.PACKET_HEADER_SIZE:])
    elif packet_type == cprotocol.DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN:
        message_decoder.receive_unknown_response(packet_data[cprotocol.PACKET_HEADER_SIZE:])
    else:
        message_decoder.receive_unknown_response(packet_data[cprotocol.PACKET_HEADER_SIZE:])
