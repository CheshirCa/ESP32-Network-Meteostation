#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <vector>

// Глобальные объекты
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5); // SCL=6, SDA=5
WebServer server(80);
Preferences preferences;

// Структура для хранения конфигурации
struct Config {
  char connectionType[10];  // "None", "DHCP", "IPV4"
  char wifiSSID[32];
  char wifiPassword[64];
  char fixedIP[20];        // Формат: XXX.XXX.XXX.XXX/XX
  char deviceName[7];      // Короткое имя устройства (6 символов + null)
  uint16_t port;
  bool configured;
};

Config config;
bool isConfigured = false;
bool inSetupMode = false;
int setupStep = 0;
unsigned long lastSensorRead = 0;
unsigned long lastDisplaySwitch = 0;
int displayPage = 0;  // 0: датчики, 1: WiFi, 2: информация
float temperature = 0;
float humidity = 0;
float pressure = 0;
std::vector<String> wifiNetworks;

// Прототипы функций
void setupDisplay();
void setupSensors();
void loadConfig();
void saveConfig();
void resetConfig();
void setupWiFi();
void startWebServer();
void handleRoot();
void handleAPI();
void consoleShell();
void processCommand(String cmd);
void displaySetupScreen();
void displaySensorData();
void displayNetworkInfo();
void displayDeviceInfo();
void runSetupWizard();
void scanWiFiAndDisplay();
void setupConnectionType();
void setupStaticIP();
void setupPort();
void setupDeviceName();
void saveWizardConfig();
void updateSensorData();
void showLargeTemperature(float temp);
void showHumidityPressure(float hum, float press);

void setup() {
  Serial.begin(115200);
  
  // Инициализация дисплея и датчиков
  setupDisplay();
  setupSensors();
  
  // Загрузка конфигурации
  loadConfig();
  
  if (!config.configured) {
    displaySetupScreen();
    inSetupMode = true;
    setupStep = 0;
    Serial.println("=== Setup Mode ===");
    Serial.println("Enter 'help' for available commands");
    Serial.println("Starting setup wizard...");
    delay(1000);
    runSetupWizard();
  } else {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(0, 10);
    u8g2.print("Starting...");
    u8g2.setCursor(0, 20);
    u8g2.print(config.deviceName);
    u8g2.sendBuffer();
    
    setupWiFi();
    startWebServer();
    isConfigured = true;
    
    Serial.print("\nT> ");
  }
}

void loop() {
  if (inSetupMode) {
    if (setupStep >= 0) {
      // runSetupWizard() вызывается из processCommand()
    }
  } else if (isConfigured) {
    server.handleClient();
    
    // Обновление данных датчиков каждые 2 секунды
    if (millis() - lastSensorRead > 2000) {
      updateSensorData();
      lastSensorRead = millis();
    }
    
    // Переключение экранов на дисплее каждые 5 секунд
    if (millis() - lastDisplaySwitch > 5000) {
      displayPage = (displayPage + 1) % 3;
      switch (displayPage) {
        case 0:
          displaySensorData();
          break;
        case 1:
          displayNetworkInfo();
          break;
        case 2:
          displayDeviceInfo();
          break;
      }
      lastDisplaySwitch = millis();
    }
  }
  
  // Обработка консольных команд
  consoleShell();
}

void setupDisplay() {
  Wire.begin(5, 6); // SDA=5, SCL=6 для ESP32-C3
  Wire.setClock(400000);
  
  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setContrast(255);
}

void setupSensors() {
  if (!aht.begin()) {
    Serial.println("AHT20 error");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(0, 10);
    u8g2.print("AHT20 error");
    u8g2.sendBuffer();
    while (1);
  }

  if (!bmp.begin(0x77)) {
    Serial.println("BMP280 error");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(0, 10);
    u8g2.print("BMP280 error");
    u8g2.sendBuffer();
    while (1);
  }
  
  Serial.println("Sensors initialized");
}

