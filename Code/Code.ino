#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// =====================================================
// SX1278 LORA PINS
// =====================================================
#define LORA_SCK 2
#define LORA_MOSI 3
#define LORA_MISO 4
#define LORA_CS 5
#define LORA_RST 6
#define LORA_DIO0 7

// =====================================================
// ST7789 DISPLAY PINS
// =====================================================
#define TFT_SCK 10
#define TFT_MOSI 11
#define TFT_CS 17
#define TFT_DC 20
#define TFT_RST 21
#define TFT_BLK 22

#define TFT_WIDTH 240
#define TFT_HEIGHT 135

// =====================================================
// SX1278 REGISTERS
// =====================================================
#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_FIFO_RX_CURRENT 0x10
#define REG_IRQ_FLAGS 0x12
#define REG_RX_NB_BYTES 0x13
#define REG_FIFO_ADDR_PTR 0x0D
#define REG_MODEM_CONFIG1 0x1D
#define REG_MODEM_CONFIG2 0x1E
#define REG_MODEM_CONFIG3 0x26
#define REG_PREAMBLE_MSB 0x20
#define REG_PREAMBLE_LSB 0x21
#define REG_SYNC_WORD 0x39
#define REG_VERSION 0x42

#define LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_RX_CONTINUOUS 0x05

#define IRQ_RX_DONE 0x40
#define IRQ_PAYLOAD_CRC_ERROR 0x20

// =====================================================
// LORA SPI
// =====================================================
SPISettings loraSPI(1000000, MSBFIRST, SPI_MODE0);

// =====================================================
// DISPLAY (SPI1)
// =====================================================
SPIClassRP2040 displaySPI(spi1, 12, TFT_CS, TFT_SCK, TFT_MOSI);

Adafruit_ST7789 tft(&displaySPI, TFT_CS, TFT_DC, TFT_RST);

// =====================================================
// GLOBAL VARIABLES
// =====================================================
uint32_t packetCount = 0;
String lastMessage = "Waiting for message...";
String latitude = "--";
String longitude = "--";
bool hasLocation = false;

// =====================================================
// LORA REGISTER FUNCTIONS
// =====================================================
void writeRegister(uint8_t reg, uint8_t value) {
  SPI.beginTransaction(loraSPI);
  digitalWrite(LORA_CS, LOW);
  SPI.transfer(reg | 0x80);
  SPI.transfer(value);
  digitalWrite(LORA_CS, HIGH);
  SPI.endTransaction();
}

uint8_t readRegister(uint8_t reg) {
  SPI.beginTransaction(loraSPI);
  digitalWrite(LORA_CS, LOW);
  SPI.transfer(reg & 0x7F);
  uint8_t value = SPI.transfer(0x00);
  digitalWrite(LORA_CS, HIGH);
  SPI.endTransaction();
  return value;
}

void resetLoRa() {
  digitalWrite(LORA_RST, LOW);
  delay(10);
  digitalWrite(LORA_RST, HIGH);
  delay(10);
}

void setFrequency(long frequency) {
  uint32_t frf = (uint32_t)((frequency / 32000000.0) * 524288);
  writeRegister(REG_FRF_MSB, (frf >> 16) & 0xFF);
  writeRegister(REG_FRF_MID, (frf >> 8) & 0xFF);
  writeRegister(REG_FRF_LSB, frf & 0xFF);
}

// =====================================================
// DISPLAY FUNCTIONS
// =====================================================
void drawHeader() {
  tft.fillRect(0, 0, 240, 26, ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(8, 5);
  tft.print("LORA RECEIVER");
  tft.fillCircle(226, 13, 5, ST77XX_GREEN);
}

void drawStatusBar() {
  tft.fillRect(0, 117, 240, 18, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 122);
  tft.print("433MHz  SF7  BW125");
  tft.setCursor(175, 122);
  tft.print("#");
  tft.print(packetCount);
}

void displayStartup() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(40, 25);
  tft.print("LORA");
  tft.setCursor(30, 50);
  tft.print("RECEIVER");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(45, 80);
  tft.print("SX1278  |  433 MHz");
  tft.setCursor(60, 100);
  tft.print("Initializing...");
  delay(1500);

  tft.fillScreen(ST77XX_BLACK);
  drawHeader();
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(25, 55);
  tft.print("Waiting for LoRa message...");
  drawStatusBar();
}

