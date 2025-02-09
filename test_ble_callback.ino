#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPI.h>
#include <XPT2046_Bitbang.h>
#include <TFT_eSPI.h>
#include <Preferences.h> // Non-volatile storages

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define MAX_DEVICES 20

XPT2046_Bitbang ts(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS);
TFT_eSPI tft = TFT_eSPI();
TFT_eSPI_Button add_new_sensor_button; // Create the toggle button
TFT_eSPI_Button temps_button;          // Create the toggle button
TFT_eSPI_Button config_button;         // Create the toggle button
TFT_eSPI_Button remove_button;         // Goes to the remove page

struct RAWTPMSDevice
{
  String name; // Device name
  String data; // Additional data (e.g., sensor data)
};

enum page
{
  CONFIG,
  ADD_NEW,
  TEMPS,
  REMOVE,
  TYRE_SET
};
page current_page = TEMPS;

unsigned long lastPressTime = 0;
Preferences preferences;


// Define a struct to represent the TyreMessage
struct TyreMessage
{
  String name; // Sensor name
  float psi;   // PSI value
  float temp;  // Temperature value
  int battery; // Battery level
  int warning; // Warning flag
};


// Bluetooth Scanner Variables
BLEScan *pBLEScan;
#define MAX_DEVICES 20
int scanTime = 5;                            // In seconds
TyreMessage devices_added[MAX_DEVICES];      // Store TPMS device names
TyreMessage new_devices_to_add[MAX_DEVICES]; // Store TPMS device name
int newDeviceCount = 0;
int deviceCount = 0;
int scrollPos = 0;
bool data_updated = false;
bool found_more_devices = false;

// Array to store buttons for new devices
TFT_eSPI_Button new_device_buttons[MAX_DEVICES];

// Array to remove buttons for devices
TFT_eSPI_Button remove_devices_buttons[MAX_DEVICES];


// Array to set sensor that will be set to tyre
TFT_eSPI_Button sensor_set_buttons[MAX_DEVICES];

// Array to set tyre for the sesonor selected
TFT_eSPI_Button tyre_set_buttons[MAX_DEVICES];

TyreMessage current_selected_for_setting;



TyreMessage LeftFront;
TyreMessage RightFront;
TyreMessage LeftRear;
TyreMessage RightRear;

TFT_eSPI_Button LeftFrontButton;
TFT_eSPI_Button RightFrontButton;
TFT_eSPI_Button LeftRearButton;
TFT_eSPI_Button RightRearButton;

const int buzzerPin = 22; // the buzzer pin
bool play_buzzer_check = false;