void loadConfig() {
  preferences.begin("iot-config", true);
  
  strlcpy(config.connectionType, preferences.getString("connType", "None").c_str(), sizeof(config.connectionType));
  strlcpy(config.wifiSSID, preferences.getString("ssid", "").c_str(), sizeof(config.wifiSSID));
  strlcpy(config.wifiPassword, preferences.getString("password", "").c_str(), sizeof(config.wifiPassword));
  strlcpy(config.fixedIP, preferences.getString("fixedIP", "192.168.1.100/24").c_str(), sizeof(config.fixedIP));
  strlcpy(config.deviceName, preferences.getString("devName", "ESP32C3").c_str(), sizeof(config.deviceName));
  config.port = preferences.getUShort("port", 80);
  config.configured = preferences.getBool("configured", false);
  
  preferences.end();
  
  Serial.println("Config loaded:");
  Serial.print("Device name: "); Serial.println(config.deviceName);
  Serial.print("Connection type: "); Serial.println(config.connectionType);
  Serial.print("SSID: "); Serial.println(config.wifiSSID);
  Serial.print("Port: "); Serial.println(config.port);
  Serial.print("Configured: "); Serial.println(config.configured);
}

void saveConfig() {
  preferences.begin("iot-config", false);
  
  preferences.putString("connType", config.connectionType);
  preferences.putString("ssid", config.wifiSSID);
  preferences.putString("password", config.wifiPassword);
  preferences.putString("fixedIP", config.fixedIP);
  preferences.putString("devName", config.deviceName);
  preferences.putUShort("port", config.port);
  preferences.putBool("configured", true);
  
  preferences.end();
  
  config.configured = true;
  Serial.println("Config saved");
}

void resetConfig() {
  preferences.begin("iot-config", false);
  preferences.clear();
  preferences.end();
  
  strcpy(config.connectionType, "None");
  strcpy(config.wifiSSID, "");
  strcpy(config.wifiPassword, "");
  strcpy(config.fixedIP, "192.168.1.100/24");
  strcpy(config.deviceName, "ESP32C3");
  config.port = 80;
  config.configured = false;
  
  Serial.println("Config reset");
  displaySetupScreen();
  inSetupMode = true;
  setupStep = 0;
  
  // Перезапускаем мастер настройки
  runSetupWizard();
}

void setupWiFi() {
  if (strcmp(config.wifiSSID, "") == 0) {
    Serial.println("No WiFi configured");
    return;
  }
  
  Serial.print("Connecting to ");
  Serial.println(config.wifiSSID);
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(0, 10);
  u8g2.print("Connecting WiFi:");
  u8g2.setCursor(0, 20);
  u8g2.print(config.wifiSSID);
  u8g2.sendBuffer();
  
  if (strcmp(config.connectionType, "DHCP") == 0) {
    WiFi.begin(config.wifiSSID, config.wifiPassword);
  } else if (strcmp(config.connectionType, "IPV4") == 0) {
    // Парсинг IP в CIDR нотации
    char ip[16];
    int prefix;
    sscanf(config.fixedIP, "%15[^/]/%d", ip, &prefix);
    
    IPAddress localIP, gateway, subnet;
    localIP.fromString(ip);
    gateway.fromString(ip);
    gateway[3] = 1; // Предполагаем шлюз .1
    
    // Расчет маски из префикса
    uint32_t mask = (0xFFFFFFFF << (32 - prefix)) & 0xFFFFFFFF;
    subnet = IPAddress(mask >> 24, (mask >> 16) & 0xFF, (mask >> 8) & 0xFF, mask & 0xFF);
    
    WiFi.config(localIP, gateway, subnet);
    WiFi.begin(config.wifiSSID, config.wifiPassword);
  }
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(0, 10);
    u8g2.print("WiFi connected!");
    u8g2.setCursor(0, 20);
    u8g2.print("IP:");
    
    String ipStr = WiFi.localIP().toString();
    if (ipStr.length() > 14) {
      // Для длинных IP - разбиваем на две строки
      u8g2.setCursor(0, 30);
      u8g2.print(ipStr.substring(0, 14));
      u8g2.setCursor(0, 40);
      u8g2.print(ipStr.substring(14));
    } else {
      u8g2.setCursor(0, 30);
      u8g2.print(ipStr);
    }
    u8g2.sendBuffer();
    delay(2000);
  } else {
    Serial.println("WiFi connection failed");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(0, 10);
    u8g2.print("WiFi failed!");
    u8g2.setCursor(0, 20);
    u8g2.print("Check config");
    u8g2.sendBuffer();
  }
}

