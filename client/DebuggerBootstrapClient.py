import protocol.native_protocol as proto
from protocol.MessageDecoder import MessageDecoder
import socket, json, argparse, selectors, sys, os
import SubscriberUpdate
import ClientConsoleColors, ClientConsole
import ProjectDescription
import __main__

def print_subscriber_update(subscriber_update_json):
    try:    
        subscriber_update = SubscriberUpdate.from_json(subscriber_update_json)
        color_for_print = ClientConsoleColors.get_color_for_tag(subscriber_update.tag)
        ClientConsole.set_color(color_for_print)
        print(subscriber_update.message)
        ClientConsole.reset_color()
    except SubscriberUpdate.SubscriberUpdateException:
        pass

class ClientSideMessageDecoder(MessageDecoder):
    def __init__(self, data):
        self.data = data

    def receive_subscription_response(self, packet_length, message_json):
        print_subscriber_update(message_json)
        self.data = self.data[packet_length:]

    def receive_incomplete_response(self, _):
        pass

    def receive_unknown_response(self, _):
        self.data = bytes()

def _receiveServerData(data):
    decoder = ClientSideMessageDecoder(data)
    proto.decode_packet(data, decoder)
    return decoder.data

def _make_argument_parser():
    parser = argparse.ArgumentParser(description="DebuggerBootstrapClient -- Connect to a remote DebuggerBootstrap instance to provide info about a project that will be debugged remotely.", 
    epilog="Please report bugs to https://github.com/FrankGoyens/DebuggerBootstrap/issues")
    parser.add_argument("executable_to_debug", type=str, help="The executable file that will be debugged remotely.")
    parser.add_argument("-s", "--server", type=str, help="Remote host of DebuggerBootstrap instance.")
    parser.add_argument("-p", "--port", type=int, help="Port of the remote DebuggerBootstrap instance.")
    parser.add_argument("--no-interactive", default=False, action="store_true", help="The user will not be prompted to enter missing data. When data is missing the program will exit with a failure status.")
    return parser

RECEIVE_BUFFER_SIZE=16

def start_connection(host, port, project_description):

    selector = selectors.DefaultSelector()

    send_buffer = proto.make_subscribe_request_packet()
    send_buffer += proto.make_project_description_packet(json.dumps(project_description)) 

    # send_buffer += proto.make_force_start_debugger_packet()
    # send_buffer += proto.make_force_stop_debugger_packet()

    receive_buffer = bytes()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.connect((host, port))
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
                        message = s.recv(RECEIVE_BUFFER_SIZE)
                        if not message:
                            print("Connection to server is lost")
                            connected = False
                        receive_buffer += message
                        receive_buffer = _receiveServerData(receive_buffer)
        except OverflowError as e:
            print("Error connecting to server: {}".format(e))
            exit(1)
        except ConnectionRefusedError as e:
            print("Error connecting to server: {}".format(e))
            exit(1)

def _get_default_if_missing(arg, args, default):
    return getattr(args, arg, default)

class MissingParameterException(Exception):
    def __init__(self, parameter):
        self.parameter = parameter

def _get_or_report_if_missing(arg, args):
    value = getattr(args, arg, None)
    if value is None:
        raise MissingParameterException(arg)
    return value

def _prompt_user_for_missing_parameter(parameter, args, validate):
    value = getattr(args, parameter, None)
    if value is not None:
        return value
    while True:
        print("Please enter a value for parameter '{}': ".format(parameter), end="")
        value = input()
        if validate(value):
            break
        print("Value is invalid for '{0}': {1}".format(parameter, value), file=sys.stderr)
    return value

def _is_non_interactive(args):
    return args.no_interactive 

def _validate_is_number(value):
    try:
        int(value)
        return True
    except ValueError:
        return False

def gather_missing_input_data(args):
    gathered_args = {}
    if _is_non_interactive(args):
        gathered_args["server"] = _get_default_if_missing("server", args, "localhost")
        gathered_args["port"] = _get_or_report_if_missing("port", args)
    else:
        gathered_args["server"] = _prompt_user_for_missing_parameter("server", args, lambda _: True)
        gathered_args["port"] = int(_prompt_user_for_missing_parameter("port", args, _validate_is_number))
    return gathered_args
    

def _exit_when_remaining_arguments_cant_be_gathered(args):
    try:
        return gather_missing_input_data(args)
    except MissingParameterException as e:
        print("Missing value for '{}'".format(e.parameter))
        exit(1)

if __name__ == "__main__":
    parser = _make_argument_parser()
    args = parser.parse_args()
    ClientConsole.init()

    if not os.path.isfile(args.executable_to_debug):
        print("Given executable to debug: '{}' does not exist. Exiting...".format(args.executable_to_debug), file=sys.stderr)
        exit(1)

    project_description = ProjectDescription.gather_recursively_from_current_dir(args.executable_to_debug)

    if project_description is None:
        print("Unknown error gathering project description", file=sys.stderr)
        exit(1)

    gathered_args = _exit_when_remaining_arguments_cant_be_gathered(args)

    try:
       start_connection(gathered_args["server"], gathered_args["port"], project_description)
    except KeyboardInterrupt:
        print("User requested exit through Ctrl+C")