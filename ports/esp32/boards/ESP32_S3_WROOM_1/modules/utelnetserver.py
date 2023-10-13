import socket
import network
import uos
import errno

from uio import IOBase 
from machine import SPI, Pin


_last_client_socket = None
_server_socket = None

_spi = None
_lan = None

# Provide necessary functions for dupterm and replace telnet control characters that come in.
class TelnetWrapper(IOBase):
    def __init__(self, socket):
        self.socket = socket
        self.discard_count = 0
        
    def readinto(self, b):
        readbytes = 0
        for i in range(len(b)):
            try:
                byte = 0
                # discard telnet control characters and
                # null bytes 
                while(byte == 0):
                    byte = self.socket.recv(1)[0]
                    if byte == 0xFF:
                        self.discard_count = 2
                        byte = 0
                    elif self.discard_count > 0:
                        self.discard_count -= 1
                        byte = 0
                    
                b[i] = byte
                
                readbytes += 1
            except (IndexError, OSError) as e:
                if type(e) == IndexError or len(e.args) > 0 and e.args[0] == errno.EAGAIN:
                    if readbytes == 0:
                        return None
                    else:
                        return readbytes
                else:
                    raise
        return readbytes
    
    def write(self, data):
        # we need to write all the data but it's a non-blocking socket
        # so loop until it's all written eating EAGAIN exceptions
        while len(data) > 0:
            try:
                written_bytes = self.socket.write(data)
                data = data[written_bytes:]
            except OSError as e:
                if len(e.args) > 0 and e.args[0] == errno.EAGAIN:
                    # can't write yet, try again
                    pass
                else:
                    # something else...propagate the exception
                    raise
    
    def close(self):
        self.socket.close()

# Attach new clients to dupterm and 
# send telnet control characters to disable line mode
# and stop local echoing
def accept_telnet_connect(telnet_server):
    global _last_client_socket
    
    if _last_client_socket:
        # close any previous clients
        uos.dupterm(None)
        _last_client_socket.close()
    
    _last_client_socket, remote_addr = telnet_server.accept()
    print("Telnet connection from:", remote_addr)
    _last_client_socket.setblocking(False)
    # dupterm_notify() not available under MicroPython v1.1
    # _last_client_socket.setsockopt(socket.SOL_SOCKET, 20, uos.dupterm_notify)
    
    _last_client_socket.sendall(bytes([255, 252, 34])) # dont allow line mode
    _last_client_socket.sendall(bytes([255, 251, 1])) # turn off local echo
    
    uos.dupterm(TelnetWrapper(_last_client_socket))

def stop():
    global _server_socket, _last_client_socket
    uos.dupterm(None)
    if _server_socket:
        _server_socket.close()
    if _last_client_socket:
        _last_client_socket.close()

# start listening for telnet connections on port 23
def start(port=23):
    stop()
    global _lan, _server_socket
    _server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    _server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    ai = socket.getaddrinfo("0.0.0.0", port)
    addr = ai[0][4]
    
    _server_socket.bind(addr)
    _server_socket.listen(1)
    _server_socket.setsockopt(socket.SOL_SOCKET, 20, accept_telnet_connect)
    
    if _lan.active():
        print("Telnet server started on {}:{}".format(_lan.ifconfig()[0], port))


def lan_w5500(spi=None, cs_pin=None, int_pin=None):
    global _spi, _lan

    if _spi is None:
        _spi = spi or SPI(1, sck=Pin(14), mosi=Pin(13), miso=Pin(12))

    if _lan is None:
        _cs_pin = cs_pin or Pin(5)
        _int_pin = int_pin or Pin(4)

        _lan = network.LAN(phy_type=network.PHY_W5500, phy_addr=0, spi=_spi, int=_int_pin, cs=_cs_pin)
        
        _lan.config(mac='\x00\x02\x00\x11\x22\x33')
        _lan.active(True)

        _lan.ifconfig(('192.168.123.100', '255.255.255.0', '192.168.123.1', '8.8.8.8'))

    return _lan
