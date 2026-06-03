"""
Test Bench – Main Window
========================
PyQt6 application with tab-based UI.
First tab: Diagnostics (ping, LED, echo, connection status).
"""

import sys
import time
import platform
from datetime import datetime

from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QFont, QColor, QIcon
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QTabWidget, QWidget,
    QVBoxLayout, QHBoxLayout, QGridLayout, QGroupBox,
    QPushButton, QLabel, QLineEdit, QTextEdit, QSpinBox,
    QFrame, QSizePolicy, QMessageBox,
)

from rest_client import RestClient


# ---------------------------------------------------------------------------
# Styles
# ---------------------------------------------------------------------------
STYLE_SHEET = """
QMainWindow {
    background-color: #1e1e2e;
}
QTabWidget::pane {
    border: 1px solid #45475a;
    background-color: #1e1e2e;
    border-radius: 4px;
}
QTabBar::tab {
    background-color: #313244;
    color: #cdd6f4;
    padding: 8px 20px;
    margin-right: 2px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    font-weight: bold;
}
QTabBar::tab:selected {
    background-color: #45475a;
    color: #89b4fa;
}
QGroupBox {
    font-weight: bold;
    color: #89b4fa;
    border: 1px solid #45475a;
    border-radius: 6px;
    margin-top: 12px;
    padding-top: 14px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 6px;
}
QPushButton {
    background-color: #313244;
    color: #cdd6f4;
    border: 1px solid #45475a;
    border-radius: 4px;
    padding: 8px 16px;
    font-weight: bold;
    min-width: 100px;
}
QPushButton:hover {
    background-color: #45475a;
    border-color: #89b4fa;
}
QPushButton:pressed {
    background-color: #585b70;
}
QPushButton:disabled {
    background-color: #1e1e2e;
    color: #585b70;
    border-color: #313244;
}
QPushButton#btn_connect {
    background-color: #a6e3a1;
    color: #1e1e2e;
}
QPushButton#btn_connect:hover {
    background-color: #94e2d5;
}
QPushButton#btn_disconnect {
    background-color: #f38ba8;
    color: #1e1e2e;
}
QPushButton#btn_disconnect:hover {
    background-color: #eba0ac;
}
QLineEdit, QSpinBox {
    background-color: #313244;
    color: #cdd6f4;
    border: 1px solid #45475a;
    border-radius: 4px;
    padding: 6px;
}
QLineEdit:focus, QSpinBox:focus {
    border-color: #89b4fa;
}
QTextEdit {
    background-color: #11111b;
    color: #a6adc8;
    border: 1px solid #45475a;
    border-radius: 4px;
    font-family: 'Menlo', 'Consolas', 'Courier New', monospace;
    font-size: 11px;
}
QLabel {
    color: #cdd6f4;
}
QLabel#lbl_status_val {
    font-weight: bold;
    font-size: 13px;
}
"""


# ---------------------------------------------------------------------------
# Status indicator widget
# ---------------------------------------------------------------------------
class StatusIndicator(QFrame):
    """Small colored circle indicating ON/OFF state."""

    def __init__(self, size: int = 14, parent=None):
        super().__init__(parent)
        self.setFixedSize(size, size)
        self._on = False
        self._update_style()

    def set_on(self, state: bool) -> None:
        self._on = state
        self._update_style()

    def _update_style(self) -> None:
        color = "#a6e3a1" if self._on else "#585b70"
        self.setStyleSheet(
            f"background-color: {color}; border-radius: {self.width() // 2}px;"
        )


