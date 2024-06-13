#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/TomThumb.h>
#include <Encoder.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

#define INDEX_VALUE_PIN A4
#define PIN_UP_CMD 10
#define PIN_DOWN_CMD 9

#define ERR_MSG_VALUE -1

#define ISO_VALUE_ROW  0
#define APERTURE_VALUE_ROW 1
#define SS_VALUE_ROW  2
#define MSG_VALUE_ROW  3

#define ENCODER_PIN_1 6
#define ENCODER_PIN_2 7


enum SelectedItem {
  ISO,
  APERTURE,
  ShutterSpeed
};

enum RotationMode {
  PORTRAIT,
  ALBUM
};

enum Command {
  CMD_UP,
  CMD_DOWN,
  CMD_NEXT,
  CMD_PREVIOUS,
  CMD_UNKNOWN
};



Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

int isoRange[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100, 150, 200, 250, 300, 350 , 400, 500, 800}; 

float appertureMin = 1.8f;
float appertureMax  = 16.f; 
float apperturreStep = 0.1f;
      
float shutterValues[] = {1/2000.f, 1/1000.f, 1/500.f, 1/250.f, 1/125.f, 1/60.f, 1/30.f, 1, 2 };
char *shutterDisplayValues[] = {"2000", "1000", "500", "250", "60", "1", "2"};


int trianglePos[][6] = {{25, 5 , 32, 0, 32 ,10}, 
                        {25, 5 + 35, 32, 0+35, 32, 10 + 35},
                        {25, 5 + 73, 32, 0+73, 32, 10 + 73}
                        };//represents 3 points (x,y) of each triangle for each selection;
int textRowPosition[][2] = {{0,0}, {0,0}, {0,0}, {0,0}};
enum RotationMode mode = PORTRAIT;
enum SelectedItem param[] = {ISO, APERTURE, ShutterSpeed};

int currentParamForModifyIndex = 0;
int isoIndex = 19;
int shutterItemIndex = 1;    

float currentApperture = appertureMin;
float currentLux = 100000.f;
float prvsLux = 0.f;

bool firstTime = false;
bool btnUpSwitch = false;
bool btnDownSwitch = false;

bool btnUpState = LOW;
bool btnDownState = LOW;


// ev = log 2 ( (aperture^2) / t )
// ev2 = log 2 (lux*ISO/K)
// => t = ((aperture^2) * K)/(lux*ISO)
float K = 12.50f; // Constant


Encoder encoder(ENCODER_PIN_1, ENCODER_PIN_2);
enum Command encoderCmd = CMD_UNKNOWN;
long oldPosition  = -999; 

void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");  
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}

void displayLightMeterLog(){
  Serial.print("Current Lux: ");
  Serial.println(currentLux, DEC);
  Serial.print("ISO: ");
  Serial.println(isoRange[isoIndex], DEC);
  Serial.print("Apperture: ");
  Serial.print(currentApperture, 2);
  Serial.println("f.");
  Serial.print("Shutter speed:");
  Serial.println(shutterDisplayValues[shutterItemIndex]);
  Serial.println("------------------------------------");
  Serial.println("");
}

void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("13 ms");
  Serial.println("------------------------------------");
}


/**************************************************************************/
/*
    Arduino setup function (automatically called at startup)
*/
/**************************************************************************/



void displayDataAlbum(){
  display.println();     
  display.setTextSize(2);      
  display.print(F("ISO     "));
  display.print(F("AP    "));
  display.print(F("SS "));
  display.println("");
  display.setCursor(0,30); 
  display.setTextSize(3);
  display.print(isoRange[isoIndex], DEC);
  display.print(" ");
  display.print(currentApperture, 1);
  display.print(" ");
  display.print(shutterDisplayValues[shutterItemIndex]);
}

