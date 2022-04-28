#include <Arduino.h> // Include arduino framework
#include <Wire.h>
#include <RGBmatrixPanel.h> // Include required AdaFruit Library
#include "SPIFFS.h" // SPI flash storage library
#include "SPI.h" // SPI library used for display and SPIFFS
#include <EEPROM.h> // EEPROM library
#include "BluetoothSerial.h" // ESP32 Bluetooth classic library

void readBluetooth(void); // Declare function to process data receieved via Bluetooth
void serialFlush(void); // Declare function to clear bluetooth buffer

#define CLK  15   // Pin definitions for matrix 
#define OE   33
#define LAT 32
#define A   12
#define B   16
#define C   17
#define D   4

unsigned long prevTime = 0;

// Initialise matrix with defined pins and use double buffering
RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, true); 

BluetoothSerial SerialBT; //  Initialise the Bluetooth Serial object

byte IMAGE_COUNT;
byte SLIDE_TIME;
byte FPS;
byte DECIDER;

#define FORMAT_SPIFFS_IF_FAILED false // Used to format SPIFFS on first test

void setup() {
  Serial.begin(115200); // Open serial communications and wait for port to open
  while (!Serial) { // wait for serial port to connect
    ; 
  }

  EEPROM.begin(4); // Start EEPROM using 4 bytes
  IMAGE_COUNT = EEPROM.read(0); // Read neccessary values from EEPROM
  SLIDE_TIME = EEPROM.read(1);
  FPS = EEPROM.read(2);
  DECIDER = EEPROM.read(3);

  Serial.print("Initializing SPIFFS...");

  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
      Serial.println("SPIFFS Mount Failed");
      while (1);
   }
  
  Serial.println("initialization done.");

  SerialBT.begin("WIRELESSDISPLAY"); // Start the bluetooth device with specified name

  matrix.begin(); // Start the LED display
}

void loop() { 
  File image;
  String fname;
  byte buffers[192]; // Initialise local image buffer
  int internalRows = ((sizeof(buffers)/3)/32); // Define how many rows can be set per buffer
  int mainRows = (32/internalRows); // Define how many buffers will set all rows
  byte counter;

  while(SerialBT.available() < 1) { // Repeat loop while there is nothing in bluetooth buffer
    double time1 = micros();
    for (byte x=1; x<=IMAGE_COUNT; x++) { // Iterate for all images on internal flash
      if (SerialBT.available() > 0) { // Check for data received in bluetooth buffer
        break;
      }

      fname = "/" + String(x); // Form image file name
      unsigned long start = millis();
      image = SPIFFS.open(fname, FILE_READ); // Open image file for reading
      unsigned long end = millis();
      unsigned long time = end-start;
      // Serial.println((String)x + ": " + time);

      if (DECIDER == 1) { // If GIF is being shown, use FPS to control speed of images
        // Hold until time passed since last loop is larger than the minimum time per 1 frame, defined from FPS  
        unsigned long t;
        while(((t = millis()) - prevTime) < (1000 / FPS));
        prevTime = t;
      }
      
      for(byte rows=0; rows<mainRows; rows++) { // Iterate for mainRows variable
        counter = 0;
        image.read(buffers, sizeof(buffers)); // Read defined number of bytes from SD card into a buffer
  
        for(byte irow=0; irow<internalRows; irow++) { // Iterate for internalRows variable
          for(byte column=0; column<32; column++) {
            // drawPixel in 8bit colour to output to display buffer
            matrix.drawPixel(irow+(rows*internalRows), column, matrix.Color888(buffers[counter], buffers[counter+1], buffers[counter+2]));
            counter+=3; // Increment counter to next set of bytes for next pixel
          }
        }
      }
  
      matrix.swapBuffers(false); // Output the buffer to the display LEDs 
      image.close();

      if (DECIDER == 2) { // If slide show images are being shown, use SLIDE_TIME to control speed of images
        delay(SLIDE_TIME*1000);
      }
    }
    double time2 = micros();
    double fps = (IMAGE_COUNT/(time2-time1))*1000000.0;
    // Serial.println(fps);
  }
  readBluetooth(); // If Serail1 buffer has data, call function to read BT data
}

