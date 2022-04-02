#include <Arduino.h>
#include <RGBmatrixPanel.h> // Required AdaFruit Libraries
#include <Wire.h>

#include "FS.h"
#include "SPI.h" // SD libraries
#include "SD.h"
#include <EEPROM.h>
#include "BluetoothSerial.h"

void readBluetooth(void);
void serialFlush(void);

#define CLK  15   // USE THIS ON ADAFRUIT METRO M0, etc.
#define OE   33
#define LAT 32
#define A   12
#define B   16
#define C   17
#define D   4

uint32_t prevTime = 0;

RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, true);

BluetoothSerial SerialBT;

byte IMAGE_COUNT;
byte SLIDE_TIME;
byte FPS;
byte DECIDER;

void setup() {
  Serial.begin(115200); // Open serial communications and wait for port to open
  while (!Serial) { // wait for serial port to connect
    ; 
  }

  EEPROM.begin(4);
  IMAGE_COUNT = EEPROM.read(0);
  SLIDE_TIME = EEPROM.read(1);
  FPS = EEPROM.read(2);
  DECIDER = EEPROM.read(3);

  Serial.print("Initializing SD card...");
  
  const int chipSelect = 5; // Define cs pin for sd card
  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed!"); // Check to see if SD is recognised
    while (1);
  }
  Serial.println("initialization done.");

  // communication with the BT module on SerialBT
  SerialBT.begin("WIRELESSDISPLAY");

  matrix.begin(); // Start the LED display
}

void loop() {
  // put your main code here, to run repeatedly:
  fs::FS &fs = SD;
  File image;
  String fname;
  byte buffers[192];
  int internalRows = ((sizeof(buffers)/3)/32);
  int mainRows = (32/internalRows);
  byte counter;

  while(SerialBT.available() < 1) {
    float time1 = micros();
    for (byte x=1; x<=IMAGE_COUNT; x++) { // Iterate for all images on sd card
      if (SerialBT.available() > 0) {
        break;
      }
      fname = "/" + String(x); // Form image file name
      unsigned long t1 = millis();
      image = fs.open(fname, FILE_READ); //open image file for reading
      unsigned long t2 = millis();
      unsigned long time = (t2-t1);

      if (DECIDER == 1) {
        uint32_t t;
        while(((t = millis()) - prevTime) < (1000 / FPS));
        prevTime = t;
      }
      
      for(byte rows=0; rows<mainRows; rows++) {
        counter = 0;
        image.read(buffers, sizeof(buffers));
  
        for(byte irow=0; irow<internalRows; irow++) {
          for(byte column=0; column<32; column++) {
            matrix.drawPixel(irow+(rows*internalRows), column, matrix.Color888(buffers[counter], buffers[counter+1], buffers[counter+2])); // Draw the RGB pixel
            counter+=3;
          }
        }
      }
  
      matrix.swapBuffers(false);

      image.close();

      if (DECIDER == 2) {
        delay(SLIDE_TIME*1000);
      }
    }

    float time2 = micros();
    float fps = (IMAGE_COUNT/(time2-time1))*1000000.0;
    Serial.println(fps);
  }

  readBluetooth();
}

void readBluetooth() {
  fs::FS &fs = SD;
  matrix.fillScreen(matrix.Color333(0, 0, 0));
  matrix.setCursor(0, 0);    // start at top left, with one pixel of spacing
  matrix.setTextSize(1);     // size 1 == 8 pixels high
  matrix.setTextWrap(true); 
  matrix.setTextColor(matrix.Color333(7,7,7));
  matrix.println("Recieving Data...");
  matrix.swapBuffers(false);
  
  int bytesize = 128;
  int chunksize = 3072/bytesize;
  byte btbuffer[bytesize];

  IMAGE_COUNT = SerialBT.read();
  DECIDER = SerialBT.read();

  EEPROM.write(0, IMAGE_COUNT);
  EEPROM.write(3, DECIDER);

  switch (DECIDER)
  {
  case 1:
    FPS = SerialBT.read();
    EEPROM.write(2, FPS);
    break;
  
  case 2:
    SLIDE_TIME = SerialBT.read();
    EEPROM.write(1, SLIDE_TIME);
    break;
  }

  EEPROM.commit();
  SerialBT.write(1);
  
  for (int x=1; x<=IMAGE_COUNT; x++) {
    if (fs.exists(String(x))) {
      fs.remove(String(x));
    }
  }

  for(int count=1; count<=IMAGE_COUNT; count++) {
    String filename = "/" + String(count); // Form image file name
    File saveBluetooth = fs.open(filename, FILE_WRITE); //open image file for writing
    
    for(byte chunks=0; chunks<chunksize; chunks++) {
      for(byte single_byte=0; single_byte<bytesize; single_byte++) {
        while (SerialBT.available() < 1) {
          ;
        }
        btbuffer[single_byte] = SerialBT.read();
      }
      saveBluetooth.write(btbuffer, bytesize);

     if(chunks+1 % (512/bytesize) == 0) { // When 512 bytes are written to the file, copy the data physically to the SD card using flush()
       saveBluetooth.flush();
     }
      SerialBT.write(1);
    }
    saveBluetooth.close();
    Serial.println("Image done");
    
    matrix.fillScreen(matrix.Color333(0, 0, 0));
    matrix.setCursor(0, 0);    // start at top left, with one pixel of spacing
    matrix.setTextSize(1);     // size 1 == 8 pixels high
    matrix.setTextWrap(true);
    matrix.setTextColor(matrix.Color333(7,7,7));
    matrix.println("Recieving Data.");
    int percent = (count*100)/IMAGE_COUNT;
    matrix.println((String) percent + "%");
    matrix.swapBuffers(false);
  }
  serialFlush();
}

void serialFlush(){
  while(SerialBT.available() > 0) {
    SerialBT.read();
  }
}