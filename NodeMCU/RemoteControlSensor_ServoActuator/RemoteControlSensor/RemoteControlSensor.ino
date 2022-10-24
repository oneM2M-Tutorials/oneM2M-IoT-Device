// Version 1.0
#include <ESP8266WiFi.h>
#include <IRrecv.h> 
#include <IRremoteESP8266.h>
#include <TM1637Display.h>
#include <Wire.h>  

///////////////Parameters & Constants/////////////////
// WIFI params
char* WIFI_SSID = "WARNING_FILL_THE_WIFI_SSID";    // Configure here the SSID of your WiFi Network
char* WIFI_PSWD = "WARNING_FILL_THE_WIFI_PSWD"; // Configure here the PassWord of your WiFi Network
int WIFI_DELAY  = 100; //ms

// oneM2M : CSE params
String CSE_IP      = "WARNING_FILL_THE_IP_ADDRESS_OF_THE_CSE"; //Configure here the IP Address of your oneM2M CSE
int   CSE_HTTP_PORT = 8080;
String CSE_NAME    = "WARNING_FILL_THE_ONEM2M_CSE_NAME";
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
int RC_PIN = D3;  // PIN for the RemoteControl InfraRed Receiver
int FDD_CLK_PIN = D5;  // PIN for the CLK Signal of the FourDigitDisplay
int FDD_DIO_PIN = D6;  // PIN for the DIO Signal of the FourDigitDisplay

// MISC
int SERIAL_SPEED  = 115200;
#define DEBUG
///////////////////////////////////////////
// Global variables
const long TIME_INTERVAL = 10000;
long currentMillis;

IRrecv IRReceiver(RC_PIN);
decode_results remoteControlDecodingResults;
class  IR_REMOTE {
  public:
    static const unsigned int CH_MINUS    = 0xFFA25D;
    static const unsigned int CHANNEL     = 0xFF629D;
    static const unsigned int CH_PLUS     = 0xFFE21D;
    static const unsigned int PREV        = 0xFF22DD;
    static const unsigned int NEXT        = 0xFF02FD;
    static const unsigned int PLAY_PAUSE  = 0xFFC23D;
    static const unsigned int VOL_MINUS   = 0xFFE01F;
    static const unsigned int VOL_PLUS    = 0xFFA857;
    static const unsigned int EQ          = 0xFF906F;
    static const unsigned int CH0         = 0xFF6897;
    static const unsigned int CH1         = 0xFF30CF;
    static const unsigned int CH2         = 0xFF18E7;
    static const unsigned int CH3         = 0xFF7A85;
    static const unsigned int CH4         = 0xFF10EF;
    static const unsigned int CH5         = 0xFF38C7;
    static const unsigned int CH6         = 0xFF5AA5;
    static const unsigned int CH7         = 0xFF42BD;
    static const unsigned int CH8         = 0xFF4AB5;
    static const unsigned int CH9         = 0xFF52AD;
    static const unsigned int CH_PLUS_100 = 0xFF9867;
    static const unsigned int CH_PLUS_200 = 0xFFB04F;

    static int GetIntegerValue(int ButtonCode) {
      int result;
      switch (ButtonCode) {
        case CH0 : result = 0; break;
        case CH1 : result = 1; break;
        case CH2 : result = 2; break;
        case CH3 : result = 3; break;
        case CH4 : result = 4; break;                 
        case CH5 : result = 5; break; 
        case CH6 : result = 6; break;
        case CH7 : result = 7; break;
        case CH8 : result = 8; break;
        case CH9 : result = 9; break; 
       
        case CHANNEL    : result = 10; break; 
        case CH_MINUS   : result = 11; break; 
        case CH_PLUS    : result = 12; break; 
        case PREV       : result = 13; break; 
        case NEXT       : result = 14; break; 
        case PLAY_PAUSE : result = 15; break; 
        case VOL_MINUS  : result = 16; break; 
        case VOL_PLUS   : result = 17; break; 
        case EQ         : result = 18; break; 
        
        case CH_PLUS_100: result = 100; break;   
        case CH_PLUS_200: result = 200; break; 

        default: result = -1; break;
      }
      return result;
    }
    