void displayDataPortrait(){
  display.setCursor(0, 0);
  display.setTextWrap(false);
  display.setTextSize(2);
  
  display.println();
  display.println(F("ISO "));
  textRowPosition[ISO_VALUE_ROW][0] =  display.getCursorX();
  textRowPosition[ISO_VALUE_ROW][1] =  display.getCursorY();
  display.println(isoRange[isoIndex], DEC);

  display.println(" ");
  display.setTextSize(2);
  display.println(F("AP "));
  textRowPosition[APERTURE_VALUE_ROW][0] =  display.getCursorX();
  textRowPosition[APERTURE_VALUE_ROW][1] =  display.getCursorY();
  display.println(currentApperture,2);

  display.println(" ");
  display.println(F("SS "));  
  display.setTextSize(2);
  textRowPosition[SS_VALUE_ROW][0] =  display.getCursorX();
  textRowPosition[SS_VALUE_ROW][1] =  display.getCursorY();
  display.println(shutterDisplayValues[shutterItemIndex]);

  display.println();
  textRowPosition[MSG_VALUE_ROW][0] =  display.getCursorX();
  textRowPosition[MSG_VALUE_ROW][1] =  display.getCursorY();
  display.println(F("AUTO"));  
  display.display();
}

void encoderISR() {
  long newPosition = encoder.read();
  if(newPosition > oldPosition){
    encoderCmd = CMD_NEXT;
  } else if(newPosition < oldPosition){
    encoderCmd = CMD_PREVIOUS;
  } else {
    encoderCmd = CMD_UNKNOWN;
  }
  /*
  if (newPosition != oldPosition) {
    oldPosition = newPosition;
    display.clearDisplay();
    display.setCursor(0, 0);
    //display.print("Position: ");
    display.print(newPosition);
    redrawDecFloat(-1.f, "Pos:", 0);
    redrawDecFloat((float)oldPosition, "", 1);
    Serial.print("Posision");
    Serial.println(oldPosition);
    display.display();
  }
  */
}

void setup(void) 
{
  Serial.begin(9600);
  Serial.println("Light Sensor Test"); Serial.println("");
   delay(550);
  /* Initialise the sensor */
  //use tsl.begin() to default to Wire, 
  //tsl.begin(&Wire2) directs api to use Wire2, etc.
  
  /*
  if(!tsl.begin())
  {
    /* There was a problem detecting the TSL2561 ... check your connections */
    /*Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  */

   if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  /* Display some basic information on this sensor */
  //displaySensorDetails();
  
  /* Setup the sensor gain and integration time */
  //configureSensor();
  display.clearDisplay();
  display.display();
  display.setFont(&TomThumb);
  delay(2000); // Pause for 2 seconds
  display.clearDisplay();
  /* We're ready to go! */
   display.setRotation(1);
  Serial.println("");
  Serial.println("GO GO LIGHTMETER");
        // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);    // Start at top-left corner

 
  /*
 
  */
  displayDataPortrait();

  //display.print(" ");
 
  //display.print(" ");
  //display.fillScreen(1);
  //pinMode(INDEX_VALUE_PIN, INPUT);

  pinMode(ENCODER_PIN_1, INPUT_PULLUP);
  pinMode(ENCODER_PIN_2, INPUT_PULLUP);

  pinMode(PIN_UP_CMD, INPUT_PULLUP);
  pinMode(PIN_DOWN_CMD, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_1), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_2), encoderISR, CHANGE);

}

/**************************************************************************/
/*
    Arduino loop function, called once 'setup' is complete (your own code
    should go here)
*/
/**************************************************************************/
void loop(void) 
{  
  /* Get a new sensor event */ 
  sensors_event_t event;
  //tsl.getEvent(&event);
 
  /* Display the results (light is measured in lux) */
  if (event.light)
  {
  //  Serial.print(event.light); Serial.println(" lux");
  }
  else
  {
    /* If event.light = 0 lux the sensor is probably saturated
       and no reliable data could be generated! */
    Serial.println("Sensor overload");
  }

  /*
  checkNavButtons();
  operateNavButtonsComnmands();
  drawSelectionIcon(SSD1306_WHITE);
  display.display();
  for(int i = 0; i< 10; i++){
    readAnalogSelectorWheelData();
    display.display();
    delay(250);
    displayLightMeterLog();
    delay(250);
  }
  delay(50);
  prvsLux = currentLux;
  currentLux = random(5000, 25000);
  */
  checkNavButton();  
  operateNavButtonsComnmands();
  readEncoderSelectorWheelData();
}

