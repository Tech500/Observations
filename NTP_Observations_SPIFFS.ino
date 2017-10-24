//////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                       ESP8266 --Internet Weather Datalogger and Dynamic Web Server  
//
//                       "NTP_Observations_SPIFFS.ino"    10//2/2017 @ 5:09 EST --ab9nq.william--at-gmail.com
//
//                       Modified Wifi mode  
//
//                       https://forum.arduino.cc/index.php?topic=466867.0     //Project discussion at arduino.cc
//
//                       Supports WeMos D1 R2 Revison 2.1.0 and RobotDyn WiFi D1 R2 32 MB   --ESP8266EX Baseds Developement Board
//
//                       https://www.wemos.cc/product/d1.html 
//
//                       http://robotdyn.com/catalog/boards/wifi_d1_r2_esp8266_dev_board_32m_flash/
//
//                       listFiles and readFile by martinayotte of ESP8266 Community Forum 
//
//                       NTP useage with Timezones by zoomx of the Arduino.cc Forum
//
//                       Previous project:  "SdBrose_CC3000_HTTPServer.ino" by tech500" on https://github.com/tech500
//
//                       Project is Open-Source, uses one Barometric Pressure sensor, BME280.
//
//                       RTC is software and NTP synchronized
//
//   Upload
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Some lines require editing with your data.  Such as YourSSID, YourPassword, Your ThinkSpeak ChannelNumber, Your Thinkspeak API Key, Public IP, Forwarded Port,
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ********************************************************************************
// ********************************************************************************
//
//   See invidual library downloads for each library license.
//
//   Following code was developed using the Adafruit CC300 library, "HTTPServer" example.
//   and by merging library examples, adding logic for Sketch flow.
//
// *********************************************************************************
// *********************************************************************************

#include <Wire.h>   //Part of the Arduino IDE software download
#include "RTClib.h" //https://github.com/adafruit/RTClib. Get it using the Library Manager
#include <TimeLib.h>        //Get it using the Library Manager. Use TimeLib instead of Time otherwise you will get errors in Timezone library
#include <Timezone.h>    //https://github.com/JChristensen/Timezone
#include <ESP8266WiFi.h>   //Part of ESP8266 Board Manager install
#include <FS.h>   //Part of ESP8266-Arduino Core
//#include <Adafruit_Sensor.h>   //https://github.com/adafruit/Adafruit_Sensor Not used anymore
#include <Adafruit_BME280.h>   //https://github.com/adafruit/Adafruit_BME280_Library . Get it using the Library Manager
//#include <LiquidCrystal_I2C.h>   //https://github.com/esp8266/Basic/tree/master/libraries/LiquidCrystal Not used anymore
#include <SPI.h>   //Part of Arduino Library Manager
#include <ThingSpeak.h>   //https://github.com/mathworks/thingspeak-arduino . Get it using the Library Manager
#include <WiFiUdp.h>  //Part of ESP8266 Board Manager install, used by NTP client

// Replace with your network details
const char* ssid = "YourSSID";
const char* password = "YourPassword";

///////////////////////////////////////////////////
// Replace with your TimeZone details
// See WordClock example int the TimeZone library
///////////////////////////////////////////////////
//Central European Time (Frankfurt, Paris, Rome)
//TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
//TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time
//Timezone myTZ(CEST, CET);
//US Eastern Time Zone (New York, Detroit)
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  //Eastern Daylight Time = UTC - 4 hours
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   //Eastern Standard Time = UTC - 5 hours
Timezone myTZ(usEDT, usEST);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev
time_t utc, local;

float bme_pressure, millibars, bme_temp, fahrenheit, bme_humidity, RHx, T, heat_index, dew, dew_point, atm, bme_altitude;

int count = 0;

Adafruit_BME280 bme;
#define BMEADDRESS 0x77 // Note Adafruit module I2C adress is 0x77 my module (eBay) uses 0x76

unsigned long delayTime;

#define SEALEVELPRESSURE_HPA (1013.25)   // Average millbars at Sea Level.  Check local weather service office.

RTC_Millis Clock;   

bool Century = false;
bool h12;
bool PM;
bool AcquisitionDone = false;

int year1, month1, date1, hour1, minute1, second1;   //the original year, month ... are in conflict with TimeLib
byte dayofWeek;

//use I2Cscanner to find LCD display address, in this case 3F   //https://github.com/todbot/arduino-i2c-scanner/
//LiquidCrystal_I2C lcd(0x3F,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display

#define sonalert 9  // pin for Piezo buzzer

#define online D6  //pin for online LED indicator

#define BUFSIZE 64  //Size of read buffer for file download  -optimized for CC3000.

float currentPressure;  //Present pressure reading used to find pressure change difference.
float pastPressure;  //Previous pressure reading used to find pressure change difference.
float milliBars;   //Barometric pressure in millibars
float difference;   //change in barometric pressure drop; greater than .020 inches of mercury.

//long int id = 1;  //Increments record number

char *filename;
char str[] = {0};

String dtStamp;
String lastUpdate;
String SMonth, SDay, SYear, SHour, SMin, SSec;

String fileRead;

char MyBuffer[13];