void getDevicesFromPreferences()
{
  // Load saved devices
  preferences.begin("tpms_data", false);
  deviceCount = preferences.getInt("deviceCount", 0);
  String prefix = "";  
  // Load each device's fields
  for (int i = 0; i < deviceCount; i++)
  {
    prefix = "device" + String(i);                                                // Unique key prefix for each device
    devices_added[i].name = preferences.getString((prefix + "_name").c_str(), "");       // Load name
    devices_added[i].psi = preferences.getFloat((prefix + "_psi").c_str(), 0);         // Load psi
    devices_added[i].temp = preferences.getFloat((prefix + "_temp").c_str(), 0);       // Load temp
    devices_added[i].battery = preferences.getInt((prefix + "_battery").c_str(), 0); // Load battery
    devices_added[i].warning = preferences.getInt((prefix + "_warning").c_str(), 0); // Load warning
  }
  
  Serial.println("Loading tyres");
  Serial.println("preferences.getStrin");
  Serial.println(preferences.getString((prefix + "_name").c_str(), ""));

  //get left front one
  prefix = "LF";                                                // Unique key prefix for each device
  LeftFront.name = preferences.getString((prefix + "_name").c_str(), "");       // Load name
  LeftFront.psi = preferences.getFloat((prefix + "_psi").c_str(), 0);         // Load psi
  LeftFront.temp = preferences.getFloat((prefix + "_temp").c_str(), 0);       // Load temp
  LeftFront.battery = preferences.getInt((prefix + "_battery").c_str(), 0); // Load battery
  LeftFront.warning = preferences.getInt((prefix + "_warning").c_str(), 0); // Load warning

  Serial.println("LeftFront.name");
  Serial.println(LeftFront.name);
  Serial.println(LeftFront.psi);
  //get left rear one
  prefix = "LR";                                                // Unique key prefix for each device
  LeftRear.name = preferences.getString((prefix + "_name").c_str(), "");       // Load name
  LeftRear.psi = preferences.getFloat((prefix + "_psi").c_str(), 0);         // Load psi
  LeftRear.temp = preferences.getFloat((prefix + "_temp").c_str(), 0);       // Load temp
  LeftRear.battery = preferences.getInt((prefix + "_battery").c_str(), 0); // Load battery
  LeftRear.warning = preferences.getInt((prefix + "_warning").c_str(), 0); // Load warning

  //get right front one
  prefix = "RF";                                                // Unique key prefix for each device
  RightFront.name = preferences.getString((prefix + "_name").c_str(), "");       // Load name
  RightFront.psi = preferences.getFloat((prefix + "_psi").c_str(), 0);         // Load psi
  RightFront.temp = preferences.getFloat((prefix + "_temp").c_str(), 0);       // Load temp
  RightFront.battery = preferences.getInt((prefix + "_battery").c_str(), 0); // Load battery
  RightFront.warning = preferences.getInt((prefix + "_warning").c_str(), 0); // Load warning

  //get right rear one
  prefix = "RR";                                                // Unique key prefix for each device
  RightRear.name = preferences.getString((prefix + "_name").c_str(), "");       // Load name
  RightRear.psi = preferences.getFloat((prefix + "_psi").c_str(), 0);         // Load psi
  RightRear.temp = preferences.getFloat((prefix + "_temp").c_str(), 0);       // Load temp
  RightRear.battery = preferences.getInt((prefix + "_battery").c_str(), 0); // Load battery
  RightRear.warning = preferences.getInt((prefix + "_warning").c_str(), 0); // Load warning

  preferences.end();
}


void saveDevicesToPreferences(TyreMessage devices[], int size)
{
  preferences.begin("tpms_data", false); // Open preferences in read/write mode

  // Save the number of devices
  preferences.putInt("deviceCount", size);

  // Save each device's fields
  for (int i = 0; i < size; i++)
  {
    String prefix = "device" + String(i);                                     // Unique key prefix for each device
    preferences.putString((prefix + "_name").c_str(), devices[i].name);       // Save name
    preferences.putFloat((prefix + "_psi").c_str(), devices[i].psi);         // Save psi
    preferences.putFloat((prefix + "_temp").c_str(), devices[i].temp);       // Save temp
    preferences.putInt((prefix + "_battery").c_str(), devices[i].battery); // Save battery
    preferences.putInt((prefix + "_warning").c_str(), devices[i].warning); // Save warning
  }

  preferences.end(); // Close preferences
}

void saveTyreToPreferences(TyreMessage device, String location)
{
  Serial.println("saving tyres");
  preferences.begin("tpms_data", false); // Open preferences in read/write mode

  Serial.println(location);
  if(location == "LF"){
    String prefix = "LF";                                   // Unique key prefix for each device
    preferences.putString((prefix + "_name").c_str(), device.name);       // Save name
    preferences.putFloat((prefix + "_psi").c_str(), device.psi);         // Save psi
    preferences.putFloat((prefix + "_temp").c_str(), device.temp);       // Save temp
    preferences.putInt((prefix + "_battery").c_str(), device.battery); // Save battery
    preferences.putInt((prefix + "_warning").c_str(), device.warning); // Save warning
    
  }
  if(location == "LR"){
    String prefix = "LR";                                   // Unique key prefix for each device
    preferences.putString((prefix + "_name").c_str(), device.name);       // Save name
    preferences.putFloat((prefix + "_psi").c_str(), device.psi);         // Save psi
    preferences.putFloat((prefix + "_temp").c_str(), device.temp);       // Save temp
    preferences.putInt((prefix + "_battery").c_str(), device.battery); // Save battery
    preferences.putInt((prefix + "_warning").c_str(), device.warning); // Save warning
  }
  if(location == "RF"){
    String prefix = "RF";                                   // Unique key prefix for each device
    preferences.putString((prefix + "_name").c_str(), device.name);       // Save name
    preferences.putFloat((prefix + "_psi").c_str(), device.psi);         // Save psi
    preferences.putFloat((prefix + "_temp").c_str(), device.temp);       // Save temp
    preferences.putInt((prefix + "_battery").c_str(), device.battery); // Save battery
    preferences.putInt((prefix + "_warning").c_str(), device.warning); // Save warning
  }
  if(location == "RR"){
    String prefix = "RR";                                   // Unique key prefix for each device
    preferences.putString((prefix + "_name").c_str(), device.name);       // Save name
    preferences.putFloat((prefix + "_psi").c_str(), device.psi);         // Save psi
    preferences.putFloat((prefix + "_temp").c_str(), device.temp);       // Save temp
    preferences.putInt((prefix + "_battery").c_str(), device.battery); // Save battery
    preferences.putInt((prefix + "_warning").c_str(), device.warning); // Save warning
  }
  
  preferences.end(); // Close preferences
}

