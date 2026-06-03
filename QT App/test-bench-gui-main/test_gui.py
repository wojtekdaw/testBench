"""
test_gui.py - Test połączenia REST API
Zamiennik starego skryptu opartego na tcp_client/protocol.
"""
import sys
import requests
from PyQt6.QtWidgets import QApplication, QLabel, QVBoxLayout, QWidget
from PyQt6.QtCore import QTimer

BASE_URL = "http://169.254.169.110/api"

def run_test():
    try:
        resp = requests.get(f"{BASE_URL}/ping", timeout=2)
        data = resp.json()
        if data.get("status") == "ok" and data.get("message") == "pong":
            print("Diagnostic test PASSED! Received PONG from STM32 over HTTP REST!")
            sys.exit(0)
        else:
            print(f"Unexpected response: {data}")
            sys.exit(1)
    except requests.exceptions.RequestException as e:
        print(f"Connection error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    QTimer.singleShot(0, run_test)
    sys.exit(app.exec())