void checkNavButton(){
  if(!btnUpSwitch){
    btnUpSwitch = digitalRead(PIN_UP_CMD);
  }
  if(!btnDownSwitch) {
    btnDownSwitch =  digitalRead(PIN_DOWN_CMD);
  }
}  

void checkNavButtonsForFixedButton() {
  if(firstTime){
    btnUpState = digitalRead(PIN_UP_CMD);
    btnDownState = digitalRead(PIN_DOWN_CMD);
    firstTime = false;
  }

  if (digitalRead(PIN_UP_CMD) != btnUpState) {
    btnUpState = !btnUpState;
    btnUpSwitch = true;
  }
  if (digitalRead(PIN_DOWN_CMD) != btnDownState) {
    btnDownState = !btnDownState;
    btnDownSwitch = true;
  }
}

void operateNavButtonsComnmands(){
 if(btnUpSwitch == true){
      drawSelectionIcon(0);
    currentParamForModifyIndex = (currentParamForModifyIndex + 1) % 3;
    btnUpSwitch = false;
  }
  if(btnDownSwitch == true){
    drawSelectionIcon(0);
    if (currentParamForModifyIndex > 0) {
      currentParamForModifyIndex--;
    } else {
      currentParamForModifyIndex = 2;
    }
    btnDownSwitch = false;
  }
}


void readEncoderSelectorWheelData(){
  bool newLux = currentLux != prvsLux;
    switch(param[currentParamForModifyIndex]){
    case ISO: 
      //reselectISO();
      if(reselectISO() || newLux) 
        updateInAutoMode(isoRange[isoIndex]);
    break;
    case APERTURE:
      Serial.println("UPDATE IN APERTURE MODE:");
        if(reselectAperture() || newLux) 
          updateInAppertureMode();
    break;
    case ShutterSpeed:
        Serial.println("UPDATE IN SHUTTER SPEED MODE:");
        if(reselectSS() || newLux) 
          updateInSSMode();
    break;
  }
  display.display();
}

void readAnalogSelectorWheelData(){
  int value =  analogRead(INDEX_VALUE_PIN);
  //int value =  random(0,1024);
  bool newLux = currentLux != prvsLux;
  switch(param[currentParamForModifyIndex]){
    case ISO: 
      reselectISO(value);
      //if(reselectISO(value) == false || newLux) 
      updateInAutoMode(isoRange[isoIndex]);
    break;
    case APERTURE:
      Serial.println("UPDATE IN APERTURE MODE:");
      reselectAperture(value);
      //if(reselectAperture(value) == false || newLux) 
      updateInAppertureMode();
    break;
    case ShutterSpeed:
    reselectSS(value);
    Serial.println("UPDATE IN SHUTTER SPEED MODE:");
      //if(reselectSS(value) == false || newLux) 
     updateInSSMode();
    break;
  }
  display.display();
}

bool reselectISO(){
  int length =  (sizeof(isoRange)/sizeof(*isoRange));
  switch (encoderCmd)
  {
  case CMD_NEXT:
    isoIndex  = (isoIndex + 1)%length; 
    break;
  case CMD_PREVIOUS:
    if(isoIndex - 1 < 0){
      isoIndex = length-1;
    } else {
      isoIndex--;
    }
    break;
  default:
    return false;
    break;
  }
  redrawISO();
  return true;
}

bool reselectISO(int rawValue){
  int length =  (sizeof(isoRange)/sizeof(*isoRange));
  int index = map(rawValue, 0, 1024, 0, length);
  Serial.print("ISO is: ");
  Serial.println(isoRange[index], DEC);
  if(index == isoIndex) return false;
  isoIndex = index;
  redrawISO();
  return true;
}



void redrawISO(){
  display.fillRect(textRowPosition[0][0], textRowPosition[0][1]-13, 40, 13, SSD1306_BLACK); //text line height  def near 6 pt so 12 
  display.setCursor(textRowPosition[0][0], textRowPosition[0][1]);
  display.println(isoRange[isoIndex], DEC);
}

