// Version 1.0
#include <ESP8266WiFi.h>
#include <TM1637Display.h>
#include <Wire.h>  // This library is already built in to the Arduino IDE
#include <LiquidCrystal_I2C.h> //This library you can add via Include Library > Manage Library > 

///////////////Parameters & Constants/////////////////
// WIFI params
char* WIFI_SSID = "FILL_THE_WIFI_SSID";    // Configure here the SSID of your WiFi Network
char* WIFI_PSWD = "FILL_THE_WIFI_PSWD"; // Configure here the PassWord of your WiFi Network
int WIFI_DELAY  = 100; //ms

// oneM2M : CSE params
String CSE_IP      = "X.X.X.X"; //Configure here the IP Address of your oneM2M CSE
int   CSE_HTTP_PORT = 8080;
String CSE_NAME    = "CseName";
String CSE_RELEASE = "3"; //Configure here the release supported by your oneM2M CSE
bool ACP_REQUIRED = false; //Configure here whether or not ACP is required controlling access
String ACPID = "";

// oneM2M : resources' params
String DESC_CNT_NAME = "DESCRIPTOR";
String DATA_CNT_NAME = "DATA";
String CMND_CNT_NAME = "COMMAND";
String ACP_NAME = "MYACP";
int TY_ACP  = 1;   
int TY_AE  = 2;   
int TY_CNT = 3; 
int TY_CI  = 4;
int TY_SUB = 23;
String originator = "Undefined";

// HTTP constants
int LOCAL_PORT = 80;
char* HTTP_CREATED = "HTTP/1.1 201 CREATED";
char* HTTP_OK    = "HTTP/1.1 200 OK\r\n";
int REQUEST_TIME_OUT = 5000; //ms
int REQUEST_NR = 0;

//PINS. Adapt the pins' number accordding to your wiring.
int BTN_PIN = D4;  // PIN for the Push Button. 
int POT_PIN = A0;  // PIN for the Potentiometer. 
int LCD_SCL_PIN = D1;  // PIN for the SCL Signal of the LCD
int LCD_SDA_PIN = D2;  // PIN for the SDA Signal of the LCD
int FDD_CLK_PIN = D5;  // PIN for the CLK Signal of the FourDigitDisplay
int FDD_DIO_PIN = D6;  // PIN for the DIO Signal of the FourDigitDisplay

// MISC
int SERIAL_SPEED  = 115200;
#define DEBUG
///////////////////////////////////////////
// Global variables
const long TIME_INTERVAL = 10000;
long currentMillis;

int newButtonState;
int oldButtonState;
LiquidCrystal_I2C lcd(0x27, 20, 4);
TM1637Display fdd(FDD_CLK_PIN, FDD_DIO_PIN);
const uint8_t FDD_SEND_MSG[]  = { SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,    /* Letter S*/
                                  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,    /* Letter E*/
                                  SEG_C | SEG_E | SEG_G,                    /* Letter n*/
                                  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G };  /* Letter d*/
const uint8_t FDD_BLANK_MSG[]  = { 0, 0, 0, 0 };  /* All digits off */

WiFiServer server(LOCAL_PORT);    // HTTP Server (over WiFi). Binded to listen on LOCAL_PORT constant
WiFiClient client0;
WiFiClient client1;
String context = "";        // The targeted actuator
String command = "";        // The received command

