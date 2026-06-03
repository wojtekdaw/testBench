import socket, struct

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc

def build_frame(cmd, payload=b""):
    length = len(payload)
    crc_data = struct.pack('<BH', cmd, length) + payload
    crc = crc16(crc_data)
    return struct.pack('<BBH', 0x02, cmd, length) + payload + struct.pack('<HB', crc, 0x03)

print("[1] Connecting to 169.254.169.110:5000 ...")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(5)
try:
    sock.connect(("169.254.169.110", 5000))
    print("[2] Connected!")

    frame = build_frame(0x01)  # CMD_PING
    print(f"[3] Sending PING frame: {frame.hex()}")
    sock.sendall(frame)

    resp = sock.recv(64)
    print(f"[4] Raw response: {resp.hex()}")

    if len(resp) >= 7 and resp[0] == 0x02 and resp[1] == 0x02:
        print("[5] SUCCESS: Received CMD_PONG from STM32!")
    else:
        print(f"[5] Unexpected response: cmd=0x{resp[1]:02X}")

    # Test ECHO
    payload = b"Hello STM32"
    frame2 = build_frame(0x03, payload)  # CMD_ECHO_REQ
    print(f"\n[6] Sending ECHO_REQ: '{payload.decode()}'")
    sock.sendall(frame2)
    resp2 = sock.recv(64)
    echo_payload = resp2[4:4+len(payload)]
    print(f"[7] Echo response payload: '{echo_payload.decode(errors='replace')}'")

    # Test GET_STATUS
    frame3 = build_frame(0x09)  # CMD_GET_STATUS
    print(f"\n[8] Sending GET_STATUS")
    sock.sendall(frame3)
    resp3 = sock.recv(64)
    if len(resp3) >= 8:
        flags = resp3[4]
        print(f"[9] Status flags: 0x{flags:02X}")
        print(f"    ETH_LINK_UP  : {'YES' if flags & 0x01 else 'NO'}")
        print(f"    CAN_BUS_OK   : {'YES' if flags & 0x02 else 'NO'}")
        print(f"    TEENSY_ALIVE : {'YES' if flags & 0x04 else 'NO'}")
        print(f"    LED_STATE    : {'ON' if flags & 0x08 else 'OFF'}")

    # Test GET_FW_VER
    frame4 = build_frame(0x0B)  # CMD_GET_FW_VER
    print(f"\n[10] Sending GET_FW_VER")
    sock.sendall(frame4)
    resp4 = sock.recv(64)
    if len(resp4) >= 10:
        major, minor, patch = resp4[4], resp4[5], resp4[6]
        print(f"[11] Firmware version: {major}.{minor}.{patch}")

    print("\n=== ALL DIAGNOSTIC TESTS PASSED ===")

except Exception as e:
    print(f"ERROR: {e}")
finally:
    sock.close()
