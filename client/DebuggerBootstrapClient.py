from re import L
import protocol.native_protocol as proto
import socket
import FileHasher
import json
import argparse
import sys

def MakeProjectDescription():
    return {"executable_name":"test_exe", "executable_hash": "abc", "link_dependencies_for_executable": [], "link_dependencies_for_executable_hashes": []}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="DebuggerBootstrapClient -- Connect to a remote DebuggerBootstrap instance to provide info about a project that will be debugged remotely.", 
    epilog="Report bugs to https://github.com/FrankGoyens/DebuggerBootstrap/issues.")
    parser.add_argument("HOST", default="localhost", type=str, help="Remote host of DebuggerBootstrap instance.")
    parser.add_argument("PORT", default=0, type=int, help="Port of the remote DebuggerBootstrap instance.")

    args = parser.parse_args()

    packet = proto.make_project_description_packet(json.dumps(MakeProjectDescription())) 

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((args.HOST, args.PORT))
            s.sendall(packet)
            print(s.recv(1024))
    except OverflowError as e:
        print("Error connecting to server: {}".format(e))
        exit(1)
    except ConnectionRefusedError as e:
        print("Error connecting to server: {}".format(e))
        exit(1)