bool hasDataChanged(TyreMessage a, TyreMessage b)
{
  if (a.name != b.name)
  {
    return true;
  }
  if (a.battery != b.battery)
  {
    return true;
  }
  if (a.psi != b.psi)
  {
    return true;
  }
  if (a.temp != b.temp)
  {
    return true;
  }
  if (a.warning != b.warning)
  {
    return true;
  }
  return false;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    // Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());

    String deviceName = advertisedDevice.getName().length() > 0 ? advertisedDevice.getName().c_str() : "Unknown Device";
    if (!isTPMSDevice(deviceName))
    {
      return;
    }
    String instring = retmanData(advertisedDevice.toString().c_str(), 0);

    if (isTPMSDevice(deviceName) && isNewDevice(deviceName) && newDeviceCount < MAX_DEVICES)
    {
      new_devices_to_add[newDeviceCount].name = deviceName;
      new_devices_to_add[newDeviceCount].psi = returnData(instring, 8) / 1000.0 * 0.1450377377;
      new_devices_to_add[newDeviceCount].temp = returnData(instring, 12) / 100.0;
      new_devices_to_add[newDeviceCount].battery = returnBatt(instring);
      new_devices_to_add[newDeviceCount].warning = returnAlarm(instring);

      newDeviceCount++;
      found_more_devices = true;
    }
    // int index = getDeviceIndex(deviceName);
    // if (index != -1)
    // {
    //   TyreMessage newData;
    //   newData.name = deviceName;
    //   newData.psi = returnData(instring, 8) / 1000.0 * 0.1450377377;
    //   newData.temp = returnData(instring, 12) / 100.0;
    //   newData.battery = returnBatt(instring);
    //   newData.warning = returnAlarm(instring);
    //   // if exsiting device then update the device data if it hasn't changed
    //   if (hasDataChanged(newData, devices_added[index]))
    //   {
    //     devices_added[index].psi = returnData(instring, 8) / 1000.0 * 0.1450377377;
    //     devices_added[index].temp = returnData(instring, 12) / 100.0;
    //     devices_added[index].battery = returnBatt(instring);
    //     devices_added[index].warning = returnAlarm(instring);
    //     data_updated = true;
    //   }
    // }
    //update the main display
    String update_sensor = getDevice(deviceName);
    if(update_sensor != ""){
      
      TyreMessage newData;
      newData.name = deviceName;
      newData.psi = returnData(instring, 8) / 1000.0 * 0.1450377377;
      newData.temp = returnData(instring, 12) / 100.0;
      newData.battery = returnBatt(instring);
      newData.warning = returnAlarm(instring);

      if(update_sensor == "LF"){
        if (hasDataChanged(newData, LeftFront)){
          LeftFront.psi = returnData(instring, 8) / 1000.0 * 0.1450377377;
          LeftFront.temp = returnData(instring, 12) / 100.0;
          LeftFront.battery = returnBatt(instring);
          LeftFront.warning = returnAlarm(instring);
          saveTyreToPreferences(LeftFront, "LF");
          data_updated = true;
        }
      }
      if(update_sensor == "LR"){
        if (hasDataChanged(newData, LeftRear)){
          LeftRear.psi = returnData(instring, 8) / 1000.0 * 0.1450377377;
          LeftRear.temp = returnData(instring, 12) / 100.0;
          LeftRear.battery = returnBatt(instring);
          LeftRear.warning = returnAlarm(instring);
          saveTyreToPreferences(LeftRear, "LR");
          data_updated = true;
        }
      }
      if(update_sensor == "RF"){
        if (hasDataChanged(newData, RightFront)){
          RightFront.psi = returnData(instring, 8) / 1000.0 * 0.1450377377;
          RightFront.temp = returnData(instring, 12) / 100.0;
          RightFront.battery = returnBatt(instring);
          RightFront.warning = returnAlarm(instring);
          saveTyreToPreferences(RightFront, "RF");
          data_updated = true;
        }
      }
      if(update_sensor == "RR"){
        if (hasDataChanged(newData, RightRear)){
          RightRear.psi = returnData(instring, 8) / 1000.0 * 0.1450377377;
          RightRear.temp = returnData(instring, 12) / 100.0;
          RightRear.battery = returnBatt(instring);
          RightRear.warning = returnAlarm(instring);
          saveTyreToPreferences(RightRear, "RR");
          data_updated = true;
        }
      }
    }
  }
};