void startWebServer() {
  server.on("/", handleRoot);
  server.on("/api/data", handleAPI);
  
  server.begin();
  Serial.print("HTTP server started on port ");
  Serial.println(config.port);
  Serial.print("Web interface: http://");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(WiFi.localIP());
  } else {
    Serial.print("local-ip");
  }
  Serial.print(":");
  Serial.println(config.port);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>" + String(config.deviceName) + " - Sensor Data</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
  html += ".container { background: white; padding: 15px; border-radius: 8px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); max-width: 400px; margin: 0 auto; }";
  html += ".header { border-bottom: 2px solid #007bff; padding-bottom: 8px; margin-bottom: 15px; }";
  html += ".device-name { font-size: 24px; color: #333; font-weight: bold; }";
  html += ".sensor { margin: 12px 0; padding: 8px; background: #e9ecef; border-radius: 5px; display: flex; justify-content: space-between; }";
  html += ".label { font-weight: bold; }";
  html += ".value { font-size: 20px; color: #007bff; font-weight: bold; }";
  html += ".footer { margin-top: 15px; font-size: 12px; color: #666; text-align: center; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<div class='device-name'>" + String(config.deviceName) + "</div>";
  html += "<div>Name: <strong>" + String(config.deviceName) + "</strong> | SSID: " + String(config.wifiSSID) + "</div>";
  html += "</div>";
  html += "<div class='sensor'><span class='label'>Temp:</span><span class='value'>" + String(temperature, 1) + "°C</span></div>";
  html += "<div class='sensor'><span class='label'>Humidity:</span><span class='value'>" + String(humidity, 1) + "%</span></div>";
  html += "<div class='sensor'><span class='label'>Pressure:</span><span class='value'>" + String(pressure, 0) + " hPa</span></div>";
  html += "<div class='footer'>";
  html += "IP: " + WiFi.localIP().toString();
  html += " | Auto-refresh: 5s";
  html += "</div>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleAPI() {
  String json = "{";
  json += "\"name\":\"" + String(config.deviceName) + "\",";
  json += "\"ssid\":\"" + String(config.wifiSSID) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"pressure\":" + String(pressure, 0);
  json += "}";
  
  server.send(200, "application/json", json);
}

void consoleShell() {
  if (Serial.available()) {
    static String input = "";
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (input.length() > 0) {
        if (!inSetupMode) Serial.print("T> ");
        Serial.println(input);
        processCommand(input);
        input = "";
      }
      if (!inSetupMode && !inSetupMode) Serial.print("T> ");
    } else {
      input += c;
    }
  }
}

