"""Sends fake FH6 Data Out packets, to test the editor without the game.

    python3 editor/tests/fake_dataout.py [port] [seconds]

Drives a steady right-hand drift circle: about 55 km/h, 30 degrees of angle.
"""
import math, socket, struct, sys, time

port = int(sys.argv[1]) if len(sys.argv) > 1 else 5607
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 30
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

R, speed, drift = 25.0, 15.3, math.radians(-30)  # drift < 0: velocity left of the nose (right-hand drift)
t0 = time.time()
while time.time() - t0 < seconds:
    t = time.time() - t0
    travel = speed / R * t                       # turning right: heading angle increases
    x, z = R * (1 - math.cos(travel)), R * math.sin(travel)
    pkt = bytearray(324)
    struct.pack_into('<iI', pkt, 0, 1, int(t * 1000))
    lat = speed * speed / R                      # centripetal, to the right
    struct.pack_into('<3f', pkt, 20, lat * math.cos(drift), 0, -lat * math.sin(drift))           # accel, car space
    struct.pack_into('<3f', pkt, 32, speed * math.sin(drift), 0, speed * math.cos(drift))       # velocity, car space
    struct.pack_into('<3f', pkt, 56, travel - drift, 0, 0)                                        # yaw, pitch, roll
    struct.pack_into('<4f', pkt, 164, 0.3, 0.3, -3.8, -3.8)                                       # tyre slip angle
    struct.pack_into('<3f', pkt, 244, x, 0, z)                                                    # position
    struct.pack_into('<f', pkt, 256, speed)
    pkt[315] = 200                                                                                # throttle
    s.sendto(pkt, ('127.0.0.1', port))
    time.sleep(1 / 60)
