#include "esp_camera.h"
#include "Arduino.h"
#include <WiFi.h>
#include <time.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <LiquidCrystal_I2C.h>

// ----------------- CONFIGURACIÓN COMÚN -----------------
const char* ssid = "Xiaomi 13T";
const char* password = "12345678";

// ----------------- CONFIGURACIÓN DE PINES -----------------
#define trigPin 13
#define echoPin 14
#define LED_VERDE 32
#define LED_ROJO 33
#define servoPin 15
#define SDA_PIN 13
#define SCL_PIN 14

// ----------------- CONSTANTES -----------------
#define ACTIVATION_DISTANCE 30
float timeOut = 200 * 60;
int soundVelocity = 340;
const char* host = "192.168.28.228";
const uint16_t port = 5000;
int posicionAbierta = 158;
int posicionCerrada = 67;
unsigned long lastCapture = 0;
const unsigned long captureCooldown = 5000;

// Configuración de pines de la cámara
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

// ----------------- OBJETOS GLOBALES -----------------
Servo myservo;
WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD 16x2

// ----------------- FUNCIONES COMUNES -----------------
void setupWiFi() {
  Serial.print("Conectando a WiFi...");
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi");
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado! IP: " + WiFi.localIP().toString());
    lcd.setCursor(0, 0);
    lcd.print("IP: ");
    lcd.print(WiFi.localIP().toString());
  } else {
    Serial.println("\nError de conexión WiFi");
    lcd.setCursor(0, 0);
    lcd.print("Error WiFi    ");
  }
}

void setupNTP() {
  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Obteniendo hora NTP...");
    delay(1000);
  }
}

// ----------------- FUNCIONES DE SENSOR ULTRASÓNICO -----------------
float getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  unsigned long pingTime = pulseIn(echoPin, HIGH, timeOut);
  return (float)pingTime * soundVelocity / 2 / 10000;
}

// ----------------- FUNCIONES DE CÁMARA -----------------
void setupCamera() {
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
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error cámara: 0x%x", err);
    lcd.setCursor(0, 1);
    lcd.print("Error Camara   ");
  } else {
    Serial.println("Cámara lista");
  }
}

void captureAndSendPhoto() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error captura");
    lcd.setCursor(0, 1);
    lcd.print("Error captura   ");
    digitalWrite(LED_ROJO, HIGH);
    delay(1000);
    digitalWrite(LED_ROJO, LOW);
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error hora");
    lcd.setCursor(0, 1);
    lcd.print("Error hora     ");
    esp_camera_fb_return(fb);
    digitalWrite(LED_ROJO, HIGH);
    delay(1000);
    digitalWrite(LED_ROJO, LOW);
    return;
  }

  char filename[64];
  strftime(filename, sizeof(filename), "photo_%Y-%m-%d_%H-%M-%S.jpg", &timeinfo);

  WiFiClient client;
  if (client.connect(host, port)) {
    String boundary = "----Boundary";
    String header = "--" + boundary + "\r\n" +
                   "Content-Disposition: form-data; name=\"file\"; filename=\"" + 
                   String(filename) + "\"\r\n" +
                   "Content-Type: image/jpeg\r\n\r\n";
    String footer = "\r\n--" + boundary + "--\r\n";

    client.println("POST /upload HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println("Content-Length: " + String(header.length() + fb->len + footer.length()));
    client.println("Connection: close");
    client.println();
    client.print(header);
    client.write(fb->buf, fb->len);
    client.print(footer);

    delay(10);
    
    // Leer respuesta del servidor
    String serverResponse = "";
    while (client.available()) {
      serverResponse += client.readStringUntil('\r');
    }
    
    // Analizar respuesta
    if (serverResponse.indexOf("🔓 Reserva activa") != -1) {
      // Matrícula reconocida y con reserva
      digitalWrite(LED_VERDE, HIGH);
      abrirBarrera();
      lcd.setCursor(0, 1);
      
      // Extraer matrícula para mostrar en LCD
      int matriculaStart = serverResponse.indexOf("para ") + 5;
      int matriculaEnd = serverResponse.indexOf(".", matriculaStart);
      String matricula = serverResponse.substring(matriculaStart, matriculaEnd);
      
      lcd.print("OK: " + matricula);
      delay(1000);
      digitalWrite(LED_VERDE, LOW);
    } 
    else if (serverResponse.indexOf("⚠️ No se pudo reconocer") != -1) {
      // Matrícula no reconocida
      digitalWrite(LED_ROJO, HIGH);
      lcd.setCursor(0, 1);
      lcd.print("Mat. no recon. ");
      delay(1000);
      digitalWrite(LED_ROJO, LOW);
    }
    else {
      // Otro tipo de error
      digitalWrite(LED_ROJO, HIGH);
      lcd.setCursor(0, 1);
      lcd.print("Error servidor ");
      delay(1000);
      digitalWrite(LED_ROJO, LOW);
    }
    
    Serial.println("Respuesta servidor: " + serverResponse);
  } else {
    Serial.println("Error conexión servidor");
    lcd.setCursor(0, 1);
    lcd.print("Error servidor ");
    digitalWrite(LED_ROJO, HIGH);
    delay(1000);
    digitalWrite(LED_ROJO, LOW);
  }

  client.stop();
  esp_camera_fb_return(fb);
}

// ----------------- FUNCIONES DE BARRERA -----------------
void abrirBarrera() {
  Serial.println("Abriendo barrera");
  lcd.setCursor(0, 1);
  lcd.print("Abriendo...    ");
  myservo.write(posicionAbierta);
  digitalWrite(LED_VERDE, HIGH);  // LED verde encendido mientras está abierta
  delay(5000);
  cerrarBarrera();
  server.send(200, "text/plain", "Barrera operada");
}

void cerrarBarrera() {
  Serial.println("Cerrando barrera");
  lcd.setCursor(0, 1);
  lcd.print("Cerrando...    ");
  myservo.write(posicionCerrada);
  digitalWrite(LED_VERDE, LOW);  // Apagar LED verde al cerrar
}

void handleAbrir() {
  abrirBarrera();
}

// ----------------- FUNCIONES LCD -----------------
bool i2CAddrTest(uint8_t addr) {
  Wire.begin();
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {
    return true;
  }
  return false;
}

// ----------------- SETUP Y LOOP -----------------
void setup() {
  Serial.begin(115200);
  
  // Inicializar LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistema Barrera");
  
  // Inicializar hardware
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  myservo.attach(servoPin, 500, 2500);
  cerrarBarrera();

  // Configurar conexiones
  setupWiFi();
  setupNTP();
  setupCamera();

  // Configurar servidor web
  server.on("/abrir", handleAbrir);
  server.begin();
  Serial.println("Servidor HTTP iniciado");
  lcd.setCursor(0, 1);
  lcd.print("Servidor listo ");
}

void loop() {
  // Detección de movimiento
  float distancia = getDistance();
  if (distancia > 0 && distancia < ACTIVATION_DISTANCE && 
      millis() - lastCapture > captureCooldown) {
    lastCapture = millis();
    lcd.setCursor(0, 1);
    lcd.print("Detectado...   ");
    captureAndSendPhoto();
  }

  // Manejar peticiones HTTP
  server.handleClient();
  
  delay(100);
}