void setup()
{
  Serial.begin(115200);
  pinMode(buzzerPin, OUTPUT); // Set as output

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
  pBLEScan->setWindow(99); // less or equal setInterval value

  // Create a task to handle the BLE scanning
  xTaskCreate(
      scanTask,   // Function to implement the task
      "ScanTask", // Name of the task
      10000,      // Stack size in words
      NULL,       // Task input parameter
      1,          // Priority of the task
      NULL        // Task handle
  );

  // Create a task to handle the BLE scanning
  xTaskCreate(
      play_buzzer,   // Function to implement the task
      "PlayBuzzer", // Name of the task
      10000,      // Stack size in words
      NULL,       // Task input parameter
      1,          // Priority of the task
      NULL        // Task handle
  );

  // uncomment to clear devices:

  // Load saved devices
  // preferences.begin("tpms_data", false);
  // preferences.clear();
  // preferences.end();

  getDevicesFromPreferences();

  // Button in the bottom-right corner (x = 270, y = 220)
  add_new_sensor_button.initButton(&tft, 270, 220, 80, 40, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Add", 1);
  config_button.initButton(&tft, 40, 220, 80, 40, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Config", 1);
  temps_button.initButton(&tft, 270, 220, 80, 40, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Temps", 1);
  remove_button.initButton(&tft, 150, 220, 80, 40, TFT_BLACK, TFT_RED, TFT_WHITE, "Remove", 1);
  Serial.println("init current_page");
  Serial.println(current_page);
  drawUI();
}

void play_buzzer(void *parameter){
  for (;;)
    {
      Serial.println("do the buz if need buzzing");
      if(play_buzzer_check){
        digitalWrite(buzzerPin, HIGH); // Set to HIGH to make the buzzer sound
        Serial.println("buzzing");
        vTaskDelay(pdMS_TO_TICKS(300));
        digitalWrite(buzzerPin, LOW); // LOW to turn off the buzzer
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    else{
        vTaskDelay(pdMS_TO_TICKS(3000)); //3 seconds delay to let other things run
    }
  }
}

void scanTask(void *parameter)
{
  for (;;)
  {
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

    pBLEScan->clearResults();        // delete results from BLEScan buffer to release memory
    vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for 2 seconds
  }
}

void drawUI()
{
  tft.fillScreen(TFT_BLACK);

  Serial.println("draw current_page");
  Serial.println(current_page);
  if (current_page == ADD_NEW)
  {
    draw_add_new_ui();
  }
  else if (current_page == TEMPS)
  {
    draw_temps();
  }
  else if (current_page == CONFIG)
  {
    draw_config_page();
  }
  else if (current_page == REMOVE)
  {
    draw_remove_page();
  }
  else if (current_page == TYRE_SET)
  {
    draw_tyre_set_page();
  }
  else
  {
    temps_button.drawButton();
    tft.setCursor(5, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.print("Something went wrong");
  }
}

// void clearRAWTPMSDeviceArray(RAWTPMSDevice arr[], int size)
// {
//   for (int i = 0; i < size; i++)
//   {
//     arr[i].name = ""; // Clear the name field
//     arr[i].data = ""; // Clear the data field
//   }
// }

void clearTyreMessageArray(TyreMessage arr[], int size)
{
  for (int i = 0; i < size; i++)
  {
    arr[i].name = "";   // Clear the name field
    arr[i].psi = 0;     // Clear the data field
    arr[i].temp = 0;    // Clear the data field
    arr[i].battery = 0; // Clear the data field
    arr[i].warning = 0; // Clear the data field
  }
}

void loop()
{
  TouchPoint p = ts.getTouch();

  if (current_page == TEMPS && data_updated == true)
  {
    drawUI();
    data_updated = false;
  }
  // update devices add page if found more
  if (current_page == ADD_NEW && found_more_devices == true)
  {
    drawUI();
    found_more_devices = false;
  }

  if (p.zRaw > 0 && millis() - lastPressTime > 500)
  { // Debounce press
    if (add_new_sensor_button.contains(p.x, p.y))
    {
      add_new_sensor_button.press(true);
    }
    else
    {
      add_new_sensor_button.press(false);
    }
    if (temps_button.contains(p.x, p.y))
    {
      temps_button.press(true);
    }
    else
    {
      temps_button.press(false);
    }
    if (config_button.contains(p.x, p.y))
    {
      config_button.press(true);
    }
    else
    {
      config_button.press(false);
    }
    if (remove_button.contains(p.x, p.y))
    {
      remove_button.press(true);
    }
    else
    {
      remove_button.press(false);
    }
    if (LeftFrontButton.contains(p.x, p.y))
    {
      LeftFrontButton.press(true);
    }
    else
    {
      LeftFrontButton.press(false);
    }
    if (LeftRearButton.contains(p.x, p.y))
    {
      LeftRearButton.press(true);
    }
    else
    {
      LeftRearButton.press(false);
    }
    if (RightRearButton.contains(p.x, p.y))
    {
      RightRearButton.press(true);
    }
    else
    {
      RightRearButton.press(false);
    }
    if (RightFrontButton.contains(p.x, p.y))
    {
      RightFrontButton.press(true);
    }
    else
    {
      RightFrontButton.press(false);
    }

    if (add_new_sensor_button.justPressed() && current_page != ADD_NEW)
    {
      Serial.println("chagning page add new");
      Serial.println(current_page);
      current_page = ADD_NEW;
      drawUI();
      lastPressTime = millis();
    }
    else if (temps_button.justPressed() && current_page != TEMPS)
    {
      Serial.println("chagning page temps");
      Serial.println(current_page);
      current_page = TEMPS;
      drawUI();
      lastPressTime = millis();
    }
    else if (config_button.justPressed() && current_page != CONFIG)
    {
      Serial.println("chagning page config");
      Serial.println(current_page);
      current_page = CONFIG;
      drawUI();
      lastPressTime = millis();
    }
    else if (remove_button.justPressed() && current_page != REMOVE)
    {
      Serial.println("chagning page remove");
      Serial.println(current_page);
      current_page = REMOVE;
      drawUI();
      lastPressTime = millis();
    }

    if (current_page == ADD_NEW)
    {
      // Check if any new device button is pressed
      Serial.println("newDeviceCount");
      Serial.println(newDeviceCount);
      for (int i = 0; i < newDeviceCount; i++)
      {
        Serial.println("checking buttons");
        if (new_device_buttons[i].contains(p.x, p.y))
        {
          new_device_buttons[i].press(true);
        }
        else
        {
          new_device_buttons[i].press(false);
        }

        // If a new device button is pressed, move it to the devices array
        if (new_device_buttons[i].justPressed())
        {
          Serial.println("button pressed for adding");
          Serial.println(i);
          devices_added[deviceCount] = new_devices_to_add[i]; // Add to devices array
          deviceCount++;

          // Remove the device from the new_devices_to_add array
          for (int j = i; j < newDeviceCount - 1; j++)
          {
            new_devices_to_add[j] = new_devices_to_add[j + 1];
          }
          newDeviceCount--;

          saveDevicesToPreferences(devices_added, deviceCount);

          clearTyreMessageArray(new_devices_to_add, MAX_DEVICES);

          // Redraw the UI to reflect the changes
          drawUI();
          lastPressTime = millis();
          break; // Exit the loop after handling the button press
        }
      }
    }
    if (current_page == REMOVE)
    {
      // Check if any new device button is pressed
      Serial.println("deleting buttons");
      for (int i = 0; i < deviceCount; i++)
      {
        if (remove_devices_buttons[i].contains(p.x, p.y))
        {
          remove_devices_buttons[i].press(true);
        }
        else
        {
          remove_devices_buttons[i].press(false);
        }

        // If a new device button is pressed, move it to the devices array
        if (remove_devices_buttons[i].justPressed())
        {
          Serial.println("button pressed for removing");
          Serial.println(i);

          // Remove the device from the new_devices_to_add array
          for (int j = i; j < deviceCount - 1; j++)
          {
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
    if (current_page == TYRE_SET)
    {
      // Check if any new device button is pressed
      Serial.println("Setting sensor to tyre");

      
      if (LeftFrontButton.justPressed())
      {
        LeftFront = current_selected_for_setting;
        saveTyreToPreferences(LeftFront, "LF");
      }
      else if (LeftRearButton.justPressed())
      {
        LeftRear = current_selected_for_setting;
        saveTyreToPreferences(LeftRear, "LR");
      }
      else if (RightFrontButton.justPressed())
      {
        RightFront = current_selected_for_setting;
        saveTyreToPreferences(RightFront, "RF");
      }
      else if (RightRearButton.justPressed())
      {
        RightRear = current_selected_for_setting;
        saveTyreToPreferences(RightRear, "RR");
      }
      else {
        Serial.print("something went wrong setting the sensor");
      }
      
      current_page = TEMPS;
      drawUI();
      lastPressTime = millis();
    }
     if (current_page == CONFIG)
    {
      // Check if any new device button is pressed
      Serial.println("config buttons");
      for (int i = 0; i < deviceCount; i++)
      {
        if (sensor_set_buttons[i].contains(p.x, p.y))
        {
          sensor_set_buttons[i].press(true);
        }
        else
        {
          sensor_set_buttons[i].press(false);
        }

        // If a set sensors button is pressed, set the device and move on to the next
        if (sensor_set_buttons[i].justPressed())
        {
          Serial.println("button pressed for removing");
          Serial.println(i);

          current_selected_for_setting = devices_added[i];

          current_page = TYRE_SET;
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
  // TODO do i need this
  // vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to prevent watchdog trigger
  delay(50);
}

// Check if device name contains "TPMS"
bool isTPMSDevice(String name)
{
  return name.indexOf("TPMS") >= 0;
  // return true;
}

// Check if device is new
bool isNewDevice(String name)
{
  for (int i = 0; i < deviceCount; i++)
  {
    if (devices_added[i].name == name)
      return false;
  }
  for (int i = 0; i < newDeviceCount; i++)
  {
    if (new_devices_to_add[i].name == name)
      return false;
  }
  return true;
}

// Check if device is new
int getDeviceIndex(String name)
{
  for (int i = 0; i < deviceCount; i++)
  {
    if (devices_added[i].name == name)
      return i;
  }
  return -1;
}
// Check if device is new
String getDevice(String name)
{
  if(name == LeftFront.name){
    return "LF";
  }
  if(name == LeftRear.name){
    return "LR";
  }
  if(name == RightFront.name){
    return "RF";
  }
  if(name == RightRear.name){
    return "RR";
  }
 
  return "";
}

void draw_add_new_ui()
{
  temps_button.drawButton();
  tft.setCursor(50, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("Adding more sensors");


  for (int i = 0; i < newDeviceCount; i++)
  {
    if (new_devices_to_add[i].name.length() > 0)
    {
      new_device_buttons[i].initButton(&tft, 100, 50 + i * 30, 150, 25, TFT_BLACK, TFT_BLUE, TFT_WHITE, const_cast<char *>(new_devices_to_add[i].name.c_str()), 1);
      new_device_buttons[i].drawButton();
    }
  }
}


void draw_temps()
{
  add_new_sensor_button.drawButton();
  config_button.drawButton();
  // remove_button.drawButton();
  tft.setCursor(50, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("Da tyre display");


  // display data
  // for (int i = 0; i < deviceCount; i++)
  // {
  //   tft.setCursor(50, (i + 2) * 20);
  //   tft.print(devices_added[i].name);

  //   Serial.println("devices_added[i].name");
  //   Serial.println(devices_added[i].name);

  //   tft.setCursor(200, (i + 2) * 20);
  //   tft.print(devices_added[i].psi);
  //   tft.setCursor(240, (i + 3) * 20);
  //   tft.print(devices_added[i].temp);
  // }

  draw_tyre_section(50, 40, LeftFront);
  
  draw_tyre_section(150, 40, RightFront);
  
  draw_tyre_section(50, 140, LeftRear);

  draw_tyre_section(150, 140, RightRear);
}

void draw_tyre_section(int start_x, int start_y, TyreMessage tyre_message)
{

  tft.setCursor(start_x, start_y);
  if (tyre_message.name != ""){
    //if warning set the text to RED for psi
    if (tyre_message.warning == 1){
      tft.setTextColor(TFT_RED);
      play_buzzer_check = true;
    }
    else{
      play_buzzer_check = false;
      // play_buzzer_check = true;
    }
    Serial.println(play_buzzer_check);
    tft.print(tyre_message.psi);
    tft.setTextColor(TFT_WHITE);
    start_y += 20;
    tft.setCursor(start_x, start_y);
    tft.print(tyre_message.temp);

    
    //if low battery show it
    if(tyre_message.battery <= 10){
      start_y += 20;
      tft.setCursor(start_x, start_y);
      tft.print("Low baterry");
    }
  }
  else{
    tft.print("--");
  }
}
void draw_remove_page()
{
  temps_button.drawButton();
  tft.setCursor(50, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("Removing sensors");

  for (int i = 0; i < deviceCount; i++)
  {
    if (devices_added[i].name.length() > 0)
    {
      remove_devices_buttons[i].initButton(&tft, 100, 50 + i * 30, 150, 25, TFT_BLACK, TFT_BLUE, TFT_WHITE, const_cast<char *>(devices_added[i].name.c_str()), 1);
      remove_devices_buttons[i].drawButton();
    }
  }
}

void draw_config_page()
{
  temps_button.drawButton();
  tft.setCursor(50, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("Select sensor to set");


  //press sensor added

  for (int i = 0; i < deviceCount; i++)
  {
    if (devices_added[i].name.length() > 0)
    {
      sensor_set_buttons[i].initButton(&tft, 100, 50 + i * 30, 150, 25, TFT_BLACK, TFT_BLUE, TFT_WHITE, const_cast<char *>(devices_added[i].name.c_str()), 1);
      sensor_set_buttons[i].drawButton();
    }
  }

  // goes to next page where you select the tyre to asign it too?
}

void draw_tyre_set_page()
{
  temps_button.drawButton();
  tft.setCursor(50, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("Select tyre to set");

  //draw select buttons
  LeftFrontButton.initButton(&tft, 100, 50, 150, 25, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Left Front", 2);
  LeftFrontButton.drawButton();
  
  RightFrontButton.initButton(&tft, 100, 50 + 1 * 30, 150, 25, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Right Front", 2);
  RightFrontButton.drawButton();
  
  LeftRearButton.initButton(&tft, 100, 50 + 2 * 30, 150, 25, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Left Rear", 2);
  LeftRearButton.drawButton();

  RightRearButton.initButton(&tft, 100, 50 + 3 * 30, 150, 25, TFT_BLACK, TFT_BLUE, TFT_WHITE, "Right Rear", 2);
  RightRearButton.drawButton();
  
}
// FUNCTIONS

String retmanData(String txt, int shift)
{
  // Return only manufacturer data string
  int start = txt.indexOf("data: ") + 6 + shift;
  return txt.substring(start, start + (36 - shift));
}

byte retByte(String Data, int start)
{
  // Return a single byte from string
  int sp = (start) * 2;
  char *ptr;
  return strtoul(Data.substring(sp, sp + 2).c_str(), &ptr, 16);
}

long returnData(String Data, int start)
{
  // Return a long value with little endian conversion
  return retByte(Data, start) | retByte(Data, start + 1) << 8 | retByte(Data, start + 2) << 16 | retByte(Data, start + 3) << 24;
}

int returnBatt(String Data)
{
  // Return battery percentage
  return retByte(Data, 16);
}

int returnAlarm(String Data)
{
  // Return battery percentage
  return retByte(Data, 17);
}