#define LISTEN_PORT           8002    // What TCP port to listen on for connections. // Change this *!
// The HTTP protocol uses port 80 by default.

#define MAX_ACTION            10      // Maximum length of the HTTP action that can be parsed.

#define MAX_PATH              64      // Maximum length of the HTTP request path that can be parsed.
// There isn't much memory available so keep this short!

#define BUFFER_SIZE           MAX_ACTION + MAX_PATH + 20  // Size of buffer for incoming request data.
// Since only the first line is parsed this
// needs to be as large as the maximum action
// and path plus a little for whitespace and
// HTTP version.

#define TIMEOUT_MS           250   // Amount of time in milliseconds to wait for     /////////default 500/////
// an incoming request to finish.  Don't set this
// too high or your server could be slow to respond.

uint8_t buffer[BUFFER_SIZE + 1];
int bufindex = 0;
char action[MAX_ACTION + 1];
char path[MAX_PATH + 1];

//NTP stuff
unsigned int localPort = 123;      // local port to listen for UDP packets
unsigned long epoch;
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;



//////////////////////////
// Web Server on port LISTEN_PORT
/////////////////////////
WiFiServer server(LISTEN_PORT);
WiFiClient client;

/*
  This is the ThingSpeak channel number for the MathwWorks weather station
  https://thingspeak.com/channels/YourChannelNumber.  It senses a number of things and puts them in the eight
  field of the channel:

  Field 1 - Temperature (Degrees F)
  Field 2 - Humidity (%RH)
  Field 3 - Barometric Pressure (in Hg)
  Field 4 - Dew Point  (Degree F)
*/

//edit ThingSpeak.com data here...
unsigned long myChannelNumber = YourChannelNumber;
const char * myWriteAPIKey = "YourAPIkey";

////////////////********************************************************************************
////////////////*******************************SETUP********************************************
////////////////********************************************************************************
void setup(void)
{
   
     wdt_reset();
     
     Serial.begin(115200);   
     Serial.println("Starting...");
     Serial.print("NTP_Observations.ino");
     Serial.print("\n");

     pinMode(online, OUTPUT); //Set pinMode to OUTPUT for online LED

     pinMode(D2, INPUT_PULLUP); //Set input (SDA) pull-up resistor on GPIO_0 // Change this *! if you don't use a Wemos

     Wire.setClock(2000000);
     Wire.begin(D2, D1);    ///Wire.begin(0, 2); //Wire.begin(sda,scl) // Change this *! if you don't use a Wemos

     wdt_reset();

     // Connecting to WiFi network
     Serial.println();
     Serial.print("Connecting to ");
     Serial.println(ssid);

     
     
     //WiFi.config(ip,gateway,subnet);
     WiFi.begin();
     delay(10);
     
     //wifi.sta.config("Security","1048acdc7388");     
     
     //WiFi.mode(WIFI_AP); //switching to AP mode
     //WiFi.softAP(ssid, password); //initializing the AP with ssid and password. This will create a WPA2-PSK secured AP

     while (WiFi.status() != WL_CONNECTED)
     {
          delay(500);
          Serial.print(".");
     }
     Serial.println("");
     Serial.println("WiFi connected");

     wdt_reset();

     // Starting the web server
     server.begin();
     Serial.println("Web server running. Waiting for the ESP IP...");
     

     // Printing the ESP IP address
     Serial.print("Server IP:  ");
     Serial.println(WiFi.localIP());
     Serial.print("Port:  ");
     Serial.print(LISTEN_PORT);
     Serial.println("\n");

     wdt_reset();

     SPIFFS.begin();

     //SPIFFS.format();
     //SPIFFS.remove("/ACCESS.TXT");
     //SPIFFS.rename("/LOG715 TXT", "/LOG715.TXT");
     
     if (!bme.begin(BMEADDRESS))
     {
          Serial.println("Could not find a valid BME280 sensor, check wiring!");
          while (1);
     }

     //Begin listening for connection
     server.begin();

     //lcdDisplay();      //   LCD 1602 Display function --used for inital display

     ThingSpeak.begin(client);

     Serial.println("Starting UDP for NTP client");
     udp.begin(localPort);
     delayTime = 1000;
     Serial.print("Local port: ");
     Serial.println(udp.localPort());
               
     setClockWithNTP(); 
     AcquisitionDone = false;
               
     
}


// How big our line buffer should be. 100 is plenty!
#define BUFFER 100

