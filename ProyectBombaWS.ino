#include <ESP8266WiFi.h>  
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WebSocketsServer.h>
const char* ssid     = "INFINITUMku6c";         // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = "7eec2030f5";     // The password of the Wi-Fi network

ESP8266WebServer server (80);

void getConfigFile(); //abre la configuracion inicial al encender
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void apiworkHandler(String button);
void apigetrunHandler(uint8_t num);
void apiconfigHandler();
void webSocketTest();
void timer(int minutos);
void InitWebSockets();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);
void sendStatusThingspeak(int(aux), boolean getResponse);

boolean bomb= false;
boolean lamp1= false;
boolean lamp2= false;

String message;

unsigned long initialTime;

int onMinutes=3;
int elapsedMinutes=0;

boolean connectedDevices[5]={false, false, false, false, false};

const char* host = "api.thingspeak.com";

WebSocketsServer webSocket(81);
WiFiClient client;

void setup() {
  pinMode(2,OUTPUT);
  pinMode(0, INPUT);
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');
  
  WiFi.begin(ssid, password);             // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i); Serial.print(' ');
  }

  Serial.println('\n');
  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer

  SPIFFS.begin();                           // Start the SPI Flash Files System
  //server.on("/apiwork/",apiworkHandler);
  //server.on("/apigetrun/",HTTP_GET,apigetrunHandler);
 // server.on("/apiconfig/",apiconfigHandler);
  server.on("/WebSocketTest",[](){
    Serial.println("XD");
    if (!handleFileRead(server.uri()+"/webOS.html"))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
    });
  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
    //if(server.uri()=="/WebSocketTest/WS.js");
      ;
  });
  getConfigFile();
  const char * headerkeys[] = {"User-Agent","Connection"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);

  InitWebSockets();
  
  
  server.collectHeaders(headerkeys, headerkeyssize ); 
  server.begin();                           // Actually start the server
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();                    // Listen for HTTP requests from clients
  if(bomb){
    timer(onMinutes);
  }
  webSocket.loop();
  if(!digitalRead(0)){
    delay(200);
    while(!digitalRead(0));
    Serial.println("hola");
    webSocket.broadcastTXT("hola");
  }
}
  
String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  Serial.println("\tFile Not Found");
  return false;                                         // If the file doesn't exist, return false
}

void apigetrunHandler(uint8_t num){
  message="";
  StaticJsonDocument<100> doc;
  if(num==7)
    doc["action"]="timerout";
  else
    doc["action"]="startConfig";
  doc["bomb"]=int(bomb);
  doc["lamp1"]=int(lamp1);
  doc["lamp2"]=int(lamp2);
  doc["onMinutes"]=int(onMinutes);
  serializeJson(doc,message);
  if(num==7)
     webSocket.broadcastTXT(message);
  else
     webSocket.sendTXT(num,message);

  Serial.println(message);
}

void apiworkHandler(String button){
  boolean aux = false;
   if(button=="bomb"){
      bomb=!bomb;
      digitalWrite(2,bomb);
      aux=bomb;
    }
   else if (button=="lamp1"){
      lamp1=!lamp1;
      aux=lamp1;
    }
   else if (button=="lamp2"){
      lamp2=!lamp2;
      aux=lamp2;
   }
   message= "";
   StaticJsonDocument<100> doc;
   doc["action"]="toggle";
   doc[button]=int(aux);
   serializeJson(doc,message);
   webSocket.broadcastTXT(message);
   Serial.println(message);
   initialTime=millis();
   sendStatusThingspeak(int(aux), false);
}

void apiconfigHandler(){
    File file=SPIFFS.open("/config.txt","w");
    StaticJsonDocument<100> sendo;
    sendo["onMinutes"]=onMinutes;
    serializeJson(sendo,file);
    file.close();
     message="";
     StaticJsonDocument<100> doc;
     doc["action"]="config";
     doc["onMinutes"]=onMinutes;
     serializeJson(doc,message);
     webSocket.broadcastTXT(message);
    Serial.println(message);
    
  }

void timer(int minutos){
  unsigned long runningTime = millis()-initialTime;
  if(runningTime>=30000){
    initialTime=millis();
    elapsedMinutes++;
    Serial.print("Valor Actual de elapsedMinutes: ");
    Serial.println(elapsedMinutes);
    if (elapsedMinutes>=(2*onMinutes)){
      bomb=false;
      digitalWrite(2,bomb);
      elapsedMinutes=0;
      sendStatusThingspeak(int(bomb), false);
      apigetrunHandler(7);
    }
  }
}

void getConfigFile(){
  String txtConfig = "/config.txt";
  if (SPIFFS.exists(txtConfig)){
    File file=SPIFFS.open(txtConfig, "r");
    StaticJsonDocument<100> doc;
    deserializeJson(doc, file);
    onMinutes=doc["onMinutes"];
    file.close();
    return;
  }
  Serial.println("File couldn't be opened");
}

void sendStatusThingspeak(int variable, boolean getResponse){
     if(client.connect(host,80)){
     Serial.println("Conectado uwu");
     client.print(String("GET /") + "update?api_key=ZAGIY1PFF98U4R8H&field1="+int(variable)+"\r\n"+"Connection: close\r\n" +"\r\n");
     if (getResponse){
       while (client.connected() || client.available())
        {
          if (client.available())
          {
            String line = client.readStringUntil('\n');
            Serial.println(line);
          }
        }
     }
      client.stop();
      Serial.println("\n[Disconnected]");
   }
}
void InitWebSockets() {
   webSocket.begin();
   webSocket.onEvent(webSocketEvent);
   Serial.println("WebSocket server started");
}
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) 
{
   switch(type) {
   case WStype_DISCONNECTED:
      connectedDevices[num]=false;
      break;
   case WStype_CONNECTED:
      apigetrunHandler(num);
      connectedDevices[num]=true;
      break;
   case WStype_TEXT:
      StaticJsonDocument<100> doc;
      deserializeJson(doc, payload);
      String command = doc["action"];
      if(command=="toggle")
        apiworkHandler(doc["button"]);
      else if (command =="config"){
        onMinutes=doc["onMinutes"];
        apiconfigHandler();
        }
      break;
   }
}
