import protocol.native_protocol as proto
import socket
import FileHasher

print(FileHasher.calculate_file_hash("/usr/bin/cmake"))

input_json = r'{"executable_name":"test_exe", "executable_hash": "abc", "link_dependencies_for_executable": [], "link_dependencies_for_executable_hashes": []}'
packet = proto.make_project_description_packet(input_json) 

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect(("localhost", 1337))
    s.sendall(packet)
    print(s.recv(1024))