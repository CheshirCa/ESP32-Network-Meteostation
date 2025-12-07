# Инструкция по настройке метеостанции на ESP32-C3

## 1. Соединение компонентов

![Wire connections](https://github.com/CheshirCa/ESP32-Network-Meteostation/blob/main/connetions.jpg)

### Необходимые компоненты:
- Плата ESP32-C3 с дисплеем 0.46 OLED "The EGG"
- Модуль AHT20 (температура и влажность)
- Модуль BMP280 (температура и давление)
- Провода для соединения

### Схема подключения (I2C все на одни пины):
```
ESP32-C3         OLED/AHT20/BMP280
GPIO 5 (SDA)  -> SDA (все устройства)
GPIO 6 (SCL)  -> SCL (все устройства)
3.3V          -> VCC (все устройства)
GND           -> GND (все устройства)
```

**Важно:**
- AHT20 обычно имеет адрес 0x38
- BMP280 обычно имеет адрес 0x77 (проверьте перемычку на модуле)
- OLED обычно имеет адрес 0x3C

Если адреса конфликтуют, BMP280 часто имеет перемычку для изменения адреса с 0x77 на 0x76.

## 2. Подготовка Arduino IDE и компиляция

### Установка Arduino IDE:
1. Скачайте Arduino IDE с официального сайта arduino.cc
2. Установите программу на компьютер

### Настройка поддержки ESP32-C3:
1. В Arduino IDE откройте Файл -> Настройки
2. В поле "Дополнительные ссылки для менеджера плат" добавьте:
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
3. Нажмите OK

4. Откройте Инструменты -> Плата -> Менеджер плат
5. В поиске введите "esp32"
6. Найдите "ESP32 by Espressif Systems" и установите последнюю версию
7. Закройте менеджер плат

### Установка библиотек:
1. В Arduino IDE откройте Скетч -> Подключить библиотеку -> Управлять библиотеками
2. По очереди установите следующие библиотеки:
   - "U8g2" от olikraus (для OLED дисплея)
   - "Adafruit AHTX0" (для датчика AHT20)
   - "Adafruit BMP280 Library" (для датчика BMP280)
   - "Adafruit Unified Sensor" (может установиться автоматически)

### Настройка проекта:
1. Скопируйте полный код скетча в Arduino IDE
2. Выберите плату: Инструменты -> Плата -> ESP32 Arduino -> "ESP32-C3 Dev Module"
3. Установите настройки:
   - CPU Frequency: 160MHz (WiFi)
   - Flash Size: "4MB (32Mb)"
   - Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
   - Upload Speed: "921600"
   - Port: выберите COM-порт вашей ESP32-C3

### Компиляция и загрузка:
1. Нажмите кнопку "Проверить" (галочка) для компиляции
2. Если нет ошибок, нажмите кнопку "Загрузить" (стрелка вправо)
3. Удерживайте кнопку BOOT на плате ESP32-C3 при нажатии кнопки загрузки, если требуется
4. Дождитесь завершения загрузки

## 3. Использование

### Первый запуск (настройка):
1. Откройте монитор порта: Инструменты -> Монитор порта
2. Установите скорость 115200 бод
3. Нажмите кнопку RST (Reset) на плате
4. На дисплее появится надпись "SETUP"
5. В мониторе порта появится мастер настройки

### Пошаговая настройка:
1. Введите имя устройства (1-6 символов), например: "METEO1"
2. Дождитесь сканирования WiFi сетей
3. Выберите сеть из списка (введите номер) или 'm' для ручного ввода
4. Введите пароль от WiFi
5. Выберите тип подключения:
   - 1 для DHCP (автоматический IP)
   - 2 для Static IP (ручная настройка)
6. Если выбран Static IP, введите IP в формате: 192.168.1.100/24
7. Введите порт HTTP сервера (по умолчанию 80)
8. Настройка завершится, устройство перезагрузится

### Работа устройства:
- Дисплей циклически показывает 3 экрана (каждые 5 секунд):
  1. Температура (крупно)
  2. Влажность и давление
  3. Имя устройства, IP адрес и SSID

- Веб-интерфейс доступен по адресу: http://IP-адрес-устройства:порт
  Страница автоматически обновляется каждые 5 секунд

### Команды в мониторе порта:
Введите команду и нажмите Enter:
- help - показать все команды
- info - показать данные датчиков и имя устройства
- net - показать сетевые настройки
- reset - сбросить настройки к заводским
- set [параметр] [значение] - изменить настройку
  Параметры: ip, port, ssid, passw, name
  Пример: set name METEO2
- save - сохранить текущие настройки
- scan - сканировать WiFi сети
- apply - применить настройки и перезагрузиться

### Изменение настроек через команды:
1. Для изменения имени: set name НОВОЕИМЯ
2. Для изменения WiFi: set ssid НОВЫЙ_SSID
3. Затем: set passw НОВЫЙ_ПАРОЛЬ
4. Для изменения IP: set ip 192.168.1.150/24
5. Для изменения порта: set port 8080
6. Сохраните: save
7. Примените: apply

### Решение проблем:
1. Если датчики не обнаруживаются:
   - Проверьте соединения
   - Убедитесь, что адреса I2C не конфликтуют
   - Проверьте питание 3.3V

2. Если WiFi не подключается:
   - Проверьте правильность SSID и пароля
   - Используйте команду scan для поиска сетей
   - Убедитесь, что сеть в диапазоне 2.4GHz

3. Если веб-страница недоступна:
   - Проверьте IP адрес командой net
   - Убедитесь, что устройство в той же сети, что и компьютер
   - Проверьте настройки брандмауэра

4. Если дисплей не работает:
   - Проверьте соединения SDA/SCL
   - Убедитесь в правильности адреса дисплея (0x3C)

### Сброс к заводским настройкам:
1. Введите команду: reset
2. Подтвердите: yes
3. Устройство перейдет в режим настройки

Устройство готово к работе после первой успешной настройки. Данные сохраняются в энергонезависимой памяти и восстанавливаются после отключения питания.

--------------------------------------------------------

# ESP32-C3 Weather Station Setup Guide

## 1. Hardware Connection

### Required Components:
- ESP32-C3 with onboard display 0.46 OLED "The EGG"
- AHT20 module (temperature and humidity)
- BMP280 module (temperature and pressure)
- Connecting wires

### Wiring Diagram (all I2C devices on same pins):
```
ESP32-C3         OLED/AHT20/BMP280
GPIO 5 (SDA)  -> SDA (all devices)
GPIO 6 (SCL)  -> SCL (all devices)
3.3V          -> VCC (all devices)
GND           -> GND (all devices)
```

**Important:**
- AHT20 typically uses address 0x38
- BMP280 typically uses address 0x77 (check module jumper)
- OLED typically uses address 0x3C

If addresses conflict, BMP280 often has a jumper to change address from 0x77 to 0x76.

## 2. Arduino IDE Setup and Compilation

### Install Arduino IDE:
1. Download Arduino IDE from arduino.cc
2. Install on your computer

### Add ESP32-C3 Support:
1. In Arduino IDE, open File -> Preferences
2. In "Additional Boards Manager URLs" add:
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
3. Click OK

4. Open Tools -> Board -> Boards Manager
5. Search for "esp32"
6. Find "ESP32 by Espressif Systems" and install latest version
7. Close Boards Manager

### Install Required Libraries:
1. In Arduino IDE, open Sketch -> Include Library -> Manage Libraries
2. Install these libraries one by one:
   - "U8g2" by olikraus (for OLED display)
   - "Adafruit AHTX0" (for AHT20 sensor)
   - "Adafruit BMP280 Library" (for BMP280 sensor)
   - "Adafruit Unified Sensor" (may install automatically)

### Project Configuration:
1. Copy the complete sketch code into Arduino IDE
2. Select board: Tools -> Board -> ESP32 Arduino -> "ESP32-C3 Dev Module"
3. Configure settings:
   - CPU Frequency: 160MHz (WiFi)
   - Flash Size: "4MB (32Mb)"
   - Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
   - Upload Speed: "921600"
   - Port: Select your ESP32-C3 COM port

### Compilation and Upload:
1. Click "Verify" (checkmark) to compile
2. If no errors, click "Upload" (right arrow)
3. Hold BOOT button on ESP32-C3 board when clicking upload if required
4. Wait for upload to complete

## 3. Usage

### First Boot (Setup Mode):
1. Open Serial Monitor: Tools -> Serial Monitor
2. Set baud rate to 115200
3. Press RST (Reset) button on the board
4. Display will show "SETUP"
5. Serial Monitor will show setup wizard

### Step-by-Step Setup:
1. Enter device name (1-6 characters), e.g., "METEO1"
2. Wait for WiFi scan
3. Select network from list (enter number) or 'm' for manual entry
4. Enter WiFi password
5. Select connection type:
   - 1 for DHCP (automatic IP)
   - 2 for Static IP (manual)
6. If Static IP selected, enter IP in format: 192.168.1.100/24
7. Enter HTTP server port (default 80)
8. Setup completes, device reboots

### Device Operation:
- Display cycles through 3 screens (every 5 seconds):
  1. Temperature (large)
  2. Humidity and pressure
  3. Device name, IP address, and SSID

- Web interface available at: http://device-IP-address:port
  Page auto-refreshes every 5 seconds

### Serial Monitor Commands:
Type command and press Enter:
- help - show all commands
- info - show sensor data and device name
- net - show network settings
- reset - factory reset
- set [parameter] [value] - change setting
  Parameters: ip, port, ssid, passw, name
  Example: set name METEO2
- save - save current settings
- scan - scan for WiFi networks
- apply - apply settings and reboot

### Changing Settings via Commands:
1. To change name: set name NEWNAME
2. To change WiFi: set ssid NEW_SSID
3. Then: set passw NEW_PASSWORD
4. To change IP: set ip 192.168.1.150/24
5. To change port: set port 8080
6. Save: save
7. Apply: apply

### Troubleshooting:
1. If sensors not detected:
   - Check connections
   - Verify I2C addresses don't conflict
   - Check 3.3V power

2. If WiFi won't connect:
   - Verify SSID and password
   - Use scan command to find networks
   - Ensure network is 2.4GHz

3. If web page inaccessible:
   - Check IP with net command
   - Ensure device and computer on same network
   - Check firewall settings

4. If display not working:
   - Check SDA/SCL connections
   - Verify display address (0x3C)

### Factory Reset:
1. Enter command: reset
2. Confirm: yes
3. Device enters setup mode

Device is ready after first successful setup. Settings are stored in non-volatile memory and restored after power cycle.