void readBluetooth() {
  // Output "Receiving data" message to display
  matrix.fillScreen(matrix.Color333(0, 0, 0));
  matrix.setCursor(0, 0);
  matrix.setTextSize(1);
  matrix.setTextWrap(true); 
  matrix.setTextColor(matrix.Color333(7,7,7));
  matrix.println("Receiving Data...");
  matrix.swapBuffers(false);
  
  int bytesize = 256; // Define how many bytes will be incoming per chunk
  int chunksize = 3072/bytesize; // Define how many chunks there will be per image
  byte btbuffer[bytesize]; // Initialise a local buffer the size of each chunk

  // Read image count and decider byte from BT and write to EEPROM
  IMAGE_COUNT = SerialBT.read();
  DECIDER = SerialBT.read();

  EEPROM.write(0, IMAGE_COUNT);
  EEPROM.write(3, DECIDER);

  switch (DECIDER)
  {
    case 1: // If decider indicates incoming GIF, read FPS and write to EEPROM
      FPS = SerialBT.read();
      EEPROM.write(2, FPS);
      break;
    
    case 2: // If decider indicates incoming slideshow images, read FPS and write to EEPROM
      SLIDE_TIME = SerialBT.read();
      EEPROM.write(1, SLIDE_TIME);
      break;
  }

  EEPROM.commit(); // Copy writes physically to EEPROM
  SerialBT.write(1); // Send byte to App to confirm data has been processed

  // for (int x=1; x<=IMAGE_COUNT; x++) { // Remove any existing files that are going to be overwritten
  //   if (fs.exists("/" + String(x))) {
  //     fs.remove("/" + String(x));
  //   }
  // }

  for(int count=1; count<=IMAGE_COUNT; count++) { // Iterate for all incoming images
    String filename = "/" + String(count); // Form image file name
    File saveBluetooth = SPIFFS.open(filename, FILE_WRITE); // Open image file for writing
    
    for(int chunks=0; chunks<chunksize; chunks++) { // Iterate for all bytes per chunk
      for(int single_byte=0; single_byte<bytesize; single_byte++) { 
        
        while (SerialBT.available() < 1) { // Hold while there is no data in the Serial1 buffer
          ;
        }

        // Serial.println((String) "Confirm start " + chunks + ": " + single_byte);

        btbuffer[single_byte] = SerialBT.read(); // Store byte from bluetooth into the local buffer
        // Serial.println((String) btbuffer[single_byte]);
      }
      saveBluetooth.write(btbuffer, 256); // Write the local buffer to the file
      // Serial.println((String) "Confirm write " + chunks);

      if(chunks+1 % (512/bytesize) == 0) { // When 512 bytes are written to the file, copy the data physically to the SD card using flush()
        saveBluetooth.flush();
      }

      SerialBT.write(1); // Send byte to App to confirm a chunk has been processed
      // Serial.println((String) "Confirm callback " + chunks);
    }
    saveBluetooth.close();

    // Output a progress percentage counter to matrix based on how many images have been processed
    matrix.fillScreen(matrix.Color333(0, 0, 0));
    matrix.setCursor(0, 0);
    matrix.setTextSize(1);
    matrix.setTextWrap(true);
    matrix.setTextColor(matrix.Color333(7,7,7));
    matrix.println("Receiving Data.");
    int percent = (count*100)/IMAGE_COUNT;
    matrix.println((String) percent + "%");
    matrix.swapBuffers(false);
  }
  serialFlush(); // Call function to clear the Serial1 buffer
}

void serialFlush(){
  while(SerialBT.available() > 0) {
    SerialBT.read(); // Read the buffer while it is larger than 0
  }
}