///////////***********************************************************************************
///////////************************************LOOP*******************************************
///////////***********************************************************************************
void loop()
{

     
     
     udp.begin(localPort);
     
     getDateTime();
     
     //Serial.println("Returned to loop");
     
     wdt_reset();
     
     //Serial.println(dayofWeek);  //Check to see which dayofWeek starts is Saturday.  Saturday is 6 dayofWeek on DS3231.

     //Collect  "LOG.TXT" Data for one day; do it early (before 00:00:00) so day of week still equals 6.
     if (((hour1) == 23 )  &&
               ((minute1) == 57) &&
               ((second1) == 0))
     {
          newDay();
     }
     
     //Serial.println(dayofWeek);  //Check to see which dayofWeek starts is Saturday.  Saturday is 6 dayofWeek on DS3231.

     //Write Data at 15 minute interval

     if ((((minute1) == 0)||
               ((minute1) == 15)||
               ((minute1) == 30)||
               ((minute1) == 45))
               && ((second1) == 0))
     {

         if (AcquisitionDone == false) 
         {
              
           lastUpdate = dtStamp;   //store dtstamp for use on dynamic web page
           getWeatherData();
           updateDifference();  //Get Barometric Pressure difference
           logtoSD();   //Output to SPIFFS  --Log to SPIFFS on 15 minute interval.
           delay(1);  //Be sure there is enough SPIFFS write time
           speak();
           AcquisitionDone = true;
           setClockWithNTP();
           
         }
     }
     else
     {
         AcquisitionDone = false;
         delay(500);
         
         // Disable listen function prior to writing data to log file 
         if (!((((minute1) == 14)||
               ((minute1) == 29)||
               ((minute1) == 44)||
               ((minute1) == 59))
               && ((second1) > 50)))
          {
               listen();
          }
     }
}

//////////////
void logtoSD()   //Output to SPIFFS Card every fifthteen minutes
{

     // Open a "log.txt" for appended writing
     File log = SPIFFS.open("/LOG.TXT", "a");

     if (!log)
     {
          Serial.println("file open failed");
     }

     //log.print(id);
     //log.print(" , ");
     log.print(dtStamp) + " EST";
     log.print(" , ");
     log.print("Humidity:  ");
     log.print(bme_humidity);
     log.print(" % , ");
     log.print("Dew Point:  ");
     log.print(dew,1);
     log.print(" F. , ");
     log.print(fahrenheit);
     log.print("  F. , ");
     // Reading temperature or humidity takes about 250 milliseconds!
     // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
     log.print("Heat Index:  ");
     log.print(heat_index);
     log.print(" F. ");
     log.print(" , ");
     log.print(currentPressure, 3);   //Inches of Mecury
     log.print(" in. Hg. ");
     log.print(" , ");

     if (pastPressure == currentPressure)
     {
          log.print("0.000");
          log.print(" Difference ");
          log.print(" ,");
     }
     else
     {
          log.print((difference),3);
          log.print(" Difference ");
          log.print(", ");
     }

     log.print(millibars, 2);  //Convert hPA to millibars  
     log.print(" millibars ");
     log.print(" , ");
     log.print(atm, 3);   
     log.print(" Atm ");
     log.print(" , ");
     log.print(bme_altitude, 1);  //Convert meters to Feet
     log.print(" Ft. ");
     log.println();
     //Increment Record ID number
     //id++;
     Serial.print("\n");
     Serial.println("Data written to log  " + dtStamp);
     Serial.print("\n");
     
     log.close();

     pastPressure = currentPressure;

     if(abs(difference) >= .020)   //After testing and observations of Data; raised from .010 to .020 inches of Mecury
     {
          // Open a "Differ.txt" for appended writing --records Barometric Pressure change difference and time stamps
          File diff = SPIFFS.open("/DIFFER.TXT", "a");

          if (!diff)
          {
               Serial.println("file open failed");
          }

          Serial.println("\n");
          Serial.println("Difference greater than .020 inches of Mecury ,  ");
          Serial.print(difference, 3);
          Serial.println("\n");
          Serial.print("  ,");
          Serial.print(dtStamp);
          Serial.println("");

          diff.println("");
          diff.print("Difference greater than .020 inches of Mecury,  ");
          diff.print(difference, 3);
          diff.print("  ,");
          diff.print(dtStamp);
          diff.close();

          beep(50);  //Duration of Sonalert tone 

     }
     
}

/*
  /////////////////***********************************************
  void lcdDisplay()   //   LCD 1602 Display function
  {

  lcd.clear();
  // set up the LCD's number of rows and columns:
  lcd.backlight();
  lcd.noAutoscroll();
  lcd.setCursor(0, 0);
  // Print Barometric Pressure
  lcd.print((bme280_pressure *  0.00029530),3);   //Convert hPa to inches mercury
  lcd.print(" in. Hg.");
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);
  // print millibars
  lcd.print(((Pressure) * .01),3);   //Convert  hPa to millibars
  lcd.print(" mb.    ");
  lcd.print("");

  }
*/

