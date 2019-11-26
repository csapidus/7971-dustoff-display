/*  B7971 DUSTOFF BOARD
 *  Developer: Mahdi Al-Husseini, mahdi07@msn.com
 *  Note: This project uses an Espressif 8266 NodeMCU LUA uC board, allowing for internet connectivity. It is required you set the ssid and password variables appropriately, and your lon and lat if intending on using METAR/TAF
 *  License (Beerware): This code is open to the public but you buy me a beer if you use this and we meet someday
 */

#include <ESP8266WiFi.h>  // standard 8266 WiFi connectivity (https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html)
#include <WiFiClientSecure.h>  // supports secure connections using TLS (SSL) (https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure)
// #include <TimeLib.h> // time-keeping functionality for Arduino (https://github.com/PaulStoffregen/Time)
#include <WiFiUdp.h> // allows for sending and recieving UDP packets (https://www.arduino.cc/en/Reference/WiFiUDPConstructor)

// WiFi
const char* ssid = "DUSTOFF";
const char* password = "MEDEVAC";

// National Weather Service (NWS) API
const char* host = "www.aviationweather.gov";

// Your current location; adjust as needed. This is currently hardcoded, but an update in the future will allow automatic triangulation of the 8266 location using LocationAPI from Unwired Labs.
String lon = "33.738328";
String lat = "-84.707237";

/* TAF is Terminal Aerodrome Forecast: https://en.wikipedia.org/wiki/Terminal_aerodrome_forecast
 * METAR is Meteorological Terminal Aviation Routine [Weather Report]: https://en.wikipedia.org/wiki/METAR
 * Guidance for designing url_METAR requests: https://www.aviationweather.gov/dataserver/example?datatype=metar 
 * Guidance for designing url_TAF requests: https://www.aviationweather.gov/dataserver/example?datatype=metar 
 * url_METAR: currently set to retrieve the most recent METAR in the past three hours within 20 statue miles
 * url_TAF: currently set to retrieve the most recent RAF in the past three hours within 20 statue miles
 */
 
String url_METAR = "/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&radialDistance=20;" + lat + "," + lon + "&hoursBeforeNow=3&mostRecent=true&&fields=raw_text";
String url_TAF = "/adds/dataserver_current/httpparam?dataSource=tafs&requestType=retrieve&format=xml&radialDistance=20;" + lat + "," + lon + "&hoursBeforeNow=3&mostRecent=true&&fields=raw_text";
const int httpsPort = 443;
BearSSL::WiFiClientSecure client;

// State Variables
bool awaitingArrivals = true;
bool arrivalsRequested = false;

// Configuration
int pollDelay = 20000; //time between each retrieval
int retryWifiDelay = 5000;
const int API_TIMEOUT = 10000;

// 7971 Smart Sockets
String submission;
String instance;
String wifi_status;

void setup() {
  Serial.begin(9600);
  // Serial.begin(115200);
  // {$B7F...}, {$B7E...}, and {$B7S...} are standard configurations for B7971 text display. This can be adjusted as desired; reference the smart_socket guide pdf. 
  Serial.println("$B7F0000000<cr>");
  Serial.println("$B7E0000000<cr>");
  Serial.println("$B7S0000000<cr>");
  // Retain "$B7M" and "<cr>, but replace "MEDEVAC" as desired for start-up display. 
  Serial.println("$B7MMEDEVAC<cr>");
  connectToWifi();
  client.setInsecure();
  client.setTimeout(API_TIMEOUT);
}

void connectToWifi() {
  //connect to wifi
  // Serial.print("connecting to ");
  // Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { //WAITING FOR WIFI
    delay(500);
    // Serial.print(".");
  }
  // Serial.println("");
  // Serial.println("WiFi connected");
  // Serial.println("IP address: ");
  // Serial.println(WiFi.localIP());
  scroll("Wifi succesfully connected");
}

void getMETAR() {
  // Use WiFiClientSecure class to create TLS connection
  // Serial.print("Connecting to ");
  // Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    scroll("connection failed");
    delay(retryWifiDelay);
    return;
  }
  
  // Query the API, METAR
  client.print(String("GET ") + url_METAR + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" + 
             "Connection: close \r\n\r\n");
  while(!client.available()){ delay(1);}

  scroll("client available");
  String matchString;
  String matchAppend;

  while(client.available()){
    matchString = client.readStringUntil('\r');
    matchAppend += matchString;
  }

  // Serial.println(matchAppend);

  // PARSING METAR
  int lineEstimated = matchAppend.indexOf("<raw_text>");
  if (lineEstimated == -1) {
    scroll("NO METAR ALTER PARAMETERS");
    delay(retryWifiDelay);
    return;
  }
  int lineStart = lineEstimated + 10; //exclude `<raw_text>`
  int lineEnd = matchAppend.indexOf("</raw_text>");
  String METAR = matchAppend.substring(lineStart, lineEnd);
  // Serial.print("METAR: ");
  // Serial.println(METAR);
  scroll("METAR");
  scroll(METAR);
  
  awaitingArrivals = false;
  client.stop();
}

void getTAF() {
  // Use WiFiClientSecure class to create TLS connection
  // Serial.print("Connecting to ");
  // Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    // Serial.println(" ... connection failed");
    delay(retryWifiDelay);
    return;
  }
  
  // Query the API, TAF
  client.print(String("GET ") + url_TAF + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" + 
             "Connection: close \r\n\r\n");
  while(!client.available()){ delay(1);}

  // Serial.println("Client available");
  String matchString;
  String matchAppend;

  while(client.available()){
    matchString = client.readStringUntil('\r');
    matchAppend += matchString;
  }

  // Serial.println(matchAppend);

  // PARSING TAF
  int lineEstimated = matchAppend.indexOf("<raw_text>");
  if (lineEstimated == -1) {
    // Serial.println("NO TAF, ALTER PARAMETERS");
    delay(retryWifiDelay);
    return;
  }
  int lineStart = lineEstimated + 10; //exclude `<raw_text>`
  int lineEnd = matchAppend.indexOf("</raw_text>");
  String TAF = matchAppend.substring(lineStart, lineEnd);
  // Serial.print("TAF: ");
  // Serial.println(TAF);
  scroll("TAF");
  scroll(TAF);
  
  awaitingArrivals = false;
  client.stop();
}

void loop() {
  if (awaitingArrivals) {
    if (!arrivalsRequested) {
      arrivalsRequested = true;
      getMETAR();
      getTAF();
    }
  }
  delay(pollDelay);
}

void resetCycle() {
  awaitingArrivals = true;
  arrivalsRequested = false;
}

// Scroll text across B7971s
void scroll(String data) {
  // Uppercase the input string
  data = "       " + data + "       ";
  data.toUpperCase();
  
  for (int x = 0; x < data.length() - 6; x++) {
    instance = data.substring(x, x+7);
    submission = "$B7M" + instance + "<cr>";
    Serial.println(submission);
    // Adjust delay speed for fast or slow scroll
    delay(350);
  }
}
