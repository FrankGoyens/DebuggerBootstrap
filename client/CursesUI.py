import sys
import curses
import protocol.native_protocol as proto
from  protocol.MessageDecoder import MessageDecoder
import SubscriberUpdate
from ClientUIDescriptor import ClientUIDescriptor

PAD_HEIGHT = 100

class CursesClientUIDescriptor(ClientUIDescriptor):

    def __init__(self, stdscr, pad):
        self.stdscr = stdscr
        self.pad = pad
        self.pad_position = 0
        self._put_pad_on_screen()

    def _put_pad_on_screen(self):
        height, width = self.stdscr.getmaxyx()
        self.pad.refresh(self.pad_position, 0, 0, 0, height-1, width-1)

    def _clamp_pad_position(self):
        self.pad_position = 0 if self.pad_position < 0 else self.pad_position
        self.pad_position = PAD_HEIGHT-1 if self.pad_position >= PAD_HEIGHT else self.pad_position

    def get_ui_input_fd(self):
        return sys.stdin

    def ui_read_poll_iteration(self):
        key = self.stdscr.getkey()
        if key == "KEY_DOWN":
            self.pad_position += 1
        if key == "KEY_UP":
            self.pad_position -= 1
        self._clamp_pad_position()
        self._put_pad_on_screen()
        

def print_subscriber_update(subscriber_update_json, curses_screen):
    try:    
        subscriber_update = SubscriberUpdate.from_json(subscriber_update_json)
        # color_for_print = ClientConsoleColors.get_color_for_tag(subscriber_update.tag)
        # ClientConsole.set_color(color_for_print)
        # curses_screen.addstr(subscriber_update.message)
        # curses_screen.addstr(subscriber_update.message + "\n")
        # curses_screen.refresh()
        # ClientConsole.reset_color()
    except SubscriberUpdate.SubscriberUpdateException:
        pass

class CursesClientMessageDecoder(MessageDecoder):
    def __init__(self, data, curses_screen):
        self.data = data
        self.curses_screen = curses_screen


    def receive_subscription_response(self, packet_length, message_json):
        print_subscriber_update(message_json, self.curses_screen)
        self.data = self.data[packet_length:]

    def receive_incomplete_response(self, _):
        pass

    def receive_unknown_response(self, _):
        self.data = bytes()

def _create_decoder(data, stdscr):
    return CursesClientMessageDecoder(data, stdscr)

def _main(stdscr, connection_starter):
    # Clear screen
    stdscr.clear()
    
    _, width = stdscr.getmaxyx()
    pad = curses.newpad(PAD_HEIGHT, width)
    pad.scrollok(True)

    for i in range(0, 100):
        pad.addstr("Wat een mooi scherm zeg{}\n".format(i))
    
    connection_starter(lambda data: _create_decoder(data, stdscr), CursesClientUIDescriptor(stdscr, pad))
    

def start(connection_starter):
    curses.wrapper(lambda stdscr: _main(stdscr, connection_starter))

if __name__ == "__main__":
    start()