/////////////
void listen()   // Listen for client connection
{
          
     client = server.available();

     while(client.available())
     {
          if (client)
          {

               // Process this request until it completes or times out.
               // Note that this is explicitly limited to handling one request at a time!

               // Clear the incoming data buffer and point to the beginning of it.
               bufindex = 0;
               memset(&buffer, 0, sizeof(buffer));

               // Clear action and path strings.
               memset(&action, 0, sizeof(action));
               memset(&path,   0, sizeof(path));

               // Set a timeout for reading all the incoming data.
               unsigned long endtime = millis() + TIMEOUT_MS;

               // Read all the incoming data until it can be parsed or the timeout expires.
               bool parsed = false;

               while (!parsed && (millis() < endtime) && (bufindex < BUFFER_SIZE))
               {

                    if (client.available())
                    {

                         buffer[bufindex++] = client.read();

                    }

                    parsed = parseRequest(buffer, bufindex, action, path);

               }

               if(parsed)
               {

                    // Check the action to see if it was a GET request.
                    if (strcmp(action, "GET") == 0)
                    {

                         digitalWrite(online,HIGH);   //turn on online LED indicator

                         String ip1String = "10.0.0.146";   //Host ip address
                         String ip2String = client.remoteIP().toString();   //client remote IP address
                         
                         Serial.print("\n");
                         Serial.println("Client connected:  " + dtStamp);
                         Serial.print("Client IP:  ");
                         Serial.println(ip2String);
                         Serial.println(F("Processing request"));
                         Serial.print(F("Action: "));
                         Serial.println(action);
                         Serial.print(F("Path: "));
                         Serial.println(path);

                         accessLog();   //log-on details for "GET" HTTP request


                         // Check the action to see if it was a GET request.
                         if (strncmp(path, "/favicon.ico", 12) == 0)   // Respond with the path that was accessed.
                         {
                              char *filename = "/FAVICON.ICO";
                              strcpy(MyBuffer, filename);

                              // send a standard http response header
                              client.println("HTTP/1.1 200 OK");
                              client.println("Content-Type: image/x-icon");
                              client.println();

                              wdt_reset();
                              
                              readFile();

                         }
                         // Check the action to see if it was a GET request.
                         if (strncmp(path, "/Weather", 8) == 0)   // Respond with the path that was accessed.
                         {


                              getWeatherData();

                              delay(200);

                              // First send the success response code.
                              client.println("HTTP/1.1 200 OK");
                              client.println("Content-Type: html");
                              client.println("Connnection: close");
                              client.println("Server: WEMOS D1 R2");
                              // Send an empty line to signal start of body.
                              client.println("");
                              // Now send the response data.
                              // output dynamic webpage
                              client.println("<!DOCTYPE HTML>");
                              client.println("<html>\r\n");
                              client.println("<body>\r\n");
                              client.println("<head><title>Weather Observations</title></head>");
                              // add a meta refresh tag, so the browser pulls again every 15 seconds:
                              //client.println("<meta http-equiv=\"refresh\" content=\"15\">");
                              client.println("<h2>Treyburn Lakes</h2><br />");
                              client.println("Indianapolis, IN 46239<br />");

                              if(lastUpdate != NULL)
                              {
                                   client.println("Last Update:  ");
                                   client.println(lastUpdate);
                                   client.println(" EST <br />");
                              }

                              client.println("Humidity:  ");
                              client.print(bme_humidity, 1);
                              client.print(" %<br />");
                              client.println("Dew point:  ");
                              client.print(dew, 1);
                              client.print(" F. <br />");
                              client.println("Temperature:  ");
                              client.print(fahrenheit, 1);
                              client.print(" F.<br />");
                              client.println("Heat Index:  ");
                              client.print(heat_index);
                              client.print(" F. <br />");
                              client.println("Barometric Pressure:  ");
                              client.print(currentPressure, 3);   //Inches of Mercury
                              client.print(" in. Hg.<br />");

                              if (pastPressure == currentPressure)
                              {
                                   client.println(difference, 3);
                                   client.print(" in. Hg --Difference since last update <br />");
                              }
                              else
                              {
                                   client.println(difference, 3);
                                   client.print(" in. Hg --Difference since last update <br />");
                              }

                              client.println("Barometric Pressure:  ");
                              client.println(millibars, 1);   //Convert hPA to millbars
                              client.println(" mb.<br />");
                              client.println("Atmosphere:  ");
                              client.print(atm, 2);   
                              client.print(" Atm <br />");
                              client.println("Altitude:  ");
                              client.print(bme_altitude, 1);  //Convert meters to Feet
                              client.print(" Feet<br />");
                              //client.println("<br />");
                              client.println("<h2>Weather Observations</h2>");
                              client.println("<h3>" + dtStamp + "  EST </h3>\r\n");
                              ///////////////////////////////////////////////////////////////////////////////////////////////////////////
                              //replace xxx.xxx.xxx.xxxx:yyyy with your Public IP and Forwarded port  --Caution --know the risk ---
                              ///////////////////////////////////////////////////////////////////////////////////////////////////////////
                              client.println("<a href=http://xxx.xxx.xxx.xxx:yyyy/LOG.TXT download>Current Week Observations</a><br />");
                              client.println("<br />\r\n");
                              client.println("<a href=http://xxx.xxx.xxx.xxx:yyyy/SdBrowse >Collected Observations</a><br />");
                              client.println("<br />\r\n");
                              client.println("<a href=http://xxx.xxx.xxx.xxx:yyyy/Graphs >Graphed Weather Observations</a><br />");
                              client.println("<br />\r\n");
                              client.println("<a href=http://xxx.xxx.xxx.xxx:yyyy/README.TXT download>Server:  README</a><br />");
                              client.println("<br />\r\n");
                              //client.print("<H2>Client IP:  <H2>");
                              //client.print(client.remoteIP().toString().c_str());
                              client.println("<body />\r\n");
                              client.println("<br />\r\n");
                              client.println("</html>\r\n");

                         }
                         // Check the action to see if it was a GET request.
                         else if (strcmp(path, "/SdBrowse") == 0)   // Respond with the path that was accessed.
                         {
                              
                              // send a standard http response header
                              client.println("HTTP/1.1 200 OK");
                              client.println("Content-Type: text/html");
                              client.println();
                              client.println("<!DOCTYPE HTML>");
                              client.println("<html>\r\n");
                              client.println("<body>\r\n");
                              client.println("<head><title>SDBrowse</title><head />");
                              // print all the files, use a helper to keep it clean
                              client.println("<h2>Collected Observations</h2>");

                              //////////////// Code to listFiles from martinayotte of the "ESP8266 Community Forum" ///////////////
                              String str = String("<html><head></head>\r\n");

                              if (!SPIFFS.begin())
                              {
                                   Serial.println("SPIFFS failed to mount !\r\n");
                              }
                              Dir dir = SPIFFS.openDir("/");
                              while (dir.next())
                              {
                                   str += "<a href=\"";
                                   str += dir.fileName();
                                   str += "\">";
                                   str += dir.fileName();
                                   str += "</a>";
                                   str += "    ";
                                   str += dir.fileSize();
                                   str += "<br>\r\n";
                              }
                              str += "</body></html>\r\n";

                              client.print(str);

                              ////////////////// End code by martinayotte //////////////////////////////////////////////////////
                              client.println("<br /><br />\r\n");
                              client.println("\n<a href=http://xxx.xxx.xxx.xxx:yyyy/Weather    >Current Observations</a><br />");
                              client.println("<br />\r\n");
                              client.println("<body />\r\n");
                              client.println("<br />\r\n");
                              client.println("</html>\r\n");

                         }
                         // Check the action to see if it was a GET request.
                         else if (strcmp(path, "/Graphs") == 0)   // Respond with the path that was accessed.
                         {
                              
                              // First send the success response code.
                              client.println("HTTP/1.1 200 OK");
                              client.println("Content-Type: html");
                              client.println("Connnection: close");
                              client.println("Server: WEMOS D1 R2");
                              // Send an empty line to signal start of body.
                              client.println("");
                              // Now send the response data.
                              // output dynamic webpage
                              client.println("<!DOCTYPE HTML>");
                              client.println("<html>\r\n");
                              client.println("<body>\r\n");
                              client.println("<head><title>Graphed Weather Observations</title></head>");
                              // add a meta refresh tag, so the browser pulls again every 15 seconds:
                              //client.println("<meta http-equiv=\"refresh\" content=\"15\">");
                              client.println("<br />\r\n");
                              client.println("<h2>Graphed Weather Observations</h2><br />");
                              //client.println("<framset>\r\n");
                              client.println("<frameset rows='30%,70%' cols='33%,34%'>");
                              client.println("<iframe width='450' height='260' 'border: 1px solid #cccccc;' src='https://thingspeak.com/channels/290421/charts/1?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=60&timescale=15&title=Temperature&type=line&xaxis=Date&yaxis=Degrees'></iframe>");
                              client.println("<iframe width='450' height='260' 'border: 1px solid #cccccc;' src='https://thingspeak.com/channels/290421/charts/2?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=60&timescale=15&title=Humidity&type=line&xaxis=Date&yaxis=Humidity'></iframe>");
                              client.println("<p>");
                              client.println("<iframe width='450' height='260' 'border: 1px solid #cccccc;' src='https://thingspeak.com/channels/290421/charts/3?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=60&timescale=15&title=Barometric+Pressure&type=line&xaxis=Date&yaxis=Pressure'></iframe>");
                              client.println("<iframe width='450' height='260' 'border: 1px solid #cccccc;' src='https://thingspeak.com/channels/290421/charts/4?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=60&timescale=15&title=Dew+Point&type=line'></iframe>");
                              client.println("</frameset>");
                              client.println("<br /><br />\r\n");
                              client.println("\n<a href=http://xxx.xxx.xxx.xxx:yyyy/Weather    >Current Observations</a><br />");
                              client.println("<br />\r\n");
                              client.println("</p>");
                              client.println("<body />\r\n");
                              client.println("<br />\r\n");
                              client.println("</html>\r\n");
                              
                         }
                         else if((strncmp(path, "/LOG", 4) == 0) ||  (strcmp(path, "/ACCESS.TXT") == 0) || (strcmp(path, "/DIFFER.TXT") == 0)|| (strcmp(path, "/SERVER.TXT") == 0) || (strcmp(path, "/README.TXT") == 0))     // Respond with the path that was accessed.
                         {

                              char *filename;
                              char name;
                              strcpy( MyBuffer, path );
                              filename = &MyBuffer[1];

                              if ((strncmp(path, "/SYSTEM~1", 9) == 0) || (strncmp(path, "/ACCESS", 7) == 0))
                              {

                                   client.println("HTTP/1.1 404 Not Found");
                                   client.println("Content-Type: text/html");
                                   client.println();
                                   client.println("<h2>404</h2>\r\n");
                                   delay(250);
                                   client.println("<h2>File Not Found!</h2>\r\n");
                                   client.println("<br /><br />\r\n");
                                   client.println("\n<a href=http://xxx.xxx.xxx.xxx:yyyy/SdBrowse    >Return to SPIFFS files list</a><br />");
                              }
                              else
                              {

                                   client.println("HTTP/1.1 200 OK");
                                   client.println("Content-Type: text/plain");
                                   client.println("Content-Disposition: attachment");
                                   client.println("Content-Length:");
                                   client.println("Connnection: close");
                                   client.println();


                                   readFile();

                              }

                         }
                         // Check the action to see if it was a GET request.
                         else  if(strncmp(path, "/Grey111", 7) == 0)   //replace "zzzzzzz" with your choice.  Makes "ACCESS.TXT" a restricted file.
                         {
                              //Restricted file:  "ACCESS.TXT."  Attempted access from "Server Files:" results in
                              //404 File not Found!

                              char *filename = "/ACCESS.TXT";
                              strcpy(MyBuffer, filename);

                              // send a standard http response header
                              client.println("HTTP/1.1 200 OK");
                              client.println("Content-Type: text/plain");
                              client.println("Content-Disposition: attachment");
                              //client.println("Content-Length:");
                              client.println();

                              wdt_reset();
                              
                              readFile();
                         }
                         else
                         {

                              delay(1000);

                              // everything else is a 404
                              client.println("HTTP/1.1 404 Not Found");  
                              client.println("Content-Type: text/html");
                              client.println();
                              client.println("<h2>404</h2>\r\n");
                              delay(250);
                              client.println("<h2>File Not Found!</h2>\r\n");
                              client.println("<br /><br />\r\n");
                              client.println("\n<a href=http://xxx.xxx.xxx.xxx:yyyy/SdBrowse    >Return to SPIFFS files list</a><br />");
                         }
                         exit;
                    }
                    else
                    {
                         // Unsupported action, respond with an HTTP 405 method not allowed error.
                         
                         
                         client.println("HTTP/1.1 405 Method Not Allowed");
                         client.println("");
                         delay(1000);
                         Serial.println("");
                         Serial.println("Http/1.1 405 issued");
                         Serial.println("\n");

                         digitalWrite(online, HIGH);   //turn-on online LED indicator

                         accessLog();   //log for, log-on details for "Method Not Allowed," HTTP request

                    }
               }
               // Wait a short period to make sure the response had time to send before
               // the connection is closed .

               delay(100);

               //Client flush buffers
               client.flush();
               // Close the connection when done.
               client.stop();

               digitalWrite(online, LOW);   //turn-off online LED indicator

               Serial.println("Client closed");
                              
               setClockWithNTP();
               AcquisitionDone = false;
               
               delay(500);

          }

     }
     

     
}


