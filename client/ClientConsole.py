import colorama

def init():
    colorama.init()

def deinit():
    colorama.deinit()

def set_color(color):
    """color should be obtained from ClientConsoleColors and may be None"""
    if color:
        print(color, end="")

def reset_color():
    print(colorama.Style.RESET_ALL, end="")
