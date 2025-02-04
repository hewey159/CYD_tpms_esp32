#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPI.h>
#include <XPT2046_Bitbang.h>
#include <TFT_eSPI.h>
#include <Preferences.h>  // Non-volatile storages


#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define MAX_DEVICES 20


XPT2046_Bitbang ts(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS);
TFT_eSPI tft = TFT_eSPI();
TFT_eSPI_Button add_new_sensor_button; // Create the toggle button
TFT_eSPI_Button temps_button; // Create the toggle button
TFT_eSPI_Button config_button; // Create the toggle button
TFT_eSPI_Button remove_button; // Goes to the remove page


struct RAWTPMSDevice {
  String name; // Device name
  String data; // Additional data (e.g., sensor data)
};


enum page { CONFIG, ADD_NEW, TEMPS, REMOVE };
page current_page = TEMPS;

unsigned long lastPressTime = 0;
Preferences preferences;


// Bluetooth Scanner Variables
BLEScan* pBLEScan;
#define MAX_DEVICES 20
int scanTime = 5; // In seconds
RAWTPMSDevice devices_added[MAX_DEVICES];  // Store TPMS device names
RAWTPMSDevice new_devices_to_add[MAX_DEVICES];  // Store TPMS device name
int newDeviceCount = 0;
int deviceCount = 0;
int scrollPos = 0;
bool data_updated = false;
bool found_more_devices = false;


// Array to store buttons for new devices
TFT_eSPI_Button new_device_buttons[MAX_DEVICES];

// Array to remove buttons for devices
TFT_eSPI_Button remove_devices_buttons[MAX_DEVICES];

// Define a struct to represent the TyreMessage
struct TyreMessage {
  int number;       // Sensor number
  String name;      // Sensor name
  float psi;        // PSI value
  float temp;       // Temperature value
  int battery;      // Battery level
  int warning;      // Warning flag
  String sensor_id; // Sensor ID
};

// RAWTPMSDevice LeftFront;
// RAWTPMSDevice RightFront;
// RAWTPMSDevice LeftRear;
// RAWTPMSDevice RightRear;


void getDevicesFromPreferences(){
  // Load saved devices
  preferences.begin("tpms_data", false);
  deviceCount = preferences.getInt("deviceCount", 0);
  // Load each device's fields
  for (int i = 0; i < deviceCount; i++) {
    String prefix = "device" + String(i); // Unique key prefix for each device
    devices_added[i].name = preferences.getString((prefix + "_name").c_str(), ""); // Load name
    devices_added[i].data = preferences.getString((prefix + "_data").c_str(), ""); // Load data
  }
  preferences.end();
}

void saveDevicesToPreferences(RAWTPMSDevice devices[], int size) {
  preferences.begin("tpms_data", false); // Open preferences in read/write mode

  // Save the number of devices
  preferences.putInt("deviceCount", size);

  // Save each device's fields
  for (int i = 0; i < size; i++) {
    String prefix = "device" + String(i); // Unique key prefix for each device
    preferences.putString((prefix + "_name").c_str(), devices[i].name); // Save name
    preferences.putString((prefix + "_data").c_str(), devices[i].data); // Save data
  }

  preferences.end(); // Close preferences
}


class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      // Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());

      String deviceName = advertisedDevice.getName().length() > 0 ? advertisedDevice.getName().c_str() : "Unknown Device";
      String data = advertisedDevice.getManufacturerData();
      // Serial.println("advertisedDevice.getManufacturerData()");
      // Serial.println(advertisedDevice.getManufacturerData());
      // Serial.println(advertisedDevice.getManufacturerData().c_str());

      uint8_t* manufacturerDataBytes = (uint8_t*)data.c_str();  // Convert to byte array
      size_t manufacturerDataLength = data.length();            // Get length of Manufacturer Data
      // Serial.print("Manufacturer Data (raw, hex): ");
      // Serial.println(byteToHexString(manufacturerDataBytes, manufacturerDataLength));

      if (isTPMSDevice(deviceName) && isNewDevice(deviceName) && newDeviceCount < MAX_DEVICES) {
        new_devices_to_add[newDeviceCount].name = deviceName;
        new_devices_to_add[newDeviceCount].data = advertisedDevice.toString();
        newDeviceCount++;
        found_more_devices = true;
      }
      int index = getDeviceIndex(deviceName);
      if (index != -1){
        //if exsiting device then update the device data if it hasn't changed
        if(advertisedDevice.toString() != devices_added[index].data){
          devices_added[index].data = advertisedDevice.toString();
          data_updated = true;
        }
      }
    }
};