//********************************************************************
//////////////////////////////////////////////////////////////////////
// Return true if the buffer contains an HTTP request.  Also returns the request
// path and action strings if the request was parsed.  This does not attempt to
// parse any HTTP headers because there really isn't enough memory to process
// them all.
// HTTP request looks like:
//  [method] [path] [version] \r\n
//  Header_key_1: Header_value_1 \r\n
//  ...
//  Header_key_n: Header_value_n \r\n
//  \r\n
bool parseRequest(uint8_t* buf, int bufSize, char* action, char* path)
{
     // Check if the request ends with \r\n to signal end of first line.
     if (bufSize < 2)
          return false;

     if (buf[bufSize - 2] == '\r' && buf[bufSize - 1] == '\n')
     {
          parseFirstLine((char*)buf, action, path);
          return true;
     }
     return false;
}


/////////////////////////////////////////////////////////**************************
// Parse the action and path from the first line of an HTTP request.
void parseFirstLine(char* line, char* action, char* path)
{
     // Parse first word up to whitespace as action.
     char* lineaction = strtok(line, " ");

     if (lineaction != NULL)

          strncpy(action, lineaction, MAX_ACTION);
     // Parse second word up to whitespace as path.
     char* linepath = strtok(NULL, " ");

     if (linepath != NULL)

          strncpy(path, linepath, MAX_PATH);
}