void updateInAutoMode(int iso){
  redrawDecFloat(-1, "AUTO", MSG_VALUE_ROW);
  if(calcAuto(iso)){
    redrawDecFloat(currentApperture, "  ", 1);
    redrawDecFloat(-1, shutterDisplayValues[shutterItemIndex], 2);
    redrawDecFloat(-1, "AUTO", MSG_VALUE_ROW);
  } else{
    redrawDecFloat(currentApperture, "EE", 1);
    redrawDecFloat(-1, "EE", 2);
    Serial.println("FAILED TO CALC IN AUTO MDOE");
    redrawDecFloat(-1, "ERR", MSG_VALUE_ROW);
  }
  
}

bool reselectAperture(){
 switch (encoderCmd) {
  case CMD_NEXT:
    if(currentApperture < appertureMax){
      currentApperture += apperturreStep;
    } else {
      currentApperture = appertureMin;
    }
    break;
  case CMD_PREVIOUS:
   if(currentApperture > appertureMin){
      currentApperture -= apperturreStep;
    } else {
      currentApperture = appertureMax;
    }
    break;
  default:
    return false;
    break;
  }
  redrawAperture();
  return true;
}


bool reselectAperture(int rawValue){
  int newValue = map(rawValue , 0, 1024, appertureMin, appertureMax);
  if(newValue == currentApperture) return false;
  currentApperture = newValue;
  redrawAperture();
  return true;
}


void updateInAppertureMode(){
   redrawDecFloat(-1, "   ", MSG_VALUE_ROW);
  int arrLenght = sizeof(shutterValues)/sizeof(shutterValues[0]);
  float newSS = calcSS();
  float bestErr = INFINITY;
  int newIndex = -1;
  
  for(int i = 0; i < arrLenght; i++){
    float newErr = abs(shutterValues[i] - newSS);
    if(newErr < bestErr){
      bestErr = newErr;
      newIndex = i;
    } 
  }

  Serial.print("new found ss: ");
  Serial.print(newSS);
  Serial.print(" best err: ");
  Serial.print(bestErr);
  if(newIndex == -1){
       Serial.println("failed to found approximate value");  
  } else{
    Serial.print(" New value will be ");
    Serial.println(shutterValues[newIndex]);
  }
  if(newIndex != -1 && ((bestErr/4.0f) < shutterValues[newIndex]) ){
    shutterItemIndex = newIndex;
    redrawSS();
  } else if( newSS > 1.0f) {
    Serial.print("failed to found ss TO HIGH: ");
    Serial.println(newSS);
    redrawDecFloat(newSS, "sec.", 2);
    redrawDecFloat(-1, "HIGH", MSG_VALUE_ROW);
  } else {
     redrawDecFloat(-1, "LOW", MSG_VALUE_ROW);
    Serial.print("failed to found ss TO LOW: ");
    Serial.print(newSS*1000.f);
    Serial.println(" ms");
    redrawDecFloat(newSS*1000.0f, "ms.", 2);
  }
}

void redrawAperture(){
  display.fillRect(textRowPosition[1][0], textRowPosition[1][1]-13, 40, 13, SSD1306_BLACK);
  display.setCursor(textRowPosition[1][0], textRowPosition[1][1]);
  display.println(currentApperture, 2);
}

bool reselectSS(){
  int length =  (sizeof(shutterValues)/sizeof(*shutterValues));
  switch (encoderCmd)
  {
  case CMD_NEXT:
    shutterItemIndex  = (shutterItemIndex + 1)%length; 
    break;
  case CMD_PREVIOUS:
    if(isoIndex - 1 < 0){
      shutterItemIndex = length-1;
    } else {
      shutterItemIndex--;
    }
    break;
  default:
    return false;
    break;
  }
  redrawSS();
  return true;
}


bool reselectSS(int rawValue){
  int length =  (sizeof(shutterValues)/sizeof(*shutterValues));
  int index = map(rawValue, 0, 1024, 0, length);
  if(index == shutterItemIndex) return false;
  shutterItemIndex = index;
  redrawSS();
  return true;
}

