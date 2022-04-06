# Should only serve as a configuration file

from colorama import Fore

def get_color_for_tag(tag):
    try:
        return {"GDB stdout": Fore.WHITE, 
            "GDB stderr": Fore.RED,
            "PROJECT DESCRIPTION": Fore.GREEN}[tag]
    except KeyError:
        return None