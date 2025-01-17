from micropython import const

CSI = '\033['
OSC = '\033]'
BEL = '\a'


def code_to_chars(code):
    return CSI + str(code) + 'm'


def set_title(title):
    return OSC + '2;' + title + BEL


def clear_screen(mode=2):
    return CSI + str(mode) + 'J'


def clear_line(mode=2):
    return CSI + str(mode) + 'K'


class AnsiCodes(object):
    def __init__(self):
        # the subclasses declare class attributes which are numbers.
        # Upon instantiation, we define instance attributes, which are the same
        # as the class attributes but wrapped with the ANSI escape sequence
        for name in dir(self):
            if not name.startswith('_'):
                value = getattr(self, name)
                setattr(self, name, code_to_chars(value))


class AnsiFore(AnsiCodes):
    # @formatter:off
    BLACK       = 30
    RED         = 31
    GREEN       = 32
    YELLOW      = 33
    BLUE        = 34
    MAGENTA     = 35
    CYAN        = 36
    WHITE       = 37
    RESET       = 39
    # @formatter:on


class AnsiBack(AnsiCodes):
    # @formatter:off
    BLACK       = 40
    RED         = 41
    GREEN       = 42
    YELLOW      = 43
    BLUE        = 44
    MAGENTA     = 45
    CYAN        = 46
    WHITE       = 47
    RESET       = 49
    # @formatter:on


class AnsiStyle(AnsiCodes):
    # @formatter:off
    RESET_ALL   = 0
    BRIGHT      = 1
    DIM         = 2
    NORMAL      = 22
    # @formatter:on


Fore = AnsiFore()
Back = AnsiBack()
Style = AnsiStyle()


class Logger:
    # @formatter:off
    ERROR   = const(0)
    WARNING = const(1)
    INFO    = const(2)
    DEBUG   = const(3)
    TRACE   = const(4)
    # @formatter:on

    _colored_prefix: dict[int, str]

    def __init__(self, name: str, level=DEBUG):
        self._name = name
        self._level = level
        self._colored_prefix = {
            self.ERROR: f'[{self.set_color_text("ERR", Fore.WHITE, Back.RED)}]',
            self.WARNING: f'[{self.set_color_text("WRN", Fore.BLACK, Back.YELLOW)}]',
            self.INFO: f'[{self.set_color_text("INF", Fore.CYAN, Back.RESET)}]',
            self.DEBUG: f'[{self.set_color_text("DBG", Fore.WHITE, Back.BLUE)}]',
            self.TRACE: f'[{self.set_color_text("TRC", Fore.WHITE, Back.MAGENTA)}]',
        }

    @property
    def name(self):
        return self._name

    @property
    def level(self):
        return self._level

    @level.setter
    def level(self, new_level: int):
        if new_level not in self._colored_prefix.keys():
            raise ValueError(f'Wrong log level: {new_level}')
        self._level = new_level

    def _print_log(self, level: int, message: str):
        if level <= self.level:
            print(f'{self._colored_prefix.get(level)} {self.name}: {message}')

    def set_color(self, fore=Fore.RESET, back=Back.RESET):
        return f'{fore}{back}'

    def reset_color(self):
        return self.set_color()

    def set_color_text(self, text, fore=Fore.RESET, back=Back.RESET, ):
        return f'{self.set_color(fore, back)}{text}{self.reset_color()}'

    def error(self, message):
        self._print_log(self.ERROR, message)

    def warning(self, message):
        self._print_log(self.WARNING, message)

    def info(self, message):
        self._print_log(self.INFO, message)

    def debug(self, message):
        self._print_log(self.DEBUG, message)

    def trace(self, message):
        self._print_log(self.TRACE, message)
