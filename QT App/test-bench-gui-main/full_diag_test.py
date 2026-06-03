"""
Full GUI diagnostic test - simulates all button clicks from MainWindow DiagnosticsTab.
Tests: PING, LED ON, LED OFF, LED TOGGLE, ECHO, GET_STATUS, GET_FW_VER
"""
import socket, struct, time

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

def parse_response(data):
    if len(data) < 7 or data[0] != 0x02:
        return None, b""
    cmd = data[1]
    length = struct.unpack_from('<H', data, 2)[0]
    payload = data[4:4+length]
    return cmd, payload

def send_recv(sock, cmd, payload=b"", label=""):
    frame = build_frame(cmd, payload)
    t0 = time.perf_counter()
    sock.sendall(frame)
    resp = sock.recv(256)
    dt = (time.perf_counter() - t0) * 1000
    rcmd, rpayload = parse_response(resp)
    return rcmd, rpayload, dt

PASS = "PASS"
FAIL = "FAIL"
results = []

print("=" * 55)
print("  TEST BENCH – Full Diagnostic Session")
print("=" * 55)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(5)
sock.connect(("169.254.169.110", 5000))
print(f"\n{PASS} TCP connected to 169.254.169.110:5000\n")

# 1. PING -> PONG
rcmd, rpl, dt = send_recv(sock, 0x01, label="PING")
ok = rcmd == 0x02
results.append(ok)
print(f"{'PING -> PONG':<30} {PASS if ok else FAIL}  ({dt:.1f} ms)")

# 2. LED ON
rcmd, rpl, dt = send_recv(sock, 0x05, label="LED_ON")
ok = rcmd == 0x08 and len(rpl) >= 1 and rpl[0] == 1
results.append(ok)
state = "ON" if (rpl and rpl[0]) else "?"
print(f"{'LED ON -> ACK':<30} {PASS if ok else FAIL}  LED={state}")

# 3. LED OFF
rcmd, rpl, dt = send_recv(sock, 0x06, label="LED_OFF")
ok = rcmd == 0x08 and len(rpl) >= 1 and rpl[0] == 0
results.append(ok)
state = "ON" if (rpl and rpl[0]) else "OFF"
print(f"{'LED OFF -> ACK':<30} {PASS if ok else FAIL}  LED={state}")

# 4. LED TOGGLE
rcmd, rpl, dt = send_recv(sock, 0x07, label="LED_TOGGLE")
ok = rcmd == 0x08 and len(rpl) >= 1 and rpl[0] == 1
results.append(ok)
state = "ON" if (rpl and rpl[0]) else "OFF"
print(f"{'LED TOGGLE -> ACK':<30} {PASS if ok else FAIL}  LED={state}")

# 5. ECHO_REQ
payload = b"Hello STM32!"
rcmd, rpl, dt = send_recv(sock, 0x03, payload, label="ECHO")
ok = rcmd == 0x04 and rpl == payload
results.append(ok)
decoded = rpl.decode(errors="replace")
print(f"{'ECHO_REQ -> ECHO_RSP':<30} {PASS if ok else FAIL}  '{decoded}'")

# 6. GET_STATUS
rcmd, rpl, dt = send_recv(sock, 0x09, label="GET_STATUS")
ok = rcmd == 0x0A and len(rpl) >= 1
results.append(ok)
if rpl:
    flags = rpl[0]
    eth = bool(flags & 0x01)
    can = bool(flags & 0x02)
    tnsy = bool(flags & 0x04)
    led = bool(flags & 0x08)
    print(f"{'GET_STATUS -> STATUS_RSP':<30} {PASS if ok else FAIL}  ETH={eth} CAN={can} Teensy={tnsy} LED={led}")

# 7. GET_FW_VER
rcmd, rpl, dt = send_recv(sock, 0x0B, label="GET_FW_VER")
ok = rcmd == 0x0C and len(rpl) >= 3
results.append(ok)
ver = f"{rpl[0]}.{rpl[1]}.{rpl[2]}" if rpl and len(rpl) >= 3 else "?"
print(f"{'GET_FW_VER -> FW_VER_RSP':<30} {PASS if ok else FAIL}  v{ver}")

# 8. NACK (unknown command)
rcmd, rpl, dt = send_recv(sock, 0xAA, label="NACK_TEST")
ok = rcmd == 0xFF
results.append(ok)
print(f"{'Unknown CMD -> NACK':<30} {PASS if ok else FAIL}")

sock.close()

print("\n" + "=" * 55)
passed = sum(results)
total = len(results)
print(f"  WYNIK: {passed}/{total} testów zaliczonych")
if passed == total:
    print(f"\n  {PASS} WSZYSTKIE TESTY DIAGNOSTYCZNE ZAKOŃCZONE SUKCESEM!")
else:
    print(f"\n  {FAIL} {total - passed} testów nie powiodło się!")
print("=" * 55)