void setup() {
  Serial.begin(115200);
  ts.begin();
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  
  // Initialize Bluetooth function
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  // Create a task to handle the BLE scanning
  xTaskCreate(
    scanTask,    // Function to implement the task
    "ScanTask",  // Name of the task
    10000,       // Stack size in words
    NULL,        // Task input parameter
    1,           // Priority of the task
    NULL         // Task handle
  );
   
  //uncomment to clear devices:
  
  // Load saved devices
  // preferences.begin("tpms_data", false);
  // preferences.clear();
  // preferences.end();

  getDevicesFromPreferences();
  
  // Button in the bottom-right corner (x = 270, y = 220)
  add_new_sensor_button.initButton(&tft, 270, 220, 80, 40, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Add", 2);
  config_button.initButton(&tft, 30, 220, 80, 40, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Config", 2);
  temps_button.initButton(&tft, 270, 220, 80, 40, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Temps", 2);
  remove_button.initButton(&tft, 150, 220, 80, 40, TFT_BLACK, TFT_RED, TFT_WHITE, "Remove", 2);
  Serial.println("init current_page");
  Serial.println(current_page);
  drawUI();
}




void scanTask(void * parameter) {
  for (;;) {
    BLEScanResults *foundDevices = pBLEScan->start(scanTime, false);
    // Serial.print("Devices found: ");
    // Serial.println(foundDevices->getCount());
    // Serial.println("Scan done!");

    // if (foundNewDevice) {
    //   for (int i = 0; i < newDeviceCount; i++) {
    //     devices_added[deviceCount] = new_devices_to_add[i];
    //     deviceCount++;
    //     new_devices_to_add[i] = "";
    //   }
    //   newDeviceCount = 0;
    //   foundNewDevice = false;
    // }

    // for (int i = 0; i < deviceCount; i++) {
    //   Serial.println(devices_added[i]);
    // }

    pBLEScan->clearResults();   // delete results from BLEScan buffer to release memory
    vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for 2 seconds
  }
}


void drawUI() {
  tft.fillScreen(TFT_BLACK);

  Serial.println("draw current_page");
  Serial.println(current_page);
  if (current_page == ADD_NEW) {
    draw_add_new_ui();
  } else if (current_page == TEMPS ) {
    draw_temps();
  } else if (current_page == CONFIG ) {
    temps_button.drawButton();
    tft.setCursor(5, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.print("Configure TPMS Devices...");
  } else if (current_page == REMOVE ) {
    draw_remove_page();
  } else {
    temps_button.drawButton();
    tft.setCursor(5, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.print("Something went wrong");
  }
}

void clearRAWTPMSDeviceArray(RAWTPMSDevice arr[], int size) {
  for (int i = 0; i < size; i++) {
    arr[i].name = ""; // Clear the name field
    arr[i].data = ""; // Clear the data field
  }
}


void loop() {
  TouchPoint p = ts.getTouch();

  if(current_page == TEMPS && data_updated == true){
    drawUI();
    data_updated = false;
  }
  //update devices add page if found more
  if(current_page == ADD_NEW && found_more_devices == true){
    drawUI();
    found_more_devices = false;
  }

  if (p.zRaw > 0 && millis() - lastPressTime > 100) { // Debounce press
    if (add_new_sensor_button.contains(p.x, p.y)) {
      add_new_sensor_button.press(true);
    } else {
      add_new_sensor_button.press(false);
    }
    if (temps_button.contains(p.x, p.y)) {
      temps_button.press(true);
    } else {
      temps_button.press(false);
    }
    if (config_button.contains(p.x, p.y)) {
      config_button.press(true);
    } else {
      config_button.press(false);
    }
    if (remove_button.contains(p.x, p.y)) {
      remove_button.press(true);
    } else {
      remove_button.press(false);
    }

    if (add_new_sensor_button.justPressed() && current_page != ADD_NEW) {
      Serial.println("chagning page add new");
      Serial.println(current_page);
      current_page = ADD_NEW;
      drawUI();
      lastPressTime = millis();
    }
    else if (temps_button.justPressed() && current_page != TEMPS) {
      Serial.println("chagning page temps");
      Serial.println(current_page);
      current_page = TEMPS;
      drawUI();
      lastPressTime = millis();
    }
    else if (config_button.justPressed() && current_page != CONFIG) {
      Serial.println("chagning page config");
      Serial.println(current_page);
      current_page = CONFIG;
      drawUI();
      lastPressTime = millis();
    }
    else if (remove_button.justPressed() && current_page != REMOVE) {
      Serial.println("chagning page remove");
      Serial.println(current_page);
      current_page = REMOVE;
      drawUI();
      lastPressTime = millis();
    }

    if(current_page == ADD_NEW){
      // Check if any new device button is pressed
      Serial.println("newDeviceCount");
      Serial.println(newDeviceCount);
      for (int i = 0; i < newDeviceCount; i++) {
        Serial.println("checking buttons");
        if (new_device_buttons[i].contains(p.x, p.y)) {
          new_device_buttons[i].press(true);
        } else {
          new_device_buttons[i].press(false);
        }

        // If a new device button is pressed, move it to the devices array
        if (new_device_buttons[i].justPressed()) {
          Serial.println("button pressed for adding");
          Serial.println(i);
          devices_added[deviceCount] = new_devices_to_add[i]; // Add to devices array
          deviceCount++;

          // Remove the device from the new_devices_to_add array
          for (int j = i; j < newDeviceCount - 1; j++) {
            new_devices_to_add[j] = new_devices_to_add[j + 1];
          }
          newDeviceCount--;

          saveDevicesToPreferences(devices_added, deviceCount);

          clearRAWTPMSDeviceArray(new_devices_to_add, MAX_DEVICES);

          // Redraw the UI to reflect the changes
          drawUI();
          lastPressTime = millis();
          break; // Exit the loop after handling the button press
        }
      }
    }
    if(current_page == REMOVE){
      // Check if any new device button is pressed
      Serial.println("deleting buttons");
      for (int i = 0; i < deviceCount; i++) {
        if (remove_devices_buttons[i].contains(p.x, p.y)) {
          remove_devices_buttons[i].press(true);
        } else {
          remove_devices_buttons[i].press(false);
        }

        // If a new device button is pressed, move it to the devices array
        if (remove_devices_buttons[i].justPressed()) {
          Serial.println("button pressed for removing");
          Serial.println(i);

          // Remove the device from the new_devices_to_add array
          for (int j = i; j < deviceCount - 1; j++) {
            devices_added[j] = devices_added[j + 1];
          }
          deviceCount--;

          saveDevicesToPreferences(devices_added, deviceCount);

          // Redraw the UI to reflect the changes
          drawUI();
          lastPressTime = millis();
          break; // Exit the loop after handling the button press
        }
      }
    }
  }
  // Main loop is free to do other tasks
  // For example, you can add other non-blocking code here
  //TODO do i need this
  vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to prevent watchdog trigger
}


// Check if device name contains "TPMS"
bool isTPMSDevice(String name) {
  return name.indexOf("TPMS") >= 0;
  // return true;
}

// Check if device is new
bool isNewDevice(String name) {
  for (int i = 0; i < deviceCount; i++) {
    if (devices_added[i].name == name) return false;
  }
  for (int i = 0; i < newDeviceCount; i++) {
    if (new_devices_to_add[i].name == name) return false;
  }
  return true;
}

// Check if device is new
int getDeviceIndex(String name) {
  for (int i = 0; i < deviceCount; i++) {
    if (devices_added[i].name == name) return i;
  }
  return -1;
}


void draw_add_new_ui() {
  temps_button.drawButton();
  tft.setCursor(50, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("Adding more sensors");

  // BLEScanResults* foundDevices = pBLEScan->start(5, false);
  // bool foundNewDevice = false;

  // for (int i = 0; i < new_devices_to_add.length(); i++) {
  //   BLEAdvertisedDevice d = foundDevices->getDevice(i);
  //   String deviceName = d.getName().length() > 0 ? d.getName().c_str() : "Unknown Device";
  //   String data = d.getManufacturerData();
  //   Serial.println("d.getManufacturerData()");
  //   Serial.println(d.getManufacturerData());
  //   Serial.println(d.getManufacturerData().c_str());

  //   uint8_t* manufacturerDataBytes = (uint8_t*)data.c_str();  // Convert to byte array
  //   size_t manufacturerDataLength = data.length();            // Get length of Manufacturer Data
  //   Serial.print("Manufacturer Data (raw, hex): ");
  //   Serial.println(byteToHexString(manufacturerDataBytes, manufacturerDataLength));

    
  //   //if a device is found add it to the new_devices array 
  //   if (isTPMSDevice(deviceName) && isNewDevice(deviceName) && newDeviceCount < MAX_DEVICES) {
  //     new_devices_to_add[newDeviceCount].name = deviceName;
  //     new_devices_to_add[newDeviceCount].data = byteToHexString(manufacturerDataBytes, manufacturerDataLength);
  //     newDeviceCount++;
  //     foundNewDevice = true;
  //   }
  // }

  for (int i = 0; i < newDeviceCount; i++) {
    if (new_devices_to_add[i].name.length() > 0) {
      new_device_buttons[i].initButton(&tft, 100, 50 + i * 30, 150, 25, TFT_BLACK, TFT_BLUE, TFT_WHITE, const_cast<char*>(new_devices_to_add[i].name.c_str()), 1);
      new_device_buttons[i].drawButton();
    }
  }
}

void draw_temps(){
  add_new_sensor_button.drawButton();
  config_button.drawButton();
  remove_button.drawButton();
  tft.setCursor(50, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("drawing temops");

  // //update current devices
  // BLEScanResults* foundDevices = pBLEScan->start(5, false);
  // bool foundNewDevice = false;

  // for (int i = 0; i < foundDevices->getCount(); i++) {
  //   BLEAdvertisedDevice d = foundDevices->getDevice(i);
  //   String deviceName = d.getName().length() > 0 ? d.getName().c_str() : "Unknown Device";
  //   String data = d.getManufacturerData();

  //   uint8_t* manufacturerDataBytes = (uint8_t*)data.c_str();  // Convert to byte array
  //   size_t manufacturerDataLength = data.length();            // Get length of Manufacturer Data
  //   Serial.print("Manufacturer Data (raw, hex): ");
  //   Serial.println(byteToHexString(manufacturerDataBytes, manufacturerDataLength));

    
  //   //if a device is found add it to the new_devices array 
  //   int index = getDeviceIndex(deviceName);
  //   if (index != -1) {
  //     //update the device
  //     devices_added[index].data = byteToHexString(manufacturerDataBytes, manufacturerDataLength);
  //   }
  // }


  //display data
  for (int i = 0; i < deviceCount; i++) {
    tft.setCursor(50, (i+2)*20);
    tft.print(devices_added[i].name);

    Serial.println("devices_added[i].name");
    Serial.println(devices_added[i].name);
    Serial.println(devices_added[i].data);

    // Convert manufacturer data to byte array
    // uint8_t byteArray1[256] = {0}; // Adjust size as needed
    // hexStringToByteArray(devices_added[i].data, byteArray1, 256);
    // // Call the function to extract sensor data
    // TyreMessage extracted_data = extract_sensor_number(byteArray1, devices_added[i].name);
    
    String instring=retmanData(devices_added[i].data.c_str(), 0); 
    Serial.println(instring);
    // Serial.print("Device found: ");
    // Serial.println(Device.getRSSI());
    // Tire Temperature in C°
    Serial.print("Temperature: ");
    Serial.print(returnData(instring,12)/100.0);
    Serial.println("C°");
    // Tire pressure in Kpa           
    Serial.print("Pressure:    ");
    Serial.print(returnData(instring,8)/1000.0);
    Serial.println("Kpa");
    // Tire pressure in Bar           
    Serial.print("Pressure:    ");
    Serial.print(returnData(instring,8)/100000.0);
    Serial.println("bar");
    Serial.print("Pressure:    ");
    Serial.print(returnData(instring, 8) / 1000.0 * 0.1450377377); // Convert kPa to PSI
    Serial.println(" PSI");
    // Battery percentage             
    Serial.print("Battery:     ");
    Serial.print(returnBatt(instring));
    Serial.println("%");
    if (returnAlarm(instring)) {
      Serial.println("ALARM!");
    }
    Serial.println("");

    tft.setCursor(200, (i+2)*20);
    tft.print(returnData(instring,12)/100.0);
    tft.setCursor(240, (i+3)*20);
    tft.print(returnData(instring, 8) / 1000.0 * 0.1450377377);
  }

  // delay(2000);
}

void draw_remove_page() {
  temps_button.drawButton();
  tft.setCursor(50, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("Removing sensors");

  for (int i = 0; i < deviceCount; i++) {
    if (devices_added[i].name.length() > 0) {
      remove_devices_buttons[i].initButton(&tft, 100, 50 + i * 30, 150, 25, TFT_BLACK, TFT_BLUE, TFT_WHITE, const_cast<char*>(devices_added[i].name.c_str()), 1);
      remove_devices_buttons[i].drawButton();
    }
  }
}


// // Function to convert bytes to hex format
// String byteToHexString(uint8_t* data, size_t length) {
//   String hexStr = "";
//   for (size_t i = 0; i < length; i++) {
//     if (data[i] < 0x10) hexStr += "0"; // Add leading 0 for values < 16
//     hexStr += String(data[i], HEX);
//   }
//   return hexStr;
// }

// // Function to convert a hex String to a uint8_t array
// void hexStringToByteArray(String hexString, uint8_t* byteArray, int maxLength) {
//   int length = hexString.length();
//   if (length > maxLength * 2) {
//     length = maxLength * 2; // Ensure we don't exceed the byte array size
//   }

//   for (int i = 0; i < length; i += 2) {
//     String byteString = hexString.substring(i, i + 2);
//     byteArray[i / 2] = (uint8_t)strtol(byteString.c_str(), NULL, 16);
//   }
// }

// // Function to extract sensor data
// TyreMessage extract_sensor_number(uint8_t x[], String name) { 

//   // Extract PSI value
//   Serial.println("here123");
//   for (int i = 0; i < sizeof(x)/sizeof(uint8_t); i += 1) {
//     Serial.print(x[i]);
//   }
//   Serial.println("");

//   Serial.print("Raw Data: ");
//   for (int i = 0; i < 16; i++) {
//     Serial.print(x[i], HEX);
//     Serial.print(" ");
//   }
//   Serial.println();

//   // float psi = (x[6] | (x[7] << 8) | (x[8] << 16) | (x[9] << 24)) * 0.000145038;
//   // Serial.print("PSI: ");
//   // Serial.println(psi);

//   // // Extract temperature value
//   // float temp = (x[10] | (x[11] << 8)) / 100.0;
//   // Serial.print("Temp: ");
//   // Serial.println(temp);

//   // // Extract battery level
//   // int battery = x[14];
//   // Serial.print("Battery: ");
//   // Serial.println(battery);

//   // // Extract warning flag
//   // int warn = x[15];
//   // Serial.print("Warning: ");
//   // Serial.println(warn);

//   // // Extract sensor ID
//   // char sensor_id[10];
//   // sprintf(sensor_id, "%d%d%d", x[3], x[4], x[5]);
//   // Serial.print("Sensor ID: ");
//   // Serial.println(sensor_id);

//   // Extract sensor number from the name (e.g., "TPMS1" → 1)
//   int number = name.substring(4).toInt(); // Assumes name is in the format "TPMSX"

//   // Create a TyreMessage object
//   TyreMessage message = {
//     // number,       // Sensor number
//     // name,         // Sensor name
//     // psi,          // PSI value
//     // temp,         // Temperature value
//     // battery,      // Battery level
//     // warn,         // Warning flag
//     // sensor_id     // Sensor ID
//   };

//   // Send the message (you need to implement this function)
//   // display_status(message);
//   return message;
// }

// // Function to send the TyreMessage (you need to implement this)
// void display_status(TyreMessage message) {
//   // Example: Print the message to Serial Monitor
//   Serial.println("Sending Tyre Message:");
//   Serial.print("Number: ");
//   Serial.println(message.number);
//   Serial.print("Name: ");
//   Serial.println(message.name);
//   Serial.print("PSI: ");
//   Serial.println(message.psi);
//   Serial.print("Temp: ");
//   Serial.println(message.temp);
//   Serial.print("Battery: ");
//   Serial.println(message.battery);
//   Serial.print("Warning: ");
//   Serial.println(message.warning);
//   Serial.print("Sensor ID: ");
//   Serial.println(message.sensor_id);
//   Serial.println();
// }


// FUNCTIONS 

String retmanData(String txt, int shift) {
  // Return only manufacturer data string
  int start=txt.indexOf("data: ")+6+shift;
  return txt.substring(start,start+(36-shift));  
}

byte retByte(String Data,int start) {
  // Return a single byte from string
  int sp=(start)*2;
  char *ptr;
  return strtoul(Data.substring(sp,sp+2).c_str(),&ptr, 16);
}

long returnData(String Data,int start) {
  // Return a long value with little endian conversion
  return retByte(Data,start)|retByte(Data,start+1)<<8|retByte(Data,start+2)<<16|retByte(Data,start+3)<<24;
}

int returnBatt(String Data) {
  // Return battery percentage
  return retByte(Data,16);
}

int returnAlarm(String Data) {
  // Return battery percentage
  return retByte(Data,17);
}