// Method for creating an HTTP POST with preconfigured oneM2M headers
// param : url  --> the url path of the targted oneM2M resource on the remote CSE
// param : ty --> content-type being sent over this POST request (2 for ae, 3 for cnt, etc.)
// param : rep  --> the representaton of the resource in JSON format
String doPOST(String url, String originator1, int ty, String rep) {

  String relHeader = "";
  if (CSE_RELEASE != "1") {
    relHeader = "X-M2M-RVI: " + CSE_RELEASE + "\r\n";
  }
  String postRequest = String() + "POST " + url + " HTTP/1.1\r\n" +
                       "Host: " + CSE_IP + ":" + CSE_HTTP_PORT + "\r\n" +
                       "X-M2M-Origin: " + originator + "\r\n" +
                       "Content-Type: application/vnd.onem2m-res+json;ty=" + ty + "\r\n" +
                       "Content-Length: " + rep.length() + "\r\n" +
                       relHeader +
                       "X-M2M-RI: req"+REQUEST_NR+"\r\n" +
                       "Connection: close\r\n\r\n" +
                       rep;

  // Connect to the CSE address
  Serial.println("connecting to " + CSE_IP + ":" + CSE_HTTP_PORT + " ...");

  // Get a client
  if (!client1.connect(CSE_IP, CSE_HTTP_PORT)) {
    Serial.println("Connection failed !");
    return "error";
  }

  // if connection succeeds, we show the request to be send
#ifdef DEBUG
  Serial.println(postRequest);
#endif

  // Send the HTTP POST request
  client1.print(postRequest);

  //Update request number after each sending
  REQUEST_NR += 1;
  
  // Manage a timeout
  unsigned long startTime = millis();
  while (client1.available() == 0) {
    if (millis() - startTime > REQUEST_TIME_OUT) {
      Serial.println("Client Timeout");
      client1.stop();
      return "error";
    }
  }

  // If success, Read the HTTP response
  String result = "";
  client1.setTimeout(500);
  if (client1.available()) {
    result = client1.readStringUntil('\r');
    Serial.println(result);
  }
  while (client1.available()) {
    String line = client1.readStringUntil('\r');
    Serial.print(line);
  }
  Serial.println();
  Serial.println("closing connection...");
  return result;
}

// Method for creating an ApplicationEntity(AE) resource on the remote CSE (this is done by sending a POST request)
// param : ae --> the AE name (should be unique under the remote CSE)
String createAE(String ae) {
  String srv = "";
    if(CSE_RELEASE != "1"){
      srv = ",\"srv\":[\""+CSE_RELEASE+"\"]";
    }
  String aeRepresentation =
    "{\"m2m:ae\": {"
    "\"rn\":\"" + ae + "\","
    "\"api\":\"Norg.demo." + ae + "\","
    "\"rr\":true,"
    "\"poa\":[\"http://" + WiFi.localIP().toString() + ":" + LOCAL_PORT + "/" + ae + "\"]" +
    srv +
    "}}";
#ifdef DEBUG
  Serial.println(aeRepresentation);
#endif
  return doPOST("/" + CSE_NAME, originator, TY_AE, aeRepresentation);
}

// Method for creating an Access Control Policy(ACP) resource on the remote CSE under a specific AE (this is done by sending a POST request)
// param : ae --> the targeted AE name (should be unique under the remote CSE)
// param : acp  --> the ACP name to be created under this AE (should be unique under this AE)
String createACP(String ae, String acp) {
  String acpRepresentation =
    "{\"m2m:acp\": {"
  "\"rn\":\"" + acp + "\","
  "\"pv\":{\"acr\":[{\"acor\":[\"all\"],\"acop\":63}]},"
  "\"pvs\":{\"acr\":[{\"acor\":[\"all\"],\"acop\":63}]}"
  "}}";
  return doPOST("/" + CSE_NAME + "/" + ae, originator, TY_ACP, acpRepresentation);
}

// Method for creating an Container(CNT) resource on the remote CSE under a specific AE (this is done by sending a POST request)
// param : ae --> the targeted AE name (should be unique under the remote CSE)
// param : cnt  --> the CNT name to be created under this AE (should be unique under this AE)
String createCNT(String ae, String cnt) {
  String cntRepresentation =
    "{\"m2m:cnt\": {"
    "\"mni\":10,"         // maximum number of instances
    "\"rn\":\"" + cnt + "\"" +
    ACPID + //IF ACP created, it is associated to the container so that anyone has access 
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae, originator, TY_CNT, cntRepresentation);
}

// Method for creating an ContentInstance(CI) resource on the remote CSE under a specific CNT (this is done by sending a POST request)
// param : ae --> the targted AE name (should be unique under the remote CSE)
// param : cnt  --> the targeted CNT name (should be unique under this AE)
// param : ciContent --> the CI content (not the name, we don't give a name for ContentInstances)
String createCI(String ae, String cnt, String ciContent) {
  String ciRepresentation =
    "{\"m2m:cin\": {"
    "\"con\":\"" + ciContent + "\""
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae + "/" + cnt, originator,  TY_CI, ciRepresentation);
}