# ---------------------------------------------------------------------------
# Diagnostics Tab
# ---------------------------------------------------------------------------
class DiagnosticsTab(QWidget):
    """Tab with diagnostic tools for STM32 communication."""

    def __init__(self, client: RestClient, parent=None):
        super().__init__(parent)
        self._client = client
        self._ping_t0: float = 0.0
        self._echo_payload: bytes = b""
        self._init_ui()

    # ------------------------------------------------------------------
    # UI construction
    # ------------------------------------------------------------------
    def _init_ui(self) -> None:
        main_layout = QVBoxLayout(self)
        main_layout.setSpacing(10)

        # ── Connection group ──────────────────────────────────────────
        grp_conn = QGroupBox("TCP Connection")
        gl = QGridLayout(grp_conn)

        gl.addWidget(QLabel("Adres IP:"), 0, 0)
        self.edt_ip = QLineEdit(RestClient.DEFAULT_IP)
        self.edt_ip.setPlaceholderText("np. 169.254.169.110")
        gl.addWidget(self.edt_ip, 0, 1)

        gl.addWidget(QLabel("Port:"), 0, 2)
        self.spn_port = QSpinBox()
        self.spn_port.setRange(1, 65535)
        self.spn_port.setValue(RestClient.DEFAULT_PORT)
        gl.addWidget(self.spn_port, 0, 3)

        self.btn_connect = QPushButton("Connect")
        self.btn_connect.setObjectName("btn_connect")
        self.btn_connect.clicked.connect(self._on_connect)
        gl.addWidget(self.btn_connect, 0, 4)

        self.btn_disconnect = QPushButton("Disconnect")
        self.btn_disconnect.setObjectName("btn_disconnect")
        self.btn_disconnect.setEnabled(False)
        self.btn_disconnect.clicked.connect(self._on_disconnect)
        gl.addWidget(self.btn_disconnect, 0, 5)

        # Status row
        gl.addWidget(QLabel("Status:"), 1, 0)
        h_status = QHBoxLayout()
        self.ind_conn = StatusIndicator()
        h_status.addWidget(self.ind_conn)
        self.lbl_status = QLabel("Disconnected")
        self.lbl_status.setObjectName("lbl_status_val")
        h_status.addWidget(self.lbl_status)
        h_status.addStretch()
        self.lbl_latency = QLabel("")
        h_status.addWidget(self.lbl_latency)
        gl.addLayout(h_status, 1, 1, 1, 5)

        main_layout.addWidget(grp_conn)

        # ── Diagnostic tools group ────────────────────────────────────
        grp_diag = QGroupBox("Diagnostic tools")
        diag_layout = QGridLayout(grp_diag)
        diag_layout.setSpacing(8)

        # Ping
        self.btn_ping = QPushButton("PING")
        self.btn_ping.setToolTip("Wyślij PING i zmierz czas odpowiedzi (PONG)")
        self.btn_ping.clicked.connect(self._on_ping)
        self.btn_ping.setEnabled(False)
        diag_layout.addWidget(self.btn_ping, 0, 0)
        self.lbl_ping_result = QLabel("—")
        diag_layout.addWidget(self.lbl_ping_result, 0, 1)

        # LED control
        self.btn_led_on = QPushButton("LED ON")
        self.btn_led_on.setToolTip("Włącz diodę LED na Nucleo (LD1 – zielona, PB0)")
        self.btn_led_on.clicked.connect(lambda: self._send_led("on"))
        self.btn_led_on.setEnabled(False)
        diag_layout.addWidget(self.btn_led_on, 1, 0)

        self.btn_led_off = QPushButton("LED OFF")
        self.btn_led_off.setToolTip("Wyłącz diodę LED na Nucleo (LD1 – zielona, PB0)")
        self.btn_led_off.clicked.connect(lambda: self._send_led("off"))
        self.btn_led_off.setEnabled(False)
        diag_layout.addWidget(self.btn_led_off, 1, 1)

        self.btn_led_toggle = QPushButton("LED TOGGLE")
        self.btn_led_toggle.setToolTip("Przełącz diodę LED na Nucleo (LD1 – zielona, PB0)")
        self.btn_led_toggle.clicked.connect(lambda: self._send_led("toggle"))
        self.btn_led_toggle.setEnabled(False)
        diag_layout.addWidget(self.btn_led_toggle, 1, 2)

        self.ind_led = StatusIndicator(size=18)
        diag_layout.addWidget(self.ind_led, 1, 3, Qt.AlignmentFlag.AlignCenter)
        self.lbl_led = QLabel("LED: ?")
        diag_layout.addWidget(self.lbl_led, 1, 4)

        # Echo test
        self.btn_echo = QPushButton("ECHO TEST")
        self.btn_echo.setToolTip("Wyślij dane i sprawdź czy STM32 odesłał identyczną kopię")
        self.btn_echo.clicked.connect(self._on_echo)
        self.btn_echo.setEnabled(False)
        diag_layout.addWidget(self.btn_echo, 2, 0)

        self.edt_echo = QLineEdit("Hello STM32!")
        self.edt_echo.setPlaceholderText("Dane do wysłania…")
        diag_layout.addWidget(self.edt_echo, 2, 1, 1, 3)
        self.lbl_echo_result = QLabel("—")
        diag_layout.addWidget(self.lbl_echo_result, 2, 4)

        # Get status
        self.btn_status = QPushButton("GET STATUS")
        self.btn_status.setToolTip("Pobierz bieżący status STM32 (ETH, CAN, Teensy)")
        self.btn_status.clicked.connect(self._on_get_status)
        self.btn_status.setEnabled(False)
        diag_layout.addWidget(self.btn_status, 3, 0)

        # Status indicators
        status_row = QHBoxLayout()
        self.ind_eth = StatusIndicator()
        status_row.addWidget(self.ind_eth)
        status_row.addWidget(QLabel("ETH"))
        status_row.addSpacing(12)
        self.ind_can = StatusIndicator()
        status_row.addWidget(self.ind_can)
        status_row.addWidget(QLabel("CAN"))
        status_row.addSpacing(12)
        self.ind_teensy = StatusIndicator()
        status_row.addWidget(self.ind_teensy)
        status_row.addWidget(QLabel("Teensy"))
        status_row.addStretch()
        diag_layout.addLayout(status_row, 3, 1, 1, 4)

        main_layout.addWidget(grp_diag)

        # ── Log console ───────────────────────────────────────────────
        grp_log = QGroupBox("Log console")
        log_layout = QVBoxLayout(grp_log)
        self.txt_log = QTextEdit()
        self.txt_log.setReadOnly(True)
        self.txt_log.document().setMaximumBlockCount(500)
        log_layout.addWidget(self.txt_log)

        btn_row = QHBoxLayout()
        self.btn_clear_log = QPushButton("Clear logs")
        self.btn_clear_log.clicked.connect(self.txt_log.clear)
        btn_row.addStretch()
        btn_row.addWidget(self.btn_clear_log)
        log_layout.addLayout(btn_row)

        main_layout.addWidget(grp_log)

    # ------------------------------------------------------------------
    # Logging
    # ------------------------------------------------------------------
    def _log(self, msg: str, color: str = "#cdd6f4") -> None:
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.txt_log.append(
            f'<span style="color:#6c7086">[{ts}]</span> '
            f'<span style="color:{color}">{msg}</span>'
        )

    # ------------------------------------------------------------------
    # Connection
    # ------------------------------------------------------------------
    def _on_connect(self) -> None:
        ip = self.edt_ip.text().strip()
        port = self.spn_port.value()
        if not ip:
            QMessageBox.warning(self, "Błąd", "Podaj adres IP!")
            return

        self._client.configure(ip, port)

        # Connect signals
        self._client.connected.connect(self._on_connected)
        self._client.disconnected.connect(self._on_disconnected)
        self._client.response_received.connect(self._on_response_received)
        self._client.error_occurred.connect(self._on_error)
        self._client.latency_measured.connect(self._on_latency)

        self._log(f"Łączenie z {ip}:{port}…", "#f9e2af")
        self._client.start()

    def _on_disconnect(self) -> None:
        if self._client:
            self._client.disconnect_from_host()

    def _on_connected(self, addr: str) -> None:
        self._log(f"Connected to {addr}", "#a6e3a1")
        self.ind_conn.set_on(True)
        self.lbl_status.setText(f"Connected ({addr})")
        self.lbl_status.setStyleSheet("color: #a6e3a1;")
        self._set_diag_buttons(True)
        self.btn_connect.setEnabled(False)
        self.btn_disconnect.setEnabled(True)
        self.edt_ip.setEnabled(False)
        self.spn_port.setEnabled(False)

    def _on_disconnected(self, reason: str) -> None:
        self._log(f"Disconnected: {reason}", "#f38ba8")
        self.ind_conn.set_on(False)
        self.lbl_status.setText("Disconnected")
        self.lbl_status.setStyleSheet("color: #f38ba8;")
        self._set_diag_buttons(False)
        self.btn_connect.setEnabled(True)
        self.btn_disconnect.setEnabled(False)
        self.edt_ip.setEnabled(True)
        self.spn_port.setEnabled(True)
        self._client = None

    def _on_error(self, msg: str) -> None:
        self._log(f"ERROR: {msg}", "#f38ba8")

    def _on_latency(self, ms: float) -> None:
        self.lbl_latency.setText(f"Latency: {ms:.1f} ms")

    def _set_diag_buttons(self, enabled: bool) -> None:
        for btn in (
            self.btn_ping, self.btn_led_on, self.btn_led_off,
            self.btn_led_toggle, self.btn_echo, self.btn_status,
        ):
            btn.setEnabled(enabled)

    # ------------------------------------------------------------------
    # Diagnostic actions
    # ------------------------------------------------------------------
    def _send_led(self, action: str) -> None:
        if self._client and self._client.is_connected:
            self._client.post_led(action)
            self._log(f"TX → POST /api/led action={action}")

    def _on_ping(self) -> None:
        self._ping_t0 = time.perf_counter()
        if self._client and self._client.is_connected:
            self._client.get_ping()
            self._log("TX → GET /api/ping")

    def _on_echo(self) -> None:
        text = self.edt_echo.text()
        if not text:
            return
        self._echo_payload = text.encode("utf-8")
        if self._client and self._client.is_connected:
            self._client.post_echo(text)
            self._log(f"TX → POST /api/echo data={text}")

    def _on_get_status(self) -> None:
        if self._client and self._client.is_connected:
            self._client.get_status()
            self._log("TX → GET /api/status")

    # ------------------------------------------------------------------
    # Frame handler
    # ------------------------------------------------------------------
    def _on_response_received(self, endpoint: str, data: dict) -> None:
        self._log(f"RX ← {endpoint} JSON={data}", "#89b4fa")

        if endpoint == "/ping":
            dt = (time.perf_counter() - self._ping_t0) * 1000.0
            self.lbl_ping_result.setText(f"PONG  ({dt:.1f} ms)")
            self.lbl_ping_result.setStyleSheet("color: #a6e3a1;")
            self._log(f"PONG ← RTT = {dt:.1f} ms", "#a6e3a1")

        elif endpoint == "/echo":
            if data.get("echo") == self._echo_payload.decode("utf-8"):
                self.lbl_echo_result.setText("OK — data correct")
                self.lbl_echo_result.setStyleSheet("color: #a6e3a1;")
                self._log("ECHO OK — payload correct", "#a6e3a1")
            else:
                self.lbl_echo_result.setText("FAIL — data differ!")
                self.lbl_echo_result.setStyleSheet("color: #f38ba8;")
                self._log("ECHO FAIL — payload niezgodny!", "#f38ba8")

        elif endpoint == "/led":
            if data.get("status") == "ok":
                led_state = bool(data.get("led_state", 0))
                self.ind_led.set_on(led_state)
                self.lbl_led.setText(f"LED: {'ON' if led_state else 'OFF'}")
                self._log(
                    f"LED ACK → {'ON' if led_state else 'OFF'}",
                    "#a6e3a1" if led_state else "#6c7086",
                )
            else:
                self.ind_led.set_on(True)
                self.lbl_led.setText("LED: Error")
                self._log("LED error")

        elif endpoint == "/status":
            if data.get("status") == "ok":
                eth_ok = bool(data.get("eth_up", False))
                can_ok = False # To be implemented
                teensy_ok = False # To be implemented
                led = bool(data.get("led_state", 0))
                self.ind_eth.set_on(eth_ok)
                self.ind_can.set_on(can_ok)
                self.ind_teensy.set_on(teensy_ok)
                self.ind_led.set_on(led)
                self.lbl_led.setText(f"LED: {'ON' if led else 'OFF'}")
                self._log(
                    f"STATUS → ETH={'OK' if eth_ok else 'DOWN'}  "
                    f"LED={'ON' if led else 'OFF'}",
                    "#a6e3a1",
                )
        # Update Control Tab as well
        main_win = self.parent().parent() if self.parent() else None
        if main_win and hasattr(main_win, "control_tab"):
            main_win.control_tab.update_status(data)


