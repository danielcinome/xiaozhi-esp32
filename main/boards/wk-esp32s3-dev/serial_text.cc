// =====================================================================
//  COOPER – canal de texto por puerto serie (herramienta de desarrollo)
//
//  Lee líneas escritas en el monitor (idf.py monitor) y las envía al
//  servidor como si fueran una frase detectada por voz. La respuesta
//  llega por el parlante, la pantalla y el log (<< ...).
//
//  Instalación (ver README_serial_text.md):
//   1. Copia este archivo en la carpeta de la placa (main/boards/wk-esp32s3-dev/).
//   2. En el .cc de la placa: declara `void StartSerialText();` arriba y
//      llama `StartSerialText();` en el constructor, tras InitializeButtons().
//   3. Añade Application::SendTextToServer en application.h / .cc.
//   4. idf.py reconfigure  (la carpeta de la placa solo se escanea al configurar)
//
//  Uso: en el monitor escribe la frase y pulsa Enter.
//        Comandos extra:  /stop  → cierra la conversación
// =====================================================================

#include "application.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/uart.h>
#include <string>

#define TAG "SerialText"
#define SERIAL_TEXT_UART   UART_NUM_0
#define SERIAL_TEXT_BUF    256

static void SerialTextTask(void*) {
    // Espera a que la aplicación termine de arrancar
    vTaskDelay(pdMS_TO_TICKS(5000));

    if (!uart_is_driver_installed(SERIAL_TEXT_UART)) {
        ESP_ERROR_CHECK(uart_driver_install(SERIAL_TEXT_UART, SERIAL_TEXT_BUF * 2, 0, 0, nullptr, 0));
    }
    ESP_LOGI(TAG, "Canal de texto listo. Escribe una frase y pulsa Enter.");

    std::string line;
    uint8_t ch;
    while (true) {
        int n = uart_read_bytes(SERIAL_TEXT_UART, &ch, 1, pdMS_TO_TICKS(100));
        if (n <= 0) continue;

        if (ch == '\r' || ch == '\n') {
            if (line.empty()) continue;
            auto& app = Application::GetInstance();
            if (line == "/stop") {
                ESP_LOGI(TAG, "Cerrando conversación");
                app.StopListening();
            } else {
                ESP_LOGI(TAG, ">> %s", line.c_str());
                app.SendTextToServer(line);
            }
            line.clear();
        } else if (ch == 0x08 || ch == 0x7f) {        // retroceso
            if (!line.empty()) line.pop_back();
        } else if (ch >= 0x20 || ch >= 0x80) {        // imprimible o UTF-8
            if (line.size() < SERIAL_TEXT_BUF) line.push_back(static_cast<char>(ch));
        }
    }
}

// Llamar desde el constructor de la placa (ver README_serial_text.md)
void StartSerialText() {
    static bool started = false;
    if (started) return;
    started = true;
    xTaskCreate(SerialTextTask, "serial_text", 4096, nullptr, 2, nullptr);
}