void redrawSS(){
  display.fillRect(textRowPosition[2][0], textRowPosition[2][1]-13, 40, 13, SSD1306_BLACK);
  display.setCursor(textRowPosition[2][0], textRowPosition[2][1]);
  display.println(shutterDisplayValues[shutterItemIndex]);
}

void updateInSSMode(){
  float apperture = calcAperture();
  redrawDecFloat(-1, "    ", MSG_VALUE_ROW);
  if(apperture >= appertureMin && apperture <= appertureMax){
      currentApperture = apperture;
      Serial.print("new apperture will be: ");
      Serial.println(apperture);
      redrawAperture();
  } else if (apperture < appertureMin){
      Serial.print("failed to found apperture: ");
      Serial.println(apperture);
      redrawDecFloat(-1.f, "--", APERTURE_VALUE_ROW);
      redrawDecFloat(-1, "HIGH", MSG_VALUE_ROW);
  } else {
     Serial.print("failed to found apperture: ");
     Serial.println(apperture);
     redrawDecFloat(-1.f, "--", APERTURE_VALUE_ROW);
     redrawDecFloat(-1, "LOW", MSG_VALUE_ROW);
  }
}

void redrawDecFloat(float value,char* postFix,int textRowIndex){
  display.fillRect(textRowPosition[textRowIndex][0], textRowPosition[textRowIndex][1]-13, 40, 14, SSD1306_BLACK);
  display.setCursor(textRowPosition[textRowIndex][0], textRowPosition[textRowIndex][1]);
  if(value != -1.0f)
    display.print(value,2);
  if(postFix != NULL)
    display.println(postFix);
}

void drawSelectionIcon(int color){
  display.fillTriangle(
    trianglePos[currentParamForModifyIndex][0],  trianglePos[currentParamForModifyIndex][1],  
    trianglePos[currentParamForModifyIndex][2],  trianglePos[currentParamForModifyIndex][3],
    trianglePos[currentParamForModifyIndex][4],  trianglePos[currentParamForModifyIndex][5],
    color);
}


bool calcAuto(int iso){
  int arrLenght = sizeof(shutterValues)/sizeof(shutterValues[0]);
  float bestError = INFINITY;
  float bestAperture = 0;
  float bestShutterSpeed = 0;
  float apertureStep = 0.05f;
  int bestSSIndex = -1;
  float minShutterSpeed =  shutterValues[0];
  float maxShutterSpeed = shutterValues[arrLenght-1];
  bool errorState = true;
  Serial.print("ISO for auto mode: ");
  Serial.println(iso);
  for (float N = appertureMin; N <= appertureMax; N += apertureStep) {
    for (int j = 0; j < arrLenght; j++) {
      float t = shutterValues[j];
      float calculated_t = (N * N * K) / (currentLux * iso);
      float error = abs(calculated_t - t);
      
      if (error < bestError && calculated_t >0) {
        bestError = error;
        bestAperture = N;
        bestShutterSpeed = t;
        bestSSIndex = j;
        if (bestAperture >= appertureMin && bestAperture <= appertureMax && 
            bestShutterSpeed >= minShutterSpeed && bestShutterSpeed <= maxShutterSpeed) {
          errorState = false; // Found acceptable values
          bestSSIndex = j;
        }
      }
    }
  }
  if(errorState == false){
      shutterItemIndex == bestSSIndex;
      currentApperture = bestAperture;
  } else {
    Serial.println("AUTO MODE FAILED ");
  }
  Serial.println("================================");
  Serial.println("AUTO MODE RESULTS: ");
  Serial.print("bestSS: ");
  Serial.println(bestShutterSpeed);
  Serial.print("best AP: ");
  Serial.println(bestAperture);
  Serial.println("================================");
  return errorState;
}

float calcAperture(){
  return sqrt((shutterValues[shutterItemIndex] * currentLux * isoRange[isoIndex]) / K);
}

float calcSS(){
  return ((currentApperture * currentApperture) * K) / (currentLux * isoRange[isoIndex]);
}