void displayMessage(String message) {
  tft.fillScreen(ST77XX_BLACK);
  drawHeader();

  // ---- Parse location from message ----
  hasLocation = false;
  latitude = "--";
  longitude = "--";

  int latIndex = message.indexOf("LAT:");
  int lonIndex = message.indexOf("LON:");

  if (latIndex != -1 && lonIndex != -1) {
    // Extract Latitude
    int latStart = latIndex + 4;
    int latEnd = message.indexOf(' ', latStart);
    if (latEnd == -1) latEnd = message.length();
    latitude = message.substring(latStart, latEnd);
    latitude.trim();

    // Extract Longitude
    int lonStart = lonIndex + 4;
    int lonEnd = message.indexOf(' ', lonStart);
    if (lonEnd == -1) lonEnd = message.length();
    longitude = message.substring(lonStart, lonEnd);
    longitude.trim();

    hasLocation = true;
  }

  // ---- Main Title ----
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(8, 32);
  tft.print("ALERT");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(8, 45);
  tft.print("PERSON DETECTED");

  // ---- Location Section ----
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(8, 72);
  tft.print("LOCATION");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);

  if (hasLocation) {
    tft.setCursor(8, 88);
    tft.print("Lat: ");
    tft.print(latitude);

    tft.setCursor(8, 102);
    tft.print("Lon: ");
    tft.print(longitude);
  } else {
    tft.setCursor(8, 90);
    tft.setTextColor(ST77XX_RED);
    tft.print("GPS: NO FIX");
  }

  drawStatusBar();
}

void displayCRCError() {
  tft.fillScreen(ST77XX_BLACK);
  drawHeader();

  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.setCursor(50, 50);
  tft.print("CRC ERROR");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(30, 85);
  tft.print("Corrupted packet received");
  drawStatusBar();
  delay(800);
}

// =====================================================
// LORA SETUP
// =====================================================
void setupLoRa() {
  resetLoRa();

  uint8_t version = readRegister(REG_VERSION);
  Serial.print("SX1278 Version: 0x");
  Serial.println(version, HEX);

  writeRegister(REG_OP_MODE, LONG_RANGE_MODE | MODE_SLEEP);
  delay(10);
  writeRegister(REG_OP_MODE, LONG_RANGE_MODE | MODE_STDBY);

  setFrequency(433000000);

  writeRegister(REG_MODEM_CONFIG1, 0x72);  // BW=125kHz, CR=4/5
  writeRegister(REG_MODEM_CONFIG2, 0x74);  // SF7 + CRC
  writeRegister(REG_MODEM_CONFIG3, 0x04);
  writeRegister(REG_PREAMBLE_MSB, 0x00);
  writeRegister(REG_PREAMBLE_LSB, 0x08);
  writeRegister(REG_SYNC_WORD, 0x12);
  writeRegister(REG_IRQ_FLAGS, 0xFF);

  writeRegister(REG_OP_MODE, LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
  Serial.println("LoRa receiver ready.");
}

// =====================================================
// RECEIVE PACKET
// =====================================================
void receivePacket() {
  uint8_t irq = readRegister(REG_IRQ_FLAGS);

  if (!(irq & IRQ_RX_DONE)) return;

  if (irq & IRQ_PAYLOAD_CRC_ERROR) {
    Serial.println("CRC ERROR");
    displayCRCError();
    writeRegister(REG_IRQ_FLAGS, 0xFF);
    return;
  }

  uint8_t length = readRegister(REG_RX_NB_BYTES);
  uint8_t fifoAddress = readRegister(REG_FIFO_RX_CURRENT);
  writeRegister(REG_FIFO_ADDR_PTR, fifoAddress);

  String message = "";
  Serial.print("Received: ");

  for (uint8_t i = 0; i < length; i++) {
    uint8_t data = readRegister(REG_FIFO);
    Serial.write(data);
    message += (char)data;
  }
  Serial.println();

  packetCount++;
  lastMessage = message;

  displayMessage(message);  // Updated display function

  writeRegister(REG_IRQ_FLAGS, 0xFF);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1500);

  // LoRa pins
  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_CS, HIGH);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);
  pinMode(LORA_DIO0, INPUT);

  // LoRa SPI (SPI0)
  SPI.setSCK(LORA_SCK);
  SPI.setTX(LORA_MOSI);
  SPI.setRX(LORA_MISO);
  SPI.begin();

  // Display backlight
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  // Display SPI (SPI1)
  displaySPI.setSCK(TFT_SCK);
  displaySPI.setTX(TFT_MOSI);
  displaySPI.begin();

  // Display init
  tft.init(135, 240);
  tft.setRotation(3);  // Landscape 240x135
  tft.fillScreen(ST77XX_BLACK);

  displayStartup();
  setupLoRa();
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  receivePacket();
  delay(10);
}