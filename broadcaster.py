import socket
import math
import time

UDP_IP = "255.255.255.255"
UDP_PORT = 4210

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

start = time.time()

while True:
    now = time.time() - start

    level = (math.sin(now * 4.0) + 1.0) / 2.0
    bass = (math.sin(now * 8.0) + 1.0) / 2.0
    mid = (math.sin(now * 3.0 + 1.5) + 1.0) / 2.0
    high = (math.sin(now * 11.0 + 0.7) + 1.0) / 2.0
    beat = 1 if bass > 0.92 else 0
    hue = int((now * 40) % 255)

    message = f"{level:.2f},{bass:.2f},{mid:.2f},{high:.2f},{beat},{hue}"
    sock.sendto(message.encode("utf-8"), (UDP_IP, UDP_PORT))

    time.sleep(1 / 40)