void processCommand(String cmd) {
  cmd.toLowerCase();
  cmd.trim();
  
  if (cmd == "help") {
    Serial.println("Available commands:");
    Serial.println("info - show sensor and device info");
    Serial.println("net - show network settings");
    Serial.println("reset - reset configuration to defaults");
    Serial.println("set [ip|port|ssid|passw|name] <value> - set parameter");
    Serial.println("save - save current configuration");
    Serial.println("scan - scan for WiFi networks");
    Serial.println("apply - apply settings and restart");
  }
  else if (cmd == "info") {
    Serial.print("Device name: "); Serial.println(config.deviceName);
    Serial.print("Temperature: "); Serial.print(temperature, 1); Serial.println("°C");
    Serial.print("Humidity: "); Serial.print(humidity, 1); Serial.println("%");
    Serial.print("Pressure: "); Serial.print(pressure, 0); Serial.println(" hPa");
  }
  else if (cmd == "net") {
    Serial.print("SSID: "); Serial.println(config.wifiSSID);
    Serial.print("Connection type: "); Serial.println(config.connectionType);
    if (strcmp(config.connectionType, "IPV4") == 0) {
      Serial.print("Static IP: "); Serial.println(config.fixedIP);
    }
    Serial.print("Port: "); Serial.println(config.port);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("IP address: "); Serial.println(WiFi.localIP());
      Serial.print("MAC: "); Serial.println(WiFi.macAddress());
    } else {
      Serial.println("WiFi: Not connected");
    }
  }
  else if (cmd == "reset") {
    Serial.println("Are you sure you want to reset all settings? (yes/no)");
    Serial.setTimeout(10000);
    String answer = Serial.readStringUntil('\n');
    answer.trim();
    answer.toLowerCase();
    
    if (answer == "yes" || answer == "y") {
      resetConfig();
      Serial.println("Settings have been reset.");
    } else {
      Serial.println("Reset cancelled.");
    }
  }
  else if (cmd.startsWith("set ")) {
    cmd = cmd.substring(4);
    int spaceIndex = cmd.indexOf(' ');
    if (spaceIndex > 0) {
      String param = cmd.substring(0, spaceIndex);
      String value = cmd.substring(spaceIndex + 1);
      
      if (param == "ip") {
        value.trim();
        if (value.length() < sizeof(config.fixedIP)) {
          strcpy(config.fixedIP, value.c_str());
          Serial.print("IP set to: "); Serial.println(config.fixedIP);
        }
      }
      else if (param == "port") {
        int port = value.toInt();
        if (port >= 1 && port <= 65535) {
          config.port = port;
          Serial.print("Port set to: "); Serial.println(port);
        } else {
          Serial.println("Invalid port (1-65535)");
        }
      }
      else if (param == "ssid") {
        value.trim();
        if (value.length() < sizeof(config.wifiSSID)) {
          strcpy(config.wifiSSID, value.c_str());
          Serial.print("SSID set to: "); Serial.println(config.wifiSSID);
        }
      }
      else if (param == "passw") {
        value.trim();
        if (value.length() < sizeof(config.wifiPassword)) {
          strcpy(config.wifiPassword, value.c_str());
          Serial.println("Password updated");
        }
      }
      else if (param == "name") {
        value.trim();
        if (value.length() <= 6 && value.length() > 0) {
          strcpy(config.deviceName, value.c_str());
          Serial.print("Device name set to: "); Serial.println(config.deviceName);
        } else {
          Serial.println("Name must be 1-6 characters");
        }
      }
      else {
        Serial.println("Unknown parameter. Use: ip, port, ssid, passw, name");
      }
    } else {
      Serial.println("Usage: set [ip|port|ssid|passw|name] <value>");
    }
  }
  else if (cmd == "save") {
    saveConfig();
    Serial.println("Configuration saved");
  }
  else if (cmd == "scan") {
    Serial.println("Scanning WiFi networks...");
    int n = WiFi.scanNetworks();
    if (n == 0) {
      Serial.println("No networks found");
    } else {
      Serial.print(n);
      Serial.println(" networks found:");
      for (int i = 0; i < n; i++) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.print(" dBm) ");
        Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured");
      }
    }
  }
  else if (cmd == "apply") {
    Serial.println("Applying settings and restarting...");
    saveConfig();
    delay(1000);
    ESP.restart();
  }
  else if (inSetupMode) {
    // Обработка команд во время настройки
    if (setupStep == 1) { // Ввод имени устройства
      if (cmd.length() <= 6 && cmd.length() > 0) {
        strcpy(config.deviceName, cmd.c_str());
        setupStep++;
        Serial.println("Device name saved: " + cmd);
        Serial.println("\nScanning WiFi networks...");
        int n = WiFi.scanNetworks();
        wifiNetworks.clear();
        
        if (n == 0) {
          Serial.println("No networks found. Enter SSID manually:");
          setupStep = 10; // Переход к ручному вводу SSID
        } else {
          Serial.print("Found ");
          Serial.print(n);
          Serial.println(" networks:");
          for (int i = 0; i < n; i++) {
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.println(WiFi.SSID(i));
            wifiNetworks.push_back(WiFi.SSID(i));
          }
          Serial.println("\nEnter network number or 'm' for manual entry:");
        }
      } else {
        Serial.println("Name must be 1-6 characters. Try again:");
      }
    }
    else if (setupStep == 2) { // Выбор WiFi сети из списка
      if (cmd == "m") {
        Serial.println("Enter SSID manually:");
        setupStep = 10;
      } else {
        int ssidNum = cmd.toInt();
        if (ssidNum > 0 && ssidNum <= wifiNetworks.size()) {
          strcpy(config.wifiSSID, wifiNetworks[ssidNum-1].c_str());
          Serial.print("Selected: "); Serial.println(config.wifiSSID);
          Serial.print("Enter password for '"); Serial.print(config.wifiSSID); Serial.println("':");
          setupStep++;
        } else {
          Serial.println("Invalid selection. Enter number or 'm':");
        }
      }
    }
    else if (setupStep == 10) { // Ручной ввод SSID
      cmd.trim();
      if (cmd.length() > 0 && cmd.length() < sizeof(config.wifiSSID)) {
        strcpy(config.wifiSSID, cmd.c_str());
        Serial.print("SSID: "); Serial.println(config.wifiSSID);
        Serial.print("Enter password for '"); Serial.print(config.wifiSSID); Serial.println("':");
        setupStep = 3;
      } else {
        Serial.println("Invalid SSID. Try again:");
      }
    }
    else if (setupStep == 3) { // Ввод пароля
      strcpy(config.wifiPassword, cmd.c_str());
      setupStep++;
      setupConnectionType();
    }
    else if (setupStep == 4) { // Выбор типа подключения
      if (cmd == "1") {
        strcpy(config.connectionType, "DHCP");
        Serial.println("Using DHCP");
        setupStep++;
        setupPort();
      } else if (cmd == "2") {
        strcpy(config.connectionType, "IPV4");
        Serial.println("Using static IP");
        setupStep++;
        setupStaticIP();
      } else {
        Serial.println("Enter 1 for DHCP or 2 for static IP:");
      }
    }
    else if (setupStep == 5 && strcmp(config.connectionType, "IPV4") == 0) { // Ввод статического IP
      // Простая проверка формата
      if (cmd.indexOf('/') > 0 && cmd.length() < sizeof(config.fixedIP)) {
        strcpy(config.fixedIP, cmd.c_str());
        Serial.print("Static IP set: "); Serial.println(config.fixedIP);
        setupStep++;
        setupPort();
      } else {
        Serial.println("Invalid format. Use XXX.XXX.XXX.XXX/XX (e.g., 192.168.1.100/24):");
      }
    }
    else if (setupStep == 6 || (setupStep == 5 && strcmp(config.connectionType, "DHCP") == 0)) { // Ввод порта
      int port = cmd.toInt();
      if (port >= 1 && port <= 65535) {
        config.port = port;
        Serial.print("Port set: "); Serial.println(port);
        setupStep = -1; // Завершение настройки
        saveWizardConfig();
      } else {
        Serial.println("Invalid port (1-65535). Enter port number:");
      }
    }
  }
  else {
    Serial.println("Unknown command. Type 'help' for help.");
  }
}

