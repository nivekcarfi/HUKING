#include "esp_camera.h"
#include "Arduino.h"
#include "SD_MMC.h"

// Definiciones para el sensor de proximidad (prioridad máxima)
#define trigPin 13
#define echoPin 14
#define MAX_DISTANCE 200  // Distancia máxima reducida para mayor reactividad
#define ACTIVATION_DISTANCE 30  // Distancia de activación en cm
float timeOut = MAX_DISTANCE * 60;
int soundVelocity = 340;

// Configuración de la cámara (se inicia solo cuando es necesario)
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

// Variables de estado
bool cameraInitialized = false;
bool sdInitialized = false;

void setup() {
  Serial.begin(115200);
  
  // Inicialización prioritaria del sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  Serial.println("Sensor de proximidad listo (prioridad máxima)");
}

float getSonar() {
  unsigned long pingTime;
  float distance;
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  pingTime = pulseIn(echoPin, HIGH, timeOut);
  distance = (float)pingTime * soundVelocity / 2 / 10000;
  return distance;
}

void initializeCameraAndSD() {
  if (!cameraInitialized) {
    Serial.println("Inicializando cámara...");
    
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
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.println("Error al inicializar la cámara");
      return;
    }
    cameraInitialized = true;
    Serial.println("Cámara inicializada");
  }

  if (!sdInitialized) {
    Serial.println("Inicializando tarjeta SD...");
    SD_MMC.setPins(14, 15, 2);
    if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
      Serial.println("Error al montar la tarjeta SD");
      return;
    }
    sdInitialized = true;
    Serial.println("Tarjeta SD montada");
  }
}

void takePhoto() {
  if (!cameraInitialized || !sdInitialized) {
    Serial.println("Error: Cámara o SD no inicializadas");
    return;
  }

  Serial.println("¡Objeto detectado! Capturando foto...");
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error al capturar la foto");
    return;
  }

  String fileName = "/detection_" + String(millis()) + ".jpg";
  File file = SD_MMC.open(fileName.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Error al abrir archivo en SD");
    esp_camera_fb_return(fb);
    return;
  }

  file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);
  Serial.printf("Foto guardada: %s (%d bytes)\n", fileName.c_str(), fb->len);
}

void loop() {
  // Lectura continua del sensor (máxima prioridad)
  float distance = getSonar();
  Serial.printf("Distancia: %.2f cm\n", distance);

  if (distance > 0 && distance < ACTIVATION_DISTANCE) {
    // Cuando detecta algo, inicializa cámara y SD (si no lo estaban)
    initializeCameraAndSD();
    
    // Toma la foto inmediatamente
    takePhoto();
    
    // Pequeña pausa para evitar múltiples detecciones
    delay(300);
  }
  
  // Pausa mínima entre lecturas del sensor
  delay(50);
}