#include <OctoWS2811.h>
#include <QNEthernet.h>

// QNEthernet setup
using namespace qindesign::network;

EthernetUDP udp;
const uint16_t ARTNET_PORT = 6454;
const int ARTNET_MAX_BUFFER = 530;
uint8_t packetBuffer[ARTNET_MAX_BUFFER];

const IPAddress staticIP(2, 0, 0, 20); // Teensy IP, 2.0.0.20
const IPAddress subnetMask(255, 0, 0, 0);
const IPAddress gatewayIP(2, 0, 0, 10); // Laptop IP, 2.0.0.10

// Device configuration.
// This assumes all devices in the array are the same configuration.
// For panels (DEVICE_HEIGHT > 1) it assumes a column-major serpentine layout, like the BTF Lighting 8x32, 16x16, and 8x8.
// OctoWS2811 splits all outputs to equal number of LEDs. Heterogeneous configurations are theoretically possible by 
// configuring to the device with the most total LEDs, but this code as-written does not currently support that.
const int DEVICE_WIDTH = 32;
const int DEVICE_HEIGHT = 8; // Set to 1 for strips
const int DEVICE_LEDS = DEVICE_WIDTH * DEVICE_HEIGHT; 
const int NUM_DEVICES = 2;

// OctoWS2811:
// length parameter = LEDs per strip
DMAMEM int displayMemory[DEVICE_LEDS * 6]; // The *6 allocates enough memory for 8 outputs (3 bytes/RGB pixel * 8 = 24, which is 6x 4-byte ints)
int drawingMemory[DEVICE_LEDS * 6];
const int config = WS2811_GRB | WS2811_800kHz;
OctoWS2811 leds(DEVICE_LEDS, displayMemory, drawingMemory, config);

// DMX Universe mapping parameters. We assume that all devices mapped from this controller will be incrementally addressed
// from a fixed starting universe. We also assume that the pixels will within each device will be split into incremental
// universes *per device* rather than treating the entire array as a one large device/screen.
const int DMX_START_UNIVERSE = 1;
const int ARTNET_MAX_DATA_BYTES = 512;
const int BYTES_PER_PIXEL = 3;
const int MAX_PIXELS_PER_UNIVERSE = ARTNET_MAX_DATA_BYTES / BYTES_PER_PIXEL; // 170 pixels per universe
const int UNIVERSES_PER_DEVICE = (DEVICE_LEDS + (MAX_PIXELS_PER_UNIVERSE - 1)) / MAX_PIXELS_PER_UNIVERSE;
const int NUM_UNIVERSES = UNIVERSES_PER_DEVICE * NUM_DEVICES;
int universeToDeviceIndex[NUM_UNIVERSES];
int universeToIntraDeviceIndex[NUM_UNIVERSES];
int universeToPixelLength[NUM_UNIVERSES];
uint64_t universesReceived = 0;
uint64_t allUniverses;

void populateUniverseLookups() {
  for (int i = 0; i < NUM_UNIVERSES; i++) {
    universeToDeviceIndex[i] = i / UNIVERSES_PER_DEVICE;
    universeToIntraDeviceIndex[i] = i % UNIVERSES_PER_DEVICE;
    int maxPossiblePixelIndex = (universeToIntraDeviceIndex[i] + 1) * MAX_PIXELS_PER_UNIVERSE;
    int leftoverPixels = DEVICE_LEDS % MAX_PIXELS_PER_UNIVERSE;
    universeToPixelLength[i] = (maxPossiblePixelIndex > DEVICE_LEDS) ? leftoverPixels : 170;
    allUniverses = allUniverses | 0x01 << i;
  }
}

int currentIndex = 0;
uint32_t received = 0x00;

#ifdef DEBUG
elapsedMicros frameTimer = 0;
elapsedMicros drawTimer = 0;
#endif

// LED panel device mapping.
// The input coordinate system is defined as right-handed (x-right, y-down) and row-major.
// This currently assumes that the LED devices are a column-major serpentine layout.

// Maps an index in the input coordinate system to an index into the physical LED device.
// Usage example: leds.setPixel(mapIndex(i), input[i]);
int mapIndex(int input_index) {
  int x = input_index % DEVICE_WIDTH;
  int y = input_index / DEVICE_WIDTH;
  int output_index;
  if (x % 2 == 0) {
    output_index = x * DEVICE_HEIGHT + y;
  } else {
    output_index = x * DEVICE_HEIGHT + (DEVICE_HEIGHT - 1 - y);
  }
  return output_index;
}

// Maps an index in the output physical LED device coordinate system to the input coordinate system.
// Usage example: leds.setPixel(i, input[reverseMapIndex(i)]);
int reverseMapIndex(int output_index) {
  int x = output_index / DEVICE_HEIGHT;
  int y_prime = output_index % DEVICE_HEIGHT;
  int y = (x % 2 == 0) ? (y_prime) : (DEVICE_HEIGHT - 1 - y_prime);
  int input_index = y * DEVICE_WIDTH + x;
  return input_index;
}
/*
This was to convert xy coordinates to a global index, treating the whole panel array as
a single device. 
int xyToIndex(int x, int y) {
  // Which panel is the pixel on? We will assume just our one column of panels for now.
  int panel_x = x / PANEL_WIDTH;
  int panel_y = y / PANEL_HEIGHT;

  // Global index to panel-local index
  int local_x = x % PANEL_WIDTH;
  int local_y = y % PANEL_HEIGHT;

  // If we weren't serpentine we'd just go like this (because column-major):
  // This should work for just a column-scanning test 
  // To compensate for serpentine, on odd rows we need to mirror the y-index
  if (local_x % 2 == 1) local_y = (PANEL_HEIGHT - 1) - local_y;
  int index = panel_y * PANEL_LEDS + local_x * PANEL_HEIGHT + local_y;
  return index;
} */

