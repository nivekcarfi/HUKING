#include "esp_camera.h"
#include "Arduino.h"
#include "SD_MMC.h"

// Definir los pines de la cámara para el ESP32-Wrover con OV2640
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      21
#define SIOD_GPIO_NUM      26
#define SIOC_GPIO_NUM      27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       19
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM        5
#define Y2_GPIO_NUM        4
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void setup() {
 Serial.begin(115200);
 Serial.println("Inicializando cámara...");

 // Inicialización de la tarjeta SD
 SD_MMC.setPins(14, 15, 2); // Ajusta los pines según tu configuración
 if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
   Serial.println("Error al montar la tarjeta SD");
   return;
 }
 Serial.println("Tarjeta SD montada correctamente");

 // Configuración de la cámara
 camera_config_t config;
 config.ledc_channel = LEDC_CHANNEL_0;
 config.ledc_timer = LEDC_TIMER_0;
 config.pin_d0 = Y2_GPIO_NUM;
 config.pin_d1 = Y3_GPIO_NUM;
 config.pin_d2 = Y4_GPIO_NUM;
 config.pin_d3 = Y5_GPIO_NUM;
 config.pin_d4 = Y6_GPIO_NUM;
 config.pin_d5 = Y7_GPIO_NUM;
 config.pin_d6 = Y8_GPIO_NUM;
 config.pin_d7 = Y9_GPIO_NUM;
 config.pin_xclk = XCLK_GPIO_NUM;
 config.pin_pclk = PCLK_GPIO_NUM;
 config.pin_vsync = VSYNC_GPIO_NUM;
 config.pin_href = HREF_GPIO_NUM;
 config.pin_sscb_sda = SIOD_GPIO_NUM;
 config.pin_sscb_scl = SIOC_GPIO_NUM;
 config.pin_pwdn = PWDN_GPIO_NUM;
 config.pin_reset = RESET_GPIO_NUM;
 config.xclk_freq_hz = 20000000;  // 20MHz para la cámara
 config.pixel_format = PIXFORMAT_JPEG;
 config.frame_size = FRAMESIZE_SVGA;  // Resolución SVGA (800x600)
 config.jpeg_quality = 12;  // Ajustar calidad (1 = mejor, 63 = peor)
 config.fb_count = 1;  // Número de framebuffers

 // Inicializar la cámara
 esp_err_t err = esp_camera_init(&config);
 if (err != ESP_OK) {
   Serial.println("Error al inicializar la cámara");
   return;
 }

 Serial.println("Cámara inicializada correctamente");
}

void loop() {
 Serial.println("Capturando foto...");
  // Capturar la imagen
 camera_fb_t *fb = esp_camera_fb_get();
 if (!fb) {
   Serial.println("Error al capturar la foto");
   return;
 }

 Serial.printf("Foto capturada con tamaño %d bytes\n", fb->len);

 // Crear un nombre único para el archivo basado en el tiempo
 String fileName = "/photo_" + String(millis()) + ".jpg";

 // Abrir el archivo en la tarjeta SD
 File file = SD_MMC.open(fileName.c_str(), FILE_WRITE);
 if (!file) {
   Serial.println("Error al abrir el archivo en la tarjeta SD");
   esp_camera_fb_return(fb);
   return;
 }

 // Escribir la imagen en el archivo
 file.write(fb->buf, fb->len);
 file.close();  // Cerrar el archivo

 Serial.println("Foto guardada en la tarjeta SD");

 // Liberar la memoria de la imagen
 esp_camera_fb_return(fb);

 delay(5000);  // Esperar 5 segundos antes de tomar otra foto
}