void updateSensorData() {
  sensors_event_t humidityEvent, tempAHT;
  aht.getEvent(&humidityEvent, &tempAHT);
  float tempBMP = bmp.readTemperature();
  float pressurePa = bmp.readPressure();
  
  // Средняя температура из двух датчиков
  temperature = (tempAHT.temperature + tempBMP) / 2.0;
  humidity = humidityEvent.relative_humidity;
  pressure = pressurePa / 100.0F; // Преобразование в гПа
  
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from AHT sensor!");
    temperature = 0;
    humidity = 0;
  }
  
  if (isnan(pressure)) {
    Serial.println("Failed to read from BMP sensor!");
    pressure = 0;
  }
}

void displaySensorData() {
  u8g2.clearBuffer();
  showLargeTemperature(temperature);
  u8g2.sendBuffer();
}

void displayNetworkInfo() {
  u8g2.clearBuffer();
  showHumidityPressure(humidity, pressure);
  u8g2.sendBuffer();
}

void displayDeviceInfo() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tf);
  
  // Имя устройства (крупнее)
  u8g2.setFont(u8g2_font_7x13_tf);
  u8g2.setCursor(0, 12);
  u8g2.print(config.deviceName);
  
  // IP адрес
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(0, 25);
  u8g2.print("IP:");
  if (WiFi.status() == WL_CONNECTED) {
    String ipStr = WiFi.localIP().toString();
    if (ipStr.length() > 11) {
      // Разбиваем длинный IP
      u8g2.setCursor(15, 25);
      u8g2.print(ipStr.substring(0, 11));
      u8g2.setCursor(0, 35);
      u8g2.print(ipStr.substring(11));
    } else {
      u8g2.setCursor(15, 25);
      u8g2.print(ipStr);
    }
  } else {
    u8g2.setCursor(15, 25);
    u8g2.print("No connection");
  }
  
  // SSID
  u8g2.setCursor(0, 38);
  u8g2.print("SSID:");
  u8g2.setCursor(25, 38);
  if (strlen(config.wifiSSID) > 9) {
    u8g2.print(String(config.wifiSSID).substring(0, 9));
    u8g2.print("..");
  } else {
    u8g2.print(config.wifiSSID);
  }
  
  u8g2.sendBuffer();
}

void displaySetupScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13_tf);
  u8g2.setCursor(10, 20);
  u8g2.print("SETUP");
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(5, 35);
  u8g2.print("Use console");
  u8g2.sendBuffer();
}

void runSetupWizard() {
  if (setupStep == 0) {
    Serial.println("\n=== Setup Wizard ===");
    Serial.println("Enter device name (1-6 characters):");
    setupStep = 1;
  }
}

void setupDeviceName() {
  Serial.println("Enter device name (1-6 characters):");
}

void setupConnectionType() {
  Serial.println("\nSelect connection type:");
  Serial.println("1. DHCP (automatic IP)");
  Serial.println("2. Static IP");
  Serial.print("Enter choice (1/2): ");
}

void setupStaticIP() {
  Serial.println("\nEnter static IP in CIDR notation (e.g., 192.168.1.100/24):");
}

void setupPort() {
  Serial.println("\nEnter HTTP server port (1-65535, default 80):");
}

void saveWizardConfig() {
  saveConfig();
  Serial.println("\n=== Setup Complete ===");
  Serial.println("Configuration saved. Restarting...");
  delay(2000);
  ESP.restart();
}

void showLargeTemperature(float temp) {
  char tempStr[8];
  
  // Форматируем в зависимости от величины
  if (temp >= 100.0 || temp <= -10.0) {
    dtostrf(temp, 5, 1, tempStr); // "XX.X" или "-X.X"
  } else if (temp >= 10.0 || temp <= -1.0) {
    dtostrf(temp, 4, 1, tempStr); // "X.X" или "-.X"
  } else {
    dtostrf(temp, 3, 1, tempStr); // ".X"
  }
  
  // Крупный шрифт для цифр
  u8g2.setFont(u8g2_font_logisoso24_tf);
  
  // Центрируем температуру
  int textWidth = u8g2.getStrWidth(tempStr);
  int xPos = (72 - textWidth - 8) / 2; // -8 для "°C"
  
  // Температура
  u8g2.setCursor(xPos, 32);
  u8g2.print(tempStr);
  
  // Знак градуса и C (маленьким шрифтом)
// u8g2.setFont(u8g2_font_4x6_tf);
//  u8g2.setCursor(xPos + textWidth, 22);
// u8g2.print("o");
//  u8g2.setCursor(xPos + textWidth + 6, 22);
//  u8g2.print("C");
}

void showHumidityPressure(float hum, float press) {
  // Влажность
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(2, 12);
  u8g2.print("H:");
  
  // Цифры влажности покрупнее
  u8g2.setFont(u8g2_font_7x13_tf);
  char humStr[5];
  dtostrf(hum, 3, 0, humStr);
  u8g2.setCursor(15, 12);
  u8g2.print(humStr);
  u8g2.print("%");
  
  // Давление
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(2, 30);
  u8g2.print("P:");
  
  u8g2.setFont(u8g2_font_7x13_tf);
  char pressStr[7];
  dtostrf(press, 4, 0, pressStr);
  u8g2.setCursor(15, 30);
  u8g2.print(pressStr);
  u8g2.print("hPa");
}