int processPacket(int packetSize) {
  udp.read(packetBuffer, packetSize);
  /*
  Serial.println("Packet data:");
  for (int i = 0; i < packetSize; i++) {
    Serial.print(packetBuffer[i], HEX);
    Serial.print(" ");
  }
  */
  uint16_t universe = 0;
  if (memcmp(packetBuffer, "Art-Net", 8) == 0) {
    Serial.println("Received Art-Net");
    uint16_t opCode = packetBuffer[8] | (packetBuffer[9] << 8);
    if (opCode == 0x5000) {
      Serial.println("Received ArtDMX OpCode");
      universe = packetBuffer[14] | (packetBuffer[15] << 8);
      Serial.printf("Universe: %lu ", universe);
      uint16_t dataLength = (packetBuffer[16] << 8) | (packetBuffer[17]);
      Serial.printf("Data Length: %lu \n", dataLength);

      uint8_t* dmxData = &packetBuffer[18]; 
      
      Serial.printf("Universe to pixel length: %lu \n", universeToPixelLength[universe - DMX_START_UNIVERSE]);
      Serial.printf("Universe starting pixel: %lu \n", MAX_PIXELS_PER_UNIVERSE * universeToIntraDeviceIndex[universe - DMX_START_UNIVERSE]);
      for (int i = 0; i < universeToPixelLength[universe - DMX_START_UNIVERSE]; i++) {
        leds.setPixel(universeToDeviceIndex[universe - DMX_START_UNIVERSE] * DEVICE_LEDS + mapIndex(i + MAX_PIXELS_PER_UNIVERSE * universeToIntraDeviceIndex[universe - DMX_START_UNIVERSE]),
            dmxData[BYTES_PER_PIXEL * i],
            dmxData[BYTES_PER_PIXEL * i + 1],
            dmxData[BYTES_PER_PIXEL * i + 2]);
      }
      //leds.show();
      /*
      if (universe == 1) {
        for (int i = 0; i < dataLength / 3; i++) {
          leds.setPixel(mapIndex(i), dmxData[3 * i], dmxData[3 * i + 1], dmxData[3 * i + 2]);
        }       
        leds.show(); 
      }
      if (universe == 2) {
        for (int i = 0; i < 86; i++) {
          leds.setPixel(mapIndex(i + 170), dmxData[3 * i], dmxData[3 * i + 1], dmxData[3 * i + 2]);
        }   
        leds.show();     
      }*/
    }
  }
  return universe;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  while (!Serial && millis() < 3000);

  Serial.println("Initiating serial debugging!");

  populateUniverseLookups();

    // Initialize Static IP
  Ethernet.begin(staticIP, subnetMask, gatewayIP);

  // Block execution until a physical cable link is registered
  Serial.print("Checking for physical link...");
  while (!Ethernet.linkState()) {
    delay(100);
    delay(100);
  }
  Serial.println(" Connected!");

  udp.begin(ARTNET_PORT);

  Serial.print("Teensy Static IP: ");
  Serial.println(Ethernet.localIP());

  leds.begin();
  leds.show();
}

void loop() {
  // put your main code here, to run repeatedly:
  #ifdef DEBUG
  frameTimer = 0;
  #endif

  #ifdef DEBUG
  uint32_t totalGenerationTime = frameTimer;
  #endif

  #ifdef DEBUG
  drawTimer = 0;
  #endif

  // leds.show();

  #ifdef DEBUG
  uint32_t totalDrawTime = drawTimer;
  #endif

  #ifdef DEBUG
  Serial.printf("Frame generation time: %lu us\n", totalGenerationTime);
  Serial.printf("Draw time: %lu us\n", totalDrawTime);
  delay(1000);
  #endif

  // Check if a packet has arrived in the hardware network buffer
  int packetSize = udp.parsePacket();
  uint16_t universe = 0;
  if (packetSize > 0) {
    // 2. Read packet properties for validation via Serial
    Serial.printf("Packet Received! Size: %d bytes | From: %d\n", 
                  packetSize,
                  udp.remotePort());
    universe = processPacket(packetSize);
  }
  if (universe > 0) universesReceived |= 0x01 << (universe - DMX_START_UNIVERSE);
  if (universesReceived == allUniverses) {
    leds.show();
    universesReceived = 0;
  }
  

  /*
  for (int y = 0; y < NUM_PANELS * PANEL_HEIGHT; y++) {
    for (int x = 0; x < PANEL_WIDTH; x++) {
      if (y == currentRow) leds.setPixel(xyToIndex(x, y), 255, 0, 0);
      else if (x == currentCol) leds.setPixel(xyToIndex(x, y), 255, 255, 0);
      else leds.setPixel(xyToIndex(x,y), 0, 0, 0);
    }
  }
  leds.show();
  delay(100);
  currentRow++;
  currentCol++;
  //if (currentRow >= PANEL_HEIGHT * NUM_PANELS) currentRow = 0;
  //if (currentCol >= PANEL_WIDTH) currentCol = 0;
  if (currentRow >= PANEL_HEIGHT * NUM_PANELS) currentRow = 0;
  if (currentCol >= PANEL_WIDTH) currentCol = 0;
  */
}