/////////////////
void accessLog()
{
     getDateTime();
      
     String ip1String = "10.0.0.146";   //Host ip address
     String ip2String = client.remoteIP().toString();   //client remote IP address

     //Open a "access.txt" for appended writing.   Client access ip address logged.
     File logFile = SPIFFS.open("/ACCESS.TXT", "a");

     if (!logFile)
     {
          Serial.println("File failed to open");
     }

     if ((ip1String == ip2String) || (ip2String == "0.0.0.0"))
     {

          //Serial.println("IP Address match");
          logFile.close();

     }
     else
     {
          //Serial.println("IP address that do not match ->log client ip address");
         
          logFile.print("Accessed:  ");
          logFile.print(dtStamp + " -- ");
          logFile.print("Client IP:  ");
          logFile.print(client.remoteIP());
          logFile.print(" -- ");
          logFile.print("Action:  ");
          logFile.print(action);
          logFile.print(" -- ");
          logFile.print("Path:  ");
          logFile.print(path);
          logFile.println("");          
          logFile.close();
          
          
     }
}


///////////////********************************************************************
void readFile()
{

     digitalWrite(online, HIGH);   //turn-on online LED indicator

     String filename = (const char *)&MyBuffer;

     Serial.print("File:  ");
     Serial.println(filename);

     File webFile = SPIFFS.open(filename, "r");

     if (!webFile)
     {
          Serial.println("File failed to open");
          Serial.println("\n");
     }
     else
     {
          char buf[1024];
          int siz = webFile.size();
          //webserver.setContentLength(str.length() + siz);
          //webserver.send(200, "text/plain", str);
          while (siz > 0)
          {
               size_t len = std::min((int)(sizeof(buf) - 1), siz);
               webFile.read((uint8_t *)buf, len);
               client.write((const char*)buf, len);
               siz -= len;
          }
          webFile.close();
     }

     delayTime = 1000;

     MyBuffer[0] = '\0';

     digitalWrite(online, LOW);   //turn-off online LED indicator

     loop();
}

