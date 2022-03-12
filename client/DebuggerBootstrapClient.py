import protocol.native_protocol as proto
from protocol.MessageDecoder import MessageDecoder
import socket, json, argparse, selectors

class ClientSideMessageDecoder(MessageDecoder):
    def __init__(self, data):
        self.data = data

    def receive_subscription_response(self, packet_length, message_json):
        print("Recieved subscription response: {}".format(message_json))
        self.data = self.data[packet_length:]

    def receive_incomplete_response(self, incomplete_data_bytes):
        print("Got incomplete response: {}".format(incomplete_data_bytes))

    def receive_unknown_response(self, unknown_data_bytes):
        print("Got complete garbage: {}".format(unknown_data_bytes))
        self.data = bytes()

def _receiveServerData(data):
    decoder = ClientSideMessageDecoder(data)
    proto.decode_packet(data, decoder)
    return decoder.data

def MakeProjectDescription():
    return {"executable_name":"test_exe", "executable_hash": "abc", "link_dependencies_for_executable": [], "link_dependencies_for_executable_hashes": []}

def _make_argument_parser():
    parser = argparse.ArgumentParser(description="DebuggerBootstrapClient -- Connect to a remote DebuggerBootstrap instance to provide info about a project that will be debugged remotely.", 
    epilog="Report bugs to https://github.com/FrankGoyens/DebuggerBootstrap/issues.")
    parser.add_argument("HOST", default="localhost", type=str, help="Remote host of DebuggerBootstrap instance.")
    parser.add_argument("PORT", default=0, type=int, help="Port of the remote DebuggerBootstrap instance.")
    return parser

if __name__ == "__main__":
    parser = _make_argument_parser()
    args = parser.parse_args()

    selector = selectors.DefaultSelector()

    send_buffer = proto.make_subscribe_request_packet()
    # send_buffer += proto.make_project_description_packet(json.dumps(MakeProjectDescription())) 

    send_buffer += proto.make_force_start_debugger_packet()

    receive_buffer = bytes()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.connect((args.HOST, args.PORT))
            connected = True
            s.setblocking(False)
            selector.register(s, selectors.EVENT_READ | selectors.EVENT_WRITE, data=None)
            while connected:
                events = selector.select(timeout=1)
                for key, mask in events:
                    if mask & selectors.EVENT_WRITE:
                        sent_bytes = s.send(send_buffer)
                        send_buffer = send_buffer[sent_bytes:]
                        if not send_buffer:
                            selector.modify(s, selectors.EVENT_READ)
                    if mask &selectors.EVENT_READ:
                        message = s.recv(16)
                        if not message:
                            print("Connection to server is lost")
                            connected = False
                        receive_buffer += message
                        receive_buffer = _receiveServerData(receive_buffer)
                print("and another")
        except OverflowError as e:
            print("Error connecting to server: {}".format(e))
            exit(1)
        except ConnectionRefusedError as e:
            print("Error connecting to server: {}".format(e))
            exit(1)
        except KeyboardInterrupt:
            print("User requested exit through Ctrl+C")