# ---------------------------------------------------------------------------
class ControlTab(QWidget):
    """Main control interface for the test bench."""

    def __init__(self, client: RestClient):
        super().__init__()
        self._client = client
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(20)

        # --- Power Control Group ---
        pwr_group = QGroupBox("Bench Power Control")
        pwr_layout = QGridLayout(pwr_group)
        pwr_layout.setContentsMargins(20, 25, 20, 20)
        pwr_layout.setSpacing(15)

        self.btn_relay = QPushButton("ECU Relay: OFF")
        self.btn_relay.setCheckable(True)
        self.btn_relay.setMinimumHeight(50)
        self.btn_relay.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_relay.clicked.connect(self._on_relay_clicked)
        pwr_layout.addWidget(QLabel("Main ECU Power:"), 0, 0)
        pwr_layout.addWidget(self.btn_relay, 0, 1)

        self.btn_ign = QPushButton("IGN HW: OFF")
        self.btn_ign.setCheckable(True)
        self.btn_ign.setMinimumHeight(50)
        self.btn_ign.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_ign.clicked.connect(self._on_ign_clicked)
        pwr_layout.addWidget(QLabel("Ignition Signal:"), 1, 0)
        pwr_layout.addWidget(self.btn_ign, 1, 1)

        layout.addWidget(pwr_group)
        layout.addStretch()

    def _on_relay_clicked(self, checked):
        state_str = "ON" if checked else "OFF"
        self.btn_relay.setText(f"ECU Relay: {state_str}")
        self._client.post_config({"ecu_relay": 1 if checked else 0})

    def _on_ign_clicked(self, checked):
        state_str = "ON" if checked else "OFF"
        self.btn_ign.setText(f"IGN HW: {state_str}")
        self._client.post_config({"ign_hw": 1 if checked else 0})

    def update_status(self, data: dict):
        """Update buttons state based on received status data."""
        if "ecu_relay" in data:
            is_on = bool(data["ecu_relay"])
            self.btn_relay.setChecked(is_on)
            self.btn_relay.setText(f"ECU Relay: {'ON' if is_on else 'OFF'}")
        if "ign_hw" in data:
            is_on = bool(data["ign_hw"])
            self.btn_ign.setChecked(is_on)
            self.btn_ign.setText(f"IGN HW: {'ON' if is_on else 'OFF'}")