/////////////////////////////////********************************************
void ReadDateTime()
{

     //delay(1000);

     DateTime now = Clock.now();
     utc = now.unixtime();
     local = myTZ.toLocal(utc, &tcr);
     now = local;

     second1 = now.second();
     minute1 = now.minute();
     hour1 = now.hour();      //24h format
     date1 = now.day();
     month1 = now.month();
     year1 = now.year();
     dayofWeek = now.dayOfTheWeek();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  DS1307 Date and Time Stamping  Orginal function by
//  Bernhard    http://www.backyardaquaponics.com/forum/viewtopic.php?f=50&t=15687
//  Modified by Tech500 to use RTCLib
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
String getDateTime()
{

     ReadDateTime();

     int temp;

     temp = (month1);
     if (temp < 10)
     {
          SMonth = ("0" + (String)temp);
     }
     else
     {
          SMonth = (String)temp;
     }

     temp = (date1);
     if (temp < 10)
     {
          SDay = ("0" + (String)temp);
     }
     else
     {
          SDay = (String)temp;
     }

     SYear = (String)(year1);

     temp = (hour1);
     if (temp < 10)
     {
          SHour = ("0" + (String)temp);
     }
     else
     {
          SHour = (String)temp;
     }

     temp = (minute1);  
     if (temp < 10)
     {
          SMin = ("0" + (String)temp);
     }
     else
     {
          SMin = (String)temp;
     }

     temp = (second1);
     if (temp < 10)
     {
          SSec = ("0" + (String)temp);
     }
     else
     {
          SSec = (String)temp;
     }

     dtStamp = SMonth + '/' + SDay + '/' + SYear + " , " + SHour + ':' + SMin + ':' + SSec;

     return (dtStamp);
}

/////////////////////
void getWeatherData()
{

     bme_temp     = bme.readTemperature();        // temperature  is read in degrees Celsius

     delay(1000);

     bme_pressure = bme.readPressure();   //pressure is read in Pascals

     fahrenheit = ((bme_temp * 1.8 + 32) - 2.3);   //convert Celsius to Fahrenheit --subtract correction factor
     currentPressure = ((bme_pressure * 0.0002953) + .877);   //convert Pascals to inches mercury --add correction factor
     millibars = ((bme_pressure * .01) + 29.7);   //convert Pascals to millibars  --plus correction factor

     bme_altitude = ((bme.readAltitude(SEALEVELPRESSURE_HPA) * 3.28084) + 24.3 );   //altitude is read in meters --conversion to feet --plus correction factor

     bme_humidity = bme.readHumidity();

     delayTime = 5000;

     RHx = bme_humidity;   // Short form of RH for inclusion in the equation makes it easier to read
     heat_index = -42.379 + (2.04901523 * fahrenheit) + (10.14333127*RHx) - (0.22475541 *fahrenheit * RHx) - (0.00683783 * sq(fahrenheit)) - (0.05481717 * sq(RHx)) + (0.00122874 * sq(fahrenheit) * RHx) + (0.00085282 * fahrenheit * sq(RHx)) - (0.00000199 * sq(fahrenheit) * sq(RHx));
     if ((bme_temp <= 26.66) || (bme_humidity <= 40)) heat_index = bme_temp * 1.8 + 32; // The convention is not to report heat Index when temperature is < 26.6 Deg-C or humidity < 40%
     dew_point = (243.04 * (log(bme_humidity/100) + ((17.625 * bme_temp)/(243.04+bme_temp))) / (17.625-log(bme_humidity/100) - ((17.625 * bme_temp) / (243.04+bme_temp))));
     dew = (((dew_point) * 1.8 +32)- 2.3);   // correction factor - 1.0 degrees Fahrenheit

     atm = bme_pressure * 0.00000986923267;   //Convert Pascals to Atm (atmospheric pressure)
}

////////////////////////
float updateDifference()  //Pressure difference for fifthteen minute interval
{

     //Function to find difference in Barometric Pressure
     //First loop pass pastPressure and currentPressure are equal resulting in an incorrect difference result.
     //Future loop passes difference results are correct

     difference = currentPressure - pastPressure;  //This will be pressure from this pass thru loop, pressure1 will be new pressure reading next loop pass
     if (difference == currentPressure)
     {
          difference = 0;
     }

     return (difference, 3); //Barometric pressure change in inches of Mecury

}

////////////////////////////////
void beep(unsigned char delayms) 
{

     // wait for a delayms ms
     digitalWrite(sonalert, HIGH);
     delay(3000);
     digitalWrite(sonalert, LOW);

}

//////////////////
void speak()
{

     char t_buffered1[14];
     dtostrf(fahrenheit, 7, 2, t_buffered1);

     char t_buffered2[14];
     dtostrf(bme_humidity, 7, 2, t_buffered2);

     char t_buffered3[14];
     dtostrf(currentPressure, 7, 2, t_buffered3);

     char t_buffered4[14];
     dtostrf(dew, 7, 2, t_buffered4);

     // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
     // pieces of information in a channel.  Here, we write to field 1.
     // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
     // pieces of information in a channel.  Here, we write to field 1.
     ThingSpeak.setField(1, t_buffered1);  //Temperature
     ThingSpeak.setField(2, t_buffered2);  //Humidity
     ThingSpeak.setField(3, t_buffered3);  //Barometric Pressure
     ThingSpeak.setField(4, t_buffered4);  //Dew Point F.

     // Write the fields that you've set all at once.
     ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

     Serial.println("Sent data to Thingspeak.com\n");
     
}     

/////////////
void newDay()   //Collect Data for twenty-four hours; then start a new day
{

     //Do file maintence on 1st day of week at appointed time from RTC.  Assign new name to "log.txt."  Create new "log.txt."
     if ((dayofWeek) == 6)
     {
          fileStore();
     }

     //id = 1;   //Reset id for start of new day
     //Write log Header

     // Open file for appended writing
     File log = SPIFFS.open("/LOG.TXT", "a");

     if (!log)
     {
          Serial.println("file open failed"); 
     }
     else
     {
          delay(1000);
          log.println(", , , , , ,"); //Just a leading blank line, in case there was previous data
          log.println("Date, Time, Humidity, Dew Point, Temperature, Heat Index, in. Hg., Difference, millibars, Atm, Altitude");
          log.close();
          Serial.println("");
          Serial.println("Date, Time, Humidity, Dew Point, Temperature, Heat Index, in. Hg., Difference, millibars, Atm, Altitude");
     }    Serial.println("\n");

}

////////////////*************************
void fileStore()   //If 6th day of week, rename "log.txt" to ("log" + month + day + ".txt") and create new, empty "log.txt"
{

     // Open file for appended writing
     File log = SPIFFS.open("/LOG.TXT", "a");

     if (!log)
     {
          Serial.println("file open failed");
     }

     // rename the file "LOG.TXT"
     String logname;
     logname = "/LOG";
     logname += month1; ////logname += Clock.getMonth(Century);
     logname += date1;   ///logname += Clock.getDate();
     logname += ".TXT";
     SPIFFS.rename("/LOG.TXT", logname.c_str());
     log.close();

     //For troubleshooting
     //Serial.println(logname.c_str());


}


//************************************************************************
// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
     Serial.println("sending NTP packet...");
     // set all bytes in the buffer to 0
     memset(packetBuffer, 0, NTP_PACKET_SIZE);
     // Initialize values needed to form NTP request
     // (see URL above for details on the packets)
     packetBuffer[0] = 0b11100011;   // LI, Version, Mode
     packetBuffer[1] = 0;     // Stratum, or type of clock
     packetBuffer[2] = 6;     // Polling Interval
     packetBuffer[3] = 0xEC;  // Peer Clock Precision
     // 8 bytes of zero for Root Delay & Root Dispersion
     packetBuffer[12]  = 49;
     packetBuffer[13]  = 0x4E;
     packetBuffer[14]  = 49;
     packetBuffer[15]  = 52;

     // all NTP fields have been given values, now
     // you can send a packet requesting a timestamp:
     udp.beginPacket(address, 123); //NTP requests are to port 123
     udp.write(packetBuffer, NTP_PACKET_SIZE);
     udp.endPacket(); 
}


