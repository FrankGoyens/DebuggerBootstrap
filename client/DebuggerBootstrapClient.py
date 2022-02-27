import protocol.native_protocol as proto
import socket
import json
import argparse
import selectors

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

    packet = proto.make_project_description_packet(json.dumps(MakeProjectDescription())) 

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.connect((args.HOST, args.PORT))
            s.setblocking(False)
            selector.register(s, selectors.EVENT_READ | selectors.EVENT_WRITE, data=None)
            sent = 0
            while True:
                events= selector.select(timeout=1)
                for key, mask in events:
                    if mask & selectors.EVENT_WRITE and sent < 5:
                        s.sendall(packet)
                        sent += 1
                    if mask &selectors.EVENT_READ:
                        print(s.recv(1024))
        except OverflowError as e:
            print("Error connecting to server: {}".format(e))
            exit(1)
        except ConnectionRefusedError as e:
            print("Error connecting to server: {}".format(e))
            exit(1)