// Method for creating an Subscription (SUB) resource on the remote CSE (this is done by sending a POST request)
// param : ae --> The AE name under which the SUB will be created .(should be unique under the remote CSE)
//          The SUB resource will be created under the COMMAND container more precisely.
String createSUB(String ae) {
  String subRepresentation =
    "{\"m2m:sub\": {"
    "\"rn\":\"SUB_" + ae + "\","
    "\"nu\":[\"" + CSE_NAME + "/" + ae  + "\"], "
    "\"nct\":1,"
    "\"enc\":{\"net\":[3]}"
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae + "/" + CMND_CNT_NAME, originator,  TY_SUB, subRepresentation);
}


// Method to register a module (i.e. sensor or actuator) on a remote oneM2M CSE
void registerModule(String module, bool isActuator, String intialDescription, String initialData) {
  if (WiFi.status() == WL_CONNECTED) {
    String result;
  
    // 1. Create the ApplicationEntity (AE) for this sensor
    result = createAE(module);
    if (result.equalsIgnoreCase(HTTP_CREATED)) {
      #ifdef DEBUG
      Serial.println("AE " + module + " created  !");
      #endif
      // 1.1 Create the AccessControlPolicy (ACP) for this sensor so that monitor can subscribe to it
      if(ACP_REQUIRED) {
        result = createACP(module, ACP_NAME);
        if (result.equalsIgnoreCase(HTTP_CREATED)) {
          ACPID = ",\"acpi\":[\"" + CSE_NAME + "/" + module + "/" + ACP_NAME + "\"]";
          #ifdef DEBUG
            Serial.println("ACP " + module + " created  !");
          #endif
        }
      }
      // 2. Create a first container (CNT) to store the description(s) of the sensor
      result = createCNT(module, DESC_CNT_NAME);
      if (result.equalsIgnoreCase(HTTP_CREATED)) {
        #ifdef DEBUG
        Serial.println("CNT " + module + "/" + DESC_CNT_NAME + " created  !");
        #endif
        // Create a first description under this container in the form of a ContentInstance (CI)
        result = createCI(module, DESC_CNT_NAME, intialDescription);
        if (result.equalsIgnoreCase(HTTP_CREATED)) {
          #ifdef DEBUG
          Serial.println("CI " + module + "/" + DESC_CNT_NAME + "/{initial_description} created !");
          #endif
        }
      }
      // 3. Create a second container (CNT) to store the data  of the sensor
      result = createCNT(module, DATA_CNT_NAME);
      if (result.equalsIgnoreCase(HTTP_CREATED)) {
        #ifdef DEBUG
        Serial.println("CNT " + module + "/" + DATA_CNT_NAME + " created !");
        #endif
        // Create a first data value under this container in the form of a ContentInstance (CI)
        result = createCI(module, DATA_CNT_NAME, initialData);
        if (result.equalsIgnoreCase(HTTP_CREATED)) {
          #ifdef DEBUG
          Serial.println("CI " + module + "/" + DATA_CNT_NAME + "/{initial_data} created !");
          #endif
        }
      }

      // 4. if the module is an actuator, create a third container (CNT) to store the received commands
      if (isActuator) {
        result = createCNT(module, CMND_CNT_NAME);
        if (result.equalsIgnoreCase(HTTP_CREATED)) {
          #ifdef DEBUG
          Serial.println("CNT " + module + "/" + CMND_CNT_NAME + " created !");
          #endif
          // subscribe to any command put in this container
          result = createSUB(module);
          if (result.equalsIgnoreCase(HTTP_CREATED)) {
            #ifdef DEBUG
            Serial.println("SUB " + module + "/" + CMND_CNT_NAME + "/SUB_" + module + " created !");
            #endif
          }
        }
      }
    }
  }
}

void init_IO() {
  Serial.begin(SERIAL_SPEED);
  
  lcd.init();   // initializing the LCD
  lcd.backlight(); // Enables backlight
  
  fdd.setBrightness(0x0A);    // Sets the brightness of the FourDigitDisplay
  fdd.setSegments(FDD_BLANK_MSG);       // Clears the FourDigitDisplay
 
  pinMode(BTN_PIN, INPUT);
  oldButtonState = 0; // At the startup, the button is not pushed
}
void task_IO() {
}

void init_WiFi() {
  Serial.println("Connecting to  " + String(WIFI_SSID) + " ...");
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PSWD);

  // wait until the device is connected to the wifi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_DELAY);
    Serial.print(".");
  }

  // Connected, show the obtained ip address
  Serial.println("WiFi Connected ==> IP Address = " + WiFi.localIP().toString());
}
void task_WiFi() {
}