    static String GetStringValue(int ButtonCode) {
      String result;
      switch (ButtonCode) {
        case CH0 : result = "CH 0"; break;
        case CH1 : result = "CH 1"; break;
        case CH2 : result = "CH 2"; break;
        case CH3 : result = "CH 3"; break;
        case CH4 : result = "CH 4"; break;                 
        case CH5 : result = "CH 5"; break; 
        case CH6 : result = "CH 6"; break;
        case CH7 : result = "CH 7"; break;
        case CH8 : result = "CH 8"; break;
        case CH9 : result = "CH 9"; break; 
       
        case CHANNEL    : result = "CH"; break; 
        case CH_MINUS   : result = "CH-"; break; 
        case CH_PLUS    : result = "CH+"; break; 
        case PREV       : result = "<<"; break; 
        case NEXT       : result = ">>"; break; 
        case PLAY_PAUSE : result = ">||"; break; 
        case VOL_MINUS  : result = "VOL-"; break; 
        case VOL_PLUS   : result = "VOL+"; break; 
        case EQ         : result = "EQ"; break; 
        
        case CH_PLUS_100: result = "CH +100"; break;   
        case CH_PLUS_200: result = "CH +200"; break; 

        default: result = "?"; break;
      }
      return result;
    }
};

TM1637Display fdd(FDD_CLK_PIN, FDD_DIO_PIN);
const uint8_t FDD_SEND_MSG[]  = { SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,    /* Letter S */
                                  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,    /* Letter E */
                                  SEG_C | SEG_E | SEG_G,                    /* Letter n */
                                  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G };  /* Letter d */
                                  
const uint8_t FDD_BLANK_MSG[]  = { 0, 0, 0, 0 };  /* All digits off */

const uint8_t FDD_DEFAULT_MSG[]  = { SEG_G, SEG_G, SEG_G, SEG_G };  /* All digits '-' */

const uint8_t FDD_CHANNEL_MSG[]  = {
                                  SEG_A | SEG_D | SEG_E | SEG_F,          /* Letter C */
                                  SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,  /* Letter H */
                                  0,                                      /* Blank */
                                  0  };                                   /* Blank */


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
  fdd.setBrightness(0x0A);    // Sets the brightness of the FourDigitDisplay
  fdd.setSegments(FDD_BLANK_MSG);       // Clears the FourDigitDisplay
 
  pinMode(RC_PIN, INPUT);   // InfraRed receiver pin is set as input
  IRReceiver.enableIRIn();    // enabling the InfraRed receiver on the corresponding pin 
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
	Serial.println("The target AE does not exist ! ");
  }
  // reset "command" and "context" variables for future received requests
  context = "";
  command = "";
}

void init_remotecontrol() {
  String initialDescription = "Name = RemoteControlSensor\t"
                              "Unit = Enum(integer)\t"
                              "Location = Home\t";
  String initialData = "0";
  originator = "Cae-RemoteControlSensor";
  registerModule("RemoteControlSensor", false, initialDescription, initialData);
}
void task_remotecontrol() {
    int sensorValue;   
    if (IRReceiver.decode(&remoteControlDecodingResults)) {
      IRReceiver.resume();
      sensorValue = remoteControlDecodingResults.value;

      int simpleSensorValue = IR_REMOTE::GetIntegerValue(sensorValue);
      if (simpleSensorValue != -1) {
        if (simpleSensorValue < 10) {
          fdd.setSegments(FDD_CHANNEL_MSG);
          fdd.showNumberDec(simpleSensorValue, false, 1, 3);  // showshowNumberDec(Number, leadingZeros=false, length=1(1 digit), position=0,1,2,3)
          
          #ifdef DEBUG
          Serial.print("RemoteControl Button Selected : Label = ");
          Serial.println(IR_REMOTE::GetStringValue(sensorValue)); 
          Serial.print("Sending data : RemoteControl value = ");
          Serial.println(simpleSensorValue);
          #endif
    
          String ciContent = String(simpleSensorValue);
          originator = "Cae-RemoteControlSensor";
          createCI("RemoteControlSensor", DATA_CNT_NAME, ciContent);   
          fdd.setSegments(FDD_BLANK_MSG);        
        
        }
        else {
          fdd.setSegments(FDD_DEFAULT_MSG);
        }
      }
    } 
}
void command_remotecontrol(String cmd) {
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
  init_remotecontrol();
}

// Main loop of the µController
void loop() {
    // Check if a client is connected
    task_HTTPServer();
    
    // analyse the received command (if any)
    handle_HTTPMessage();

    // handle sensors 
    task_remotecontrol();

    delay(100);
}
