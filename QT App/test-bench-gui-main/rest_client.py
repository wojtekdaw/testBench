import requests
import time
from PyQt6.QtCore import QObject, pyqtSignal, QRunnable, QThreadPool

class RestWorker(QRunnable):
    def __init__(self, method, url, json_data=None):
        super().__init__()
        self.method = method
        self.url = url
        self.json_data = json_data
        self.signals = WorkerSignals()

    def run(self):
        start_time = time.perf_counter()
        try:
            if self.method.upper() == "GET":
                response = requests.get(self.url, timeout=2.0)
            else:
                response = requests.post(self.url, json=self.json_data, timeout=2.0)
            
            latency = (time.perf_counter() - start_time) * 1000.0
            response.raise_for_status()
            
            self.signals.result.emit(self.url, response.json(), latency)
        except requests.exceptions.RequestException as e:
            self.signals.error.emit(self.url, str(e))

class WorkerSignals(QObject):
    result = pyqtSignal(str, dict, float)
    error = pyqtSignal(str, str)

class RestClient(QObject):
    DEFAULT_IP = "169.254.169.110"
    DEFAULT_PORT = 80

    response_received = pyqtSignal(str, dict)
    error_occurred = pyqtSignal(str)
    latency_measured = pyqtSignal(float)
    connected = pyqtSignal(str)
    disconnected = pyqtSignal(str)

    def __init__(self, ip: str = DEFAULT_IP, parent=None):
        super().__init__(parent)
        self.configure(ip, self.DEFAULT_PORT)
        self.is_connected = False
        self.thread_pool = QThreadPool()

    def configure(self, ip: str, port: int):
        self.base_url = f"http://{ip}:{port}/api"

    def start(self):
        # Test connection with a ping
        self._send_request("GET", "/ping", is_connect=True)

    def disconnect_from_host(self):
        self.is_connected = False
        self.disconnected.emit("Rozłączono przez użytkownika")

    def _send_request(self, method: str, endpoint: str, json_data: dict = None, is_connect: bool = False):
        url = self.base_url + endpoint
        worker = RestWorker(method, url, json_data)
        
        def handle_result(req_url, response_data, latency):
            if is_connect and not self.is_connected:
                self.is_connected = True
                self.connected.emit(self.base_url)
            self.latency_measured.emit(latency)
            self.response_received.emit(endpoint, response_data)

        def handle_error(req_url, error_msg):
            if is_connect and not self.is_connected:
                self.disconnected.emit("Brak odpowiedzi / Timeout")
            self.error_occurred.emit(error_msg)

        worker.signals.result.connect(handle_result)
        worker.signals.error.connect(handle_error)
        self.thread_pool.start(worker)

    def get_ping(self):
        self._send_request("GET", "/ping")

    def get_status(self):
        self._send_request("GET", "/status")

    def post_echo(self, text: str):
        self._send_request("POST", "/echo", {"data": text})

    def post_led(self, action: str):
        self._send_request("POST", "/led", {"action": action})

    def post_config(self, config_data: dict):
        """Send configuration update (e.g. ECU relay, IGN) to STM32."""
        self._send_request("POST", "/config", config_data)

    def post_can(self, can_id: int, data: list):
        self._send_request("POST", "/can", {"id": can_id, "data": data})

    def stop(self):
        self.disconnect_from_host()
        self.thread_pool.waitForDone(1000)