void init_HTTPServer() {
  server.begin();
  Serial.println("Local HTTP Server started !");
}
void task_HTTPServer() {
  // Check if a client is connected
  client0 = server.available();
  if (client0){  
    // Wait until the client sends some data
    Serial.println("New client connected. Receiving request <=== ");
  
    while (!client0.available()) {
      #ifdef DEBUG
      Serial.print(".");
      #endif
      delay(5);
    }

    // Read the request
    client0.setTimeout(500);
    String request = client0.readString();
    Serial.println(request);

    int start, end;
    // identify the right module (sensor or actuator) that received the notification
    // the URL used is ip:port/ae
    start = request.indexOf("/");
    end = request.indexOf("HTTP") - 1;
    context = request.substring(start+1, end);
    if context.endsWith("/") {
	  context.remove(length(context)-1,1);
	}
	#ifdef DEBUG
    Serial.println(String() + "context = [" + start + "," + end + "] -> " + context);
    #endif

    // ingore verification messages 
    if (request.indexOf("vrq") > 0) {
      client0.flush();
      client0.print(HTTP_OK);
      delay(5);
      client0.stop();
      Serial.println("Client disconnected");
      return;
    }

    //Parse the request and identify the requested command from the device
    //Request should be like "[operation_name]"
    start = request.indexOf("[");  
    end = request.indexOf("]"); // first occurence fo 
    command = request.substring(start+1, end);
    #ifdef DEBUG
    Serial.println(String() + + "command = [" +  start + "," + end + "] -> " + command);
    #endif

    client0.flush();
    client0.print(HTTP_OK);
    delay(5);
    client0.stop();
    Serial.println("Client disconnected");
  }
}
void handle_HTTPMessage(){
  if (context != "") {
      if (context == "DisplayActuator") {
          command_display(command);
          task_display(command);
      }
      else
          Serial.println("The target AE does not exist ! ");
  }
  // reset "command" and "context" variables for future received requests
  context = "";
  command = "";
}

void init_potentiometer() {
  String initialDescription = "Name = PotentiometerSensor\t"
                              "Unit = Bool\t"
                              "Location = MainDoor\t";
  String initialData = "0";
  originator = "Cae-PotentiometerSensor";
  registerModule("PotentiometerSensor", false, initialDescription, initialData);
}
void task_potentiometer() {
    int sensorValue;
    sensorValue = analogRead(POT_PIN);
    fdd.showNumberDec(sensorValue);   
       
    newButtonState = digitalRead(BTN_PIN);
    if (oldButtonState==0 && newButtonState==1) {
      
      fdd.setSegments(FDD_SEND_MSG);
      String ciContent = "";
      #ifdef DEBUG
      Serial.print("Button Clicked -> Sending data : potentiometer value = ");
      Serial.println(sensorValue);
      #endif
      ciContent = sensorValue;
      originator = "Cae-PotentiometerSensor";
      createCI("PotentiometerSensor", DATA_CNT_NAME, ciContent);   
      fdd.setSegments(FDD_BLANK_MSG);
    }
    oldButtonState = newButtonState;
}
void command_potentiometer(String cmd) {
}

void init_display() {
  String initialDescription = "Name = DisplayActuator\t"
                              "Location = Home\t";
  String initialData = "void";
  originator = "Cae-DisplayActuator";
  registerModule("DisplayActuator", true, initialDescription, initialData);
}
void task_display(String cmd) {
  originator = "Cae-DisplayActuator";
  createCI("DisplayActuator", DATA_CNT_NAME, cmd);  
}
void command_display(String cmd) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MSG Received => ");
  lcd.setCursor(0, 1);
  lcd.print(cmd); 
  delay(1000);
  lcd.clear();
}

void setup() {
  // intialize the serial liaison
  Serial.begin(SERIAL_SPEED);

  // configure sensors and actuators HW
  init_IO();

  // Connect to WiFi network
  init_WiFi();

  // Start HTTP server
  init_HTTPServer();

  // register sensors and actuators
  init_potentiometer();
  init_display();
}

// Main loop of the µController
void loop() {
    // Check if a client is connected
    task_HTTPServer();
    
    // analyse the received command (if any)
    handle_HTTPMessage();

    // handle sensors 
    task_potentiometer();
}
