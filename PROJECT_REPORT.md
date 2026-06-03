# Raport projektowy — Test Bench STM32 / GUI
**Data aktualizacji:** 2026-04-20
**Wersja raportu:** 3.0 (Migracja na REST API / HTTP / JSON)

---

## ZASADY EDYCJI KODU STM32 (CRITICAL)

Wszelkie modyfikacje w plikach źródłowych C/C++ dla STM32 (`.c`, `.h`) **MUSZĄ** znajdować się wyłącznie pomiędzy znacznikami:
```
/* USER CODE BEGIN [Nazwa] */
/* USER CODE END [Nazwa] */
```
Nigdy nie modyfikuj, nie usuwaj ani nie dodawaj kodu poza tymi blokami — zostanie on bezpowrotnie usunięty przy kolejnym generowaniu projektu przez STM32CubeMX.

**Wyjątek:** Pliki autorskie (`http_server.c`, `http_server.h`, `cJSON.c`, `cJSON.h`) nie podlegają tym ograniczeniom.

---

## 1. Przegląd systemu (Architektura REST API)

```
┌─────────────────────────────┐
│  PC – Python GUI            │  PyQt6, biblioteka 'requests' (Klient HTTP)
│  test-bench-gui-main/       │  Tryb: Asynchroniczny (RestWorker)
└────────────┬────────────────┘
             │ HTTP/1.1 (port 80)
             │ Protokół: REST API (Payload: JSON)
┌────────────▼────────────────┐
│  STM32 Nucleo-F767ZI        │  Gateway / kontroler
│  LAN8742A PHY (RMII, 100M) │  IP: 169.254.169.110
│  LwIP 2.1.2 + FreeRTOS     │  API: Netconn (Sequential API)
│  HTTP Server (Threaded)    │  JSON: cJSON (mapped to FreeRTOS heap)
└────────────┬────────────────┘
             │ CAN (Faza 2)
    ┌────────┴────────┐
    │ Teensy 4.1      │  BenchTester2_0_5-13-24.ino
    │ (nie modyfikować│  kod Teensy jest frozen
    └─────────────────┘
```

---

## 2. Sprzęt — konfiguracja

| Parametr | Wartość |
|----------|---------|
| MCU | STM32F767ZI (Cortex-M7, 216 MHz) |
| PHY | LAN8742A (RMII, 100 Mbit/s) |
| IP STM32 | **169.254.169.110** (statyczne) |
| UART debug | USART3, 115200 baud (ST-Link VCOM) |
| Timebase HAL | TIM6 (SysTick zarezerwowany dla FreeRTOS) |

### 2.1 Piny LED (Nucleo-F767ZI)
- **LD1 (zielona) [PB0]**: Sygnalizuje stan serwera (ON = gotowy).
- **LD2 (niebieska) [PB7]**: Heartbeat FreeRTOS (toggle co 500ms).
- **LD3 (czerwona) [PB14]**: Błąd inicjalizacji lub błąd krytyczny.

---

## 3. Oprogramowanie firmware (Stos sieciowy)

### 3.1 Architektura wątków FreeRTOS

1.  **tcpip_thread** (Priorytet: 40 - High): Rdzeń LwIP.
2.  **ethRxTask** (Priorytet: 40 - High): Odbiór ramek z DMA. Budzony przez semafor z przerwania `HAL_ETH_RxCpltCallback`.
3.  **httpServerTask** (Priorytet: 24 - Normal): Obsługa połączeń HTTP (Netconn API).
4.  **defaultTask** (Priorytet: 24 - Normal): Monitoring linku PHY, mruganie LD2.

### 3.2 Kluczowe mechanizmy

-   **Sequential API (Netconn)**: Wykorzystywany w `http_server.c`. Pozwala na blokujący odczyt/zapis, co eliminuje skomplikowane callbacki LwIP RAW API.
-   **cJSON Integration**: Biblioteka JSON skonfigurowana do używania sterty FreeRTOS (`pvPortMalloc`) poprzez `cJSON_InitHooks`.
-   **Robust HTTP Reading**: Serwer czyta dane w pętli, dopóki nie otrzyma całej treści (na podstawie nagłówka `Content-Length`), co zapobiega błędom `ConnectionResetError`.

---

## 4. Protokół REST API (JSON)

| Metoda | Endpoint | Opis | Payload (JSON) |
|--------|----------|------|----------------|
| GET | `/api/ping` | Sprawdzenie łączności | `{}` |
| GET | `/api/status`| Status (ETH, LED, CAN) | `{}` |
| POST | `/api/led` | Sterowanie LED | `{"action": "on/off/toggle"}` |
| POST | `/api/echo` | Test pętli zwrotnej | `{"data": "string"}` |
| POST | `/api/can` | Wysłanie ramki CAN | `{"id": 123, "data": [1,2,3]}` |

---

## 5. Historia rozwiązanych problemów (Sesja 2026-04-20)

### 5.1 Brak odczytu pakietów (Rx Thread)
**Problem**: Po przejściu na FreeRTOS brakowało wątku odczytującego dane z buforów DMA Ethernet.
**Rozwiązanie**: Dodano wątek `ethRxTask` i semafor `RxPktSemaphore` wyzwalany z przerwania `HAL_ETH_RxCpltCallback`.

### 5.2 Konflikt `errno`
**Problem**: Redefinicja `errno` w LwIP kolidowała z biblioteką `newlib-nano`.
**Rozwiązanie**: Usunięto lokalną definicję `errno` w `sys_arch.c`, włączono `LWIP_PROVIDE_ERRNO 1` w `lwipopts.h` i usunięto redefinicję w `cc.h`.

### 5.3 Zrywanie połączeń POST
**Problem**: Klient (Python) otrzymywał `ConnectionResetError` przy wysyłaniu JSONa.
**Rozwiązanie**: Zaimplementowano pętlę odczytu body w `http_server_serve`, która czeka na kompletną treść przed przetworzeniem i zamknięciem gniazda.

---

## 6. GUI (Python / PyQt6)

-   **RestClient**: Klasa bazowa używająca wątków (`RestWorker`) do nieblokującej komunikacji HTTP.
-   **DiagnosticsTab**: Zakładka diagnostyczna z obsługą wszystkich endpointów REST.
-   **StatusIndicator**: Dynamiczna aktualizacja kontrolek na podstawie JSONa z `/api/status`.

---

## 7. Kolejne kroki (Faza 2)

-   [ ] Implementacja sterownika CAN w firmware (CAN1).
-   [ ] Oprogramowanie endpointu `/api/can`.
-   [ ] Integracja asynchronicznych powiadomień o odebranych ramkach CAN.
-   [ ] Dodanie zakładki "Control" w GUI.
