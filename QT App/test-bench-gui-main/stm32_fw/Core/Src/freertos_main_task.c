/**
 * @file    freertos_main_task.c
 * @brief   Główny wątek FreeRTOS – inicjalizacja LwIP i serwera TCP.
 *
 * INSTRUKCJA INTEGRACJI z main.c wygenerowanym przez CubeMX:
 *
 * 1. W STM32CubeMX → FreeRTOS → Tasks: zmień DefaultTask na "StartNetTask"
 *    (lub zachowaj nazwę defaultTask i wklej ciało poniżej).
 *
 * 2. W freertos.c (generowany przez CubeMX) znajdź:
 *      void StartDefaultTask(void *argument) { ... }
 *    i zastąp jej ciało zawartością funkcji StartNetTask() poniżej.
 *
 * 3. W main.c NIE wywołuj tcp_server_init() w "USER CODE BEGIN 2" –
 *    LwIP wymaga działającego schedulera FreeRTOS.
 *    MX_LWIP_Init() wygenerowane przez CubeMX MUSI być wywołane z wątku,
 *    a nie przed osKernelStart()!
 *
 * 4. Upewnij się, że w main.c sekcja "USER CODE BEGIN 2" jest PUSTA
 *    (lub zawiera tylko kod niezwiązany z siecią).
 *
 * 5. Dodaj include w freertos.c:
 *      #include "lwip.h"
 *      #include "tcp_server.h"
 */

/*
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║  WKLEJ TO DO freertos.c  (sekcja USER CODE w StartDefaultTask) ║
 * ╚══════════════════════════════════════════════════════════════════╝

void StartDefaultTask(void *argument)
{
    // Etap I: sprawdź czy RTOS żyje – miganie LD2 (niebieska) co 100ms
    // przez 3 sekundy, zanim uruchomimy sieć
    for (int i = 0; i < 30; i++)
    {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        osDelay(100);
    }
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

    // Etap II: Inicjalizacja stosu LwIP (musi być z wątku FreeRTOS)
    MX_LWIP_Init();

    // Uruchom serwer TCP
    err_t err = tcp_server_init();
    if (err == ERR_OK)
    {
        // Sukces – zapal LD1 (zielona = serwer gotowy)
        HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_SET);
    }
    else
    {
        // Błąd – migaj LD3 (czerwona)
        for (;;)
        {
            HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
            osDelay(200);
        }
    }

    // Pętla główna wątku – LwIP obsługiwany jest przez ethernetif_input_task
    // generowany przez CubeMX. Tutaj możemy wykonywać inne zadania.
    for (;;)
    {
        osDelay(1000);
        // np. HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);  // heartbeat
    }
}

 * ═══════════════════════════════════════════════════════════════════
 */

/*
 * WAŻNE – Kolejność inicjalizacji w main.c:
 *
 *   // USER CODE BEGIN 2  ← NIE inicjalizuj LwIP tutaj!
 *   // Wszystko sieciowe idzie do StartDefaultTask powyżej.
 *   // USER CODE END 2
 *
 *   MX_FREERTOS_Init();  // tworzy wątki
 *   osKernelStart();     // uruchamia scheduler – dopiero teraz LwIP może żyć
 *
 * Dlaczego?
 *   MX_LWIP_Init() tworzy semafory i kolejki FreeRTOS wewnętrznie.
 *   Wywołanie jej przed osKernelStart() spowoduje HardFault lub zawieszenie.
 */

/*
 * WERYFIKACJA KONFIGURACJI CubeMX dla Nucleo-F767ZI:
 *
 * [ ] ETH → Mode: RMII (LAN8742A)
 * [ ] LwIP → Enable = true
 * [ ] LwIP → LWIP_DHCP = false
 * [ ] LwIP → IP_ADDRESS = 192.168.1.100
 * [ ] LwIP → NETMASK = 255.255.255.0
 * [ ] LwIP → GW = 192.168.1.1
 * [ ] LwIP → MEM_SIZE >= 10240
 * [ ] LwIP → TCP_SND_BUF >= 4096
 * [ ] LwIP → TCP_WND >= 4096
 * [ ] FreeRTOS → API: CMSIS_V2
 * [ ] FreeRTOS → configTOTAL_HEAP_SIZE >= 32768 (32 KB)
 * [ ] SYS → Timebase: TIM6 (nie SysTick – zajęty przez HAL+FreeRTOS)
 * [ ] GPIO: PB0=LD1, PB7=LD2, PB14=LD3 → Output, Push-Pull
 */