//************************************************************************
void setClockWithNTP()   
{
     WiFi.hostByName(ntpServerName, timeServerIP);

     sendNTPpacket(timeServerIP); // send an NTP packet to a time server
     // wait to see if a reply is available
     delay(1000);

     int cb = udp.parsePacket();
     
     if (cb != 48)   // was if(!cb)  --testing cb != 48  --Packet length of 48 known good.
     {
          Serial.println("no packet yet");

     }
     else 
     {
          Serial.print("packet received, length = "); 
          Serial.println(cb);
          // We've received a packet, read the data from it
          udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

          //the timestamp starts at byte 40 of the received packet and is four bytes,
          // or two words, long. First, esxtract the two words:

          unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
          unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
          // combine the four bytes (two words) into a long integer
          // this is NTP time (seconds since Jan 1 1900):
          unsigned long secsSince1900 = highWord << 16 | lowWord;
          Serial.print("Seconds since Jan 1 1900 = " );
          Serial.println(secsSince1900);

          // now convert NTP time into everyday time:
          Serial.print("Unix time = ");
          // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
          const unsigned long seventyYears = 2208988800UL;
          // subtract seventy years:
          epoch = secsSince1900 - seventyYears;
          // print Unix time:
          Serial.println(epoch);

          // following line sets the RTC to the date & time this sketch was compiled
          Clock.begin(DateTime(epoch));

          AcquisitionDone == false;

          cb = 0;

          getDateTime();
          delayTime = 1000;
          Serial.println(dtStamp);
          Serial.print("\n");

          // following line sets the RTC to the date & time this sketch was compiled
          //Clock.begin(DateTime(epoch));
          
     }
}