# ---------------------------------------------------------------------------
# Main Window
# ---------------------------------------------------------------------------
class MainWindow(QMainWindow):
    """Application main window with tabbed interface."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Test Bench")
        self.setMinimumSize(900, 650)
        self.resize(1000, 700)

        # Central tab widget
        self.tabs = QTabWidget()
        self.setCentralWidget(self.tabs)

        # Shared REST Client
        self.client = RestClient("169.254.169.110")

        # --- Diagnostics tab ---
        self.diag_tab = DiagnosticsTab(self.client)
        self.tabs.addTab(self.diag_tab, "Diagnostics")

        # --- Control tab ---
        self.control_tab = ControlTab(self.client)
        self.tabs.addTab(self.control_tab, "Control")

        # Status bar
        self.statusBar().setStyleSheet("color: #6c7086;")
        self.statusBar().showMessage("Ready")

    def closeEvent(self, event) -> None:
        """Ensure TCP thread stops on window close."""
        if self.diag_tab._client:
            self.diag_tab._client.stop()
        event.accept()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    app.setStyleSheet(STYLE_SHEET)

    # Cross-platform font selection
    os_name = platform.system()
    if os_name == "Darwin":       # macOS
        font = QFont("SF Pro Text", 12)
    elif os_name == "Windows":
        font = QFont("Segoe UI", 10)
    else:                         # Linux
        font = QFont("Noto Sans", 10)
    app.setFont(font)

    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
