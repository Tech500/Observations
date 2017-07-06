//////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                       Internet Weather Datalogger and Dynamic Web Server --ESP8266
//                       Added sending of FAVICON.ICO   07/05/2017 @ 13:15 EST --ab9nq.william--at-gmail.com
//
//
//                       Supports WeMos D1 R2 Revison 2.1.0,   --ESP8266EX Based Developement Board
//
//                       https://www.wemos.cc/product/d1.html
//
//                       listFiles and readFile by martinayotte of ESP8266 Community Forum
//
//                       Observations_SPIFFS.ino  by tech500
//                       Previous project:  "SdBrose_CC3000_HTTPServer.ino" by tech500" on https://github.com/tech500
//
//                       Project is Open-Source uses one RTC, DS3231 and one Barometric Pressure sensor, BME280;
//                       project cost less than $15.00
//
//   					 New --online LED indicator
///////////////////////////////////////////////////////////////////////////////////////////////////////
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
#include <DS3231.h>   //http://misclab.umeoce.maine.edu/boss/Arduino/bensguides/DS3231_Arduino_Clock_Instructions.pdf
#include <ESP8266WiFi.h>   //Part of ESP8266 Board Manager install
#include <FS.h>   //Part of ESP8266-Arduino Core
#include <Adafruit_Sensor.h>   //https://github.com/adafruit/Adafruit_Sensor
#include <Adafruit_BME280.h>   //https://github.com/adafruit/Adafruit_BME280_Library
//#include <LiquidCrystal_I2C.h>   //https://github.com/esp8266/Basic/tree/master/libraries/LiquidCrystal
#include <ThingSpeak.h>   //https://github.com/mathworks/thingspeak-arduino
#include "SPI.h"   //Part of Arduino Library Manager

// Replace with your network details
const char* ssid = "YourSSID";
const char* password = "YourPassword";

float bme_pressure, bme_temp, bme_humidity, RHx, T, heat_index, dp, dew_point, bme_altitude;

int count = 0;

Adafruit_BME280 bme; // Note Adafruit assumes I2C adress = 0x77 my module (eBay) uses 0x76 so the library address has been changed.

unsigned long delayTime;

#define SEALEVELPRESSURE_HPA (1032.4)   //Current millbars at Sea Level National Weather Station

//Real Time Clock used  DS3231
DS3231 Clock;

bool Century=false;
bool h12;
bool PM;


int year, month, date, hour, minute, second;
byte DoW, dayofWeek;

//use I2Cscanner to find LCD display address, in this case 3F   //https://github.com/todbot/arduino-i2c-scanner/
//LiquidCrystal_I2C lcd(0x3F,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display

#define sonalertPin 9  //pin for Piezo buzzer

#define online D0  //pin for online LED indicator

#define BUFSIZE 64  //size of read buffer for file download  -optimized for CC3000.

float currentPressure;  //Present pressure reading used to find pressure change difference.
float pastPressure;  //Previous pressure reading used to find pressure change difference.
float milliBars;

float difference;

//long int id = 1;  //Increments record number

char *filename;
char str[] = {0};

int timer;

String temperatureF, heatindex, humidity, pressure, pressureinch, dewpoint;

String dtStamp;
String lastUpdate;
String SMonth, SDay, SYear, SHour, SMin, SSec;

String reConnect;

String fileRead;

int fileDownload;   //File download status; 1 = file download has started, 0 = file has finished downloading

char MyBuffer[13];

//#define LISTEN_PORT           8002    // What TCP port to listen on for connections.
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

uint8_t buffer[BUFFER_SIZE+1];
int bufindex = 0;
char action[MAX_ACTION+1];
char path[MAX_PATH+1];



// Web Server on port 8002
WiFiServer server(8002);
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


////////////////
void setup(void)
{

	wdt_reset();

	Serial.begin(115200);   //Removed for Wireless operation

	pinMode(online, OUTPUT); //Set input (SDA) pull-up resistor on

	pinMode(D3, INPUT_PULLUP); //Set input (SDA) pull-up resistor on
	//Wire.setClock(2000000);
	Wire.begin(D3, D4);

	/*
	     Clock.setSecond(30);//Set the second
	     Clock.setMinute(437);//Set the minute
	     Clock.setHour(12);  //Set the hour
	     Clock.setDoW(4);    //Set the day of the week
	     Clock.setDate(5);  //Set the date of the month
	     Clock.setMonth(7);  //Set the month of the year
	     Clock.setYear(17);  //Set the year (Last two digits of the year)
	*/

	wdt_reset();

// Connecting to WiFi network
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.begin(ssid, password);
	delay(10);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");

	wdt_reset();

	getDateTime();

	// Starting the web server
	//server.begin();
	Serial.println(dtStamp);
	Serial.println("Web server running. Waiting for the ESP IP...");
	delay(1000);

	// Printing the ESP IP address
	Serial.println(WiFi.localIP());

	wdt_reset();


	SPIFFS.begin();

	//SPIFFS.format();

	//SPIFFS.remove("/ACCESS.TXT");

	//pinMode(sonalertPin, OUTPUT);  //Used for Piezo buzzer

	pinMode(D3, INPUT_PULLUP); //Set input (SDA) pull-up resistor on

	if (!bme.begin()) {
		Serial.println("Could not find a valid BME280 sensor, check wiring!");
		while (1);
	}

	//Begin listening for connection
	server.begin();

	//lcdDisplay();      //   LCD 1602 Display function --used for inital display

	//Serial.end();

	ThingSpeak.begin(client);

}


// How big our line buffer should be. 100 is plenty!
#define BUFFER 100


///////////
void loop()
{


	wdt_reset();

	getDateTime();

	//Serial.println(dayofWeek);  //Check to see which dayofWeek starts is Saturday.  Saturday is 6 dayofWeek on DS3231.


	//Collect  "LOG.TXT" Data for one day; do it early (before 00:00:00) so day of week still equals 6.
	if (((hour) == 23 )  &&
	    ((minute) == 57) &&
	    ((second) == 0)) {
		newDay();
	}

	//Write Data at 15 minute interval

	if ((((minute) == 0)||
	     ((minute) == 15)||
	     ((minute) == 30)||
	     ((minute) == 45))
	    && ((second) == 0)) {


		lastUpdate = dtStamp;   //store dtstamp for use on dynamic web page

		getWeatherData();

		updateDifference();  //Get Barometric Pressure difference

		logtoSD();   //Output to SD Card  --Log to SD on 15 minute interval.

		delay(10);  //Be sure there is enough SD write time

		speak();

		//lcdDisplay();      //   LCD 1602 Display function --used for 15 minute update



	} else {
		listen();  //Listen for web client
	}

}

//////////////
void logtoSD()   //Output to SPIFFS Card every fifthteen minutes
{

	//getWeatherData();

	if(fileDownload == 1) { //File download has started
		exit;   //Skip logging this time --file download in progress
	} else {

		fileDownload = 1;

		// Open a "log.txt" for appended writing
		File log = SPIFFS.open("/LOG.TXT", "a");

		if (!log) {
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
		log.print(dew_point * 1.8 +32);
		log.print(" F. , ");
		log.print(bme_temp * 1.8 + 32);
		log.print("  F. , ");
		// Reading temperature or humidity takes about 250 milliseconds!
		// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
		log.print("Heat Index:  ");
		log.print(heat_index * 1.8 + 32);
		log.print(" F. ");
		log.print(" , ");
		log.print(currentPressure, 3);   //Inches of Mecury
		log.print(" in. Hg. ");
		log.print(" , ");

		if (pastPressure == currentPressure) {
			log.print("0.000");
			log.print(" Difference ");
			log.print(" ,");
		} else {
			log.print((difference),3);
			log.print(" Difference ");
			log.print(", ");
		}

		log.print((bme_pressure + 27.47), 2);  //Convert hPA to millibars
		log.print(" millibars ");
		log.print(" , ");
		log.print(bme_pressure * 0.00098692326671601, 3);   //Convert hPa to atm (atmospheric pressure)
		log.print(" atm ");
		log.print(" , ");
		log.print(bme_altitude, 1);  //Convert meters to Feet
		log.print(" Ft. ");
		log.println();
		//Increment Record ID number
		//id++;
		Serial.println("\nData written to log  " + dtStamp);
		Serial.println("");

		log.close();

		pastPressure = currentPressure;

		fileDownload = 0;

		if(abs(difference) >= .020) { //After testing and observations of Data; raised from .010 to .020 inches of Mecury
			// Open a "Differ.txt" for appended writing --records Barometric Pressure change difference and time stamps
			File diff = SPIFFS.open("/DIFFER.TXT", "a");

			if (!diff) {
				Serial.println("file open failed");
			}

			Serial.println("");
			Serial.print("Difference greater than .020 inches of Mecury ,  ");
			Serial.print(difference, 3);
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
	listen();
}
/*
/////////////////
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

	fileDownload = 0;   //No file being downloaded

	while(client.available()) {
		if (client) {

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

			while (!parsed && (millis() < endtime) && (bufindex < BUFFER_SIZE)) {

				if (client.available()) {

					buffer[bufindex++] = client.read();

				}

				parsed = parseRequest(buffer, bufindex, action, path);

			}

			if(parsed) {

				// Check the action to see if it was a GET request.
				if (strcmp(action, "GET") == 0) {

					digitalWrite(online,HIGH);   //turn on online LED indicator

					String ip1String = "10.0.0.146";   //Host ip address
					String ip2String = client.remoteIP().toString();   //client remote IP address

					Serial.println();
					getDateTime();
					Serial.println("Client connected:  " + dtStamp);
					Serial.print("Client IP:  ");
					Serial.println(ip2String);
					Serial.println(F("Processing request"));
					Serial.print(F("Action: "));
					Serial.println(action);
					Serial.print(F("Path: "));
					Serial.println(path);

					//Open a "access.txt" for appended writing.   Client access ip address logged.
					File logFile = SPIFFS.open("/ACCESS.TXT", "a");

					if (!logFile) {
						Serial.println("File failed to open");
					}

					if ((ip1String == ip2String) || (ip2String == "0.0.0.0")) {

						//Serial.println("IP Address match");
						logFile.close();

					} else {
						//Serial.println("IP address that do not match ->log client ip address");

						logFile.print("Accessed:  ");
						logFile.print(dtStamp + " -- ");
						logFile.print("Client IP:  ");
						logFile.print(client.remoteIP());
						logFile.print(" -- ");
						logFile.print("Path:  ");
						logFile.print(path);
						logFile.println("");
						logFile.close();
					}
					// Check the action to see if it was a GET request.
					if (strncmp(path, "/favicon.ico", 12) == 0) { // Respond with the path that was accessed.
						char *filename = "/FAVICON.ICO";
						strcpy(MyBuffer, filename);

						// send a standard http response header
						client.println("HTTP/1.1 200 OK");
						client.println("Content-Type: image/x-icon");
						client.println();

						fileDownload = 1;   //File download has started

						wdt_reset();

						readFile();

					}
					// Check the action to see if it was a GET request.
					if (strncmp(path, "/Weather", 8) == 0) { // Respond with the path that was accessed.


						getWeatherData();

						delay(200);

						fileDownload = 1;

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

						if(lastUpdate != NULL) {
							client.println("Last Update:  ");
							client.println(lastUpdate);
							client.println(" EST <br />");
						}

						client.println("Humidity:  ");
						client.print(bme_humidity, 1);
						client.print(" %<br />");
						client.println("Dew point:  ");
						client.print(dew_point * 1.8 +32, 1);
						client.print(" F. <br />");
						client.println("Temperature:  ");
						client.print(bme_temp * 1.8 + 32, 1);
						client.print(" F.<br />");
						client.println("Heat Index:  ");
						client.print(heat_index * 1.8 + 32, 1);
						client.print(" F. <br />");
						client.println("Barometric Pressure:  ");
						client.print(currentPressure, 3);   //Inches of Mercury
						client.print(" in. Hg.<br />");

						if (pastPressure == currentPressure) {
							client.println(difference, 3);
							client.print(" in. Hg --Difference since last update <br />");
						} else {
							client.println(difference, 3);
							client.print(" in. Hg --Difference since last update <br />");
						}

						client.println("Barometric Pressure:  ");
						client.println(((bme_pressure) + 27.47), 1);   //Convert hPA to millbars
						client.println(" mb.<br />");
						client.println("Atmosphere:  ");
						client.print(bme_pressure * 0.00098692326671601, 2);   //Convert hPa to Atm (atmospheric pressure)
						client.print(" atm <br />");
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

						fileDownload = 0;

					}
					// Check the action to see if it was a GET request.
					else if (strcmp(path, "/SdBrowse") == 0) { // Respond with the path that was accessed.
						fileDownload = 1;

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

						if (!SPIFFS.begin()) {
							Serial.println("SPIFFS failed to mount !\r\n");
						}
						Dir dir = SPIFFS.openDir("/");
						while (dir.next()) {
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

						fileDownload = 0;

					}
					// Check the action to see if it was a GET request.
					else if (strcmp(path, "/Graphs") == 0) { // Respond with the path that was accessed.
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
						client.println("\n<a href=http://xxx.xxx.xxx.xxx:yyyy/Weather    >Continue</a><br />");
						client.println("<br />\r\n");
						client.println("</p>");
						client.println("<body />\r\n");
						client.println("<br />\r\n");
						client.println("</html>\r\n");

					} else if((strncmp(path, "/LOG", 4) == 0) ||  (strcmp(path, "/ACCESS.TXT") == 0) || (strcmp(path, "/DIFFER.TXT") == 0)|| (strcmp(path, "/SERVER.TXT") == 0) || (strcmp(path, "/README.TXT") == 0)) { // Respond with the path that was accessed.

						fileDownload = 1;   //File download has started; used to stop log from logging during download

						char *filename;
						char name;
						strcpy( MyBuffer, path );
						filename = &MyBuffer[1];

						if ((strncmp(path, "/SYSTEM~1", 9) == 0) || (strncmp(path, "/ACCESS", 7) == 0)) {

							client.println("HTTP/1.1 404 Not Found");
							client.println("Content-Type: text/html");
							client.println();
							client.println("<h2>404</h2>\r\n");
							delay(250);
							client.println("<h2>File Not Found!</h2>\r\n");
							client.println("<br /><br />\r\n");
							client.println("\n<a href=http://xxx.xxx.xxx.xxx:yyyy/SdBrowse    >Return to SPIFFS files list</a><br />");
						} else {

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
					//replace xxxxxxx with your choice of 7 characters or modifier "7" for number of characters used  --gives you access to file "ACCESS.TXT"
					else  if(strncmp(path, "/xxxxxx", 7) == 0) {
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

						fileDownload = 1;   //File download has started

						wdt_reset();

						readFile();
					} else {

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
				} else {
					// Unsupported action, respond with an HTTP 405 method not allowed error.
					client.println("HTTP/1.1 405 Method Not Allowed");
					client.println("");
					delay(1000);
					Serial.println("");
					Serial.println("Http/1.1 405 issued");

				}
			}
			// Wait a short period to make sure the response had time to send before
			// the connection is closed (the CC3000 sends data asyncronously).

			delay(10);

			//Client flush buffers
			client.flush();
			// Close the connection when done.
			client.stop();

			digitalWrite(online, LOW);   //turn-off online LED indicator

			Serial.println("Client closed");
			Serial.println("");



		}

	}
}

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

	if (buf[bufSize-2] == '\r' && buf[bufSize-1] == '\n') {
		parseFirstLine((char*)buf, action, path);
		return true;
	}
	return false;
}


/////////////////////////////////////////////////////////
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



///////////////
void readFile()
{

	digitalWrite(online, HIGH);   //turn-on online LED indicator

	String filename = (const char *)&MyBuffer;

	Serial.print("File:  ");
	Serial.println(filename);

	File webFile = SPIFFS.open(filename, "r");

	if (!webFile) {
		Serial.println("File failed to open");
	} else {
		char buf[1024];
		int siz = webFile.size();
		//webserver.setContentLength(str.length() + siz);
		//webserver.send(200, "text/plain", str);
		while(siz > 0) {
			size_t len = std::min((int)(sizeof(buf) - 1), siz);
			webFile.read((uint8_t *)buf, len);
			client.write((const char*)buf, len);
			siz -= len;
		}
		webFile.close();
	}

	fileDownload = 0;  //File download has finished; allow logging since download has completed

	delay(500);

	MyBuffer[0] = '\0';

	digitalWrite(online, LOW);   //turn-off online LED indicator

	loop();
}

/////////////////////////////////
void ReadDS3231()
{

	delay(1000);

	second = Clock.getSecond();
	minute = Clock.getMinute();
	hour = Clock.getHour(h12, PM);
	date = Clock.getDate();
	month = Clock.getMonth(Century);
	year = Clock.getYear();
	dayofWeek = Clock.getDoW();


}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  DS1307 Date and Time Stamping  Orginal function by
//  Bernhard    http://www.backyardaquaponics.com/forum/viewtopic.php?f=50&t=15687
//  Modified by Tech500 to use RTCLib
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
String getDateTime()
{

	ReadDS3231();

	int temp;

	temp = (month);
	if (temp < 10) {
		SMonth = ("0" + (String)temp);
	} else {
		SMonth = (String)temp;
	}

	temp = (date);
	if (temp < 10) {
		SDay = ("0" + (String)temp);
	} else {
		SDay = (String)temp;
	}

	SYear = (String)(year);

	temp = (hour);
	if (temp < 10) {
		SHour = ("0" + (String)temp);
	} else {
		SHour = (String)temp;
	}

	temp = (minute);
	if (temp < 10) {
		SMin = ("0" + (String)temp);
	} else {
		SMin = (String)temp;
	}

	temp = (second);
	if (temp < 10) {
		SSec = ("0" + (String)temp);
	} else {
		SSec = (String)temp;
	}

	dtStamp = SMonth + '/' + SDay + '/' + SYear + " , " + SHour + ':' + SMin + ':' + SSec;

	return(dtStamp);
}

/////////////////////
void getWeatherData()
{

	bme_temp     = bme.readTemperature();        // No correction factor needed for this sensor

	bme_humidity = bme.readHumidity();     // Plus a correction factor for this sensor

	bme_pressure = (bme.readPressure() / 100);
	currentPressure = ((bme_pressure * 0.0295299830714) + .831);   //Convert hPa to in. HG add correction factor

	bme_altitude = ((bme.readAltitude(SEALEVELPRESSURE_HPA) * 3.28084) -285.67);   //Altitude in feet

	delayTime = 2000;

	T = (bme_temp * 9 / 5) + 32;   // Convert back to deg-F for the RH equation
	RHx = bme_humidity;   // Short form of RH for inclusion in the equation makes it easier to read
	heat_index = ((-42.379 + (2.04901523 * T) + (10.14333127*RHx) - (0.22475541 *T * RHx) - (0.00683783 * sq(T)) - (0.05481717 * sq(RHx)) + (0.00122874 * sq(T) * RHx) + (0.00085282 * T * sq(RHx)) - (0.00000199 * sq(T) * sq(RHx)) - 32) * 5/9);
	if ((bme_temp <= 26.66) || (bme_humidity <= 40)) heat_index = bme_temp; // The convention is not to report heat Index when temperature is < 26.6 Deg-C or humidity < 40%
	dew_point = (243.04 * (log(bme_humidity/100) + ((17.625 * bme_temp)/(243.04+bme_temp))) / (17.625-log(bme_humidity/100) - ((17.625 * bme_temp) / (243.04+bme_temp))));



	delay(100);

}

////////////////////////
float updateDifference()  //Pressure difference for fifthteen minute interval
{

	//Function to find difference in Barometric Pressure
	//First loop pass pastPressure and currentPressure are equal resulting in an incorrect difference result.
	//Future loop passes difference results are correct

	difference = currentPressure - pastPressure;  //This will be pressure from this pass thru loop, pressure1 will be new pressure reading next loop pass
	if (difference == currentPressure) {
		difference = 0;
	}

	return(difference, 3);  //Barometric pressure change in inches of Mecury

}

////////////////////////////////
void beep(unsigned char delayms)
{

	// wait for a delayms ms
	digitalWrite(sonalertPin, HIGH);
	delay(3000);
	digitalWrite(sonalertPin, LOW);

}

//////////////////
void speak()
{

	char t_buffered1[14];
	dtostrf(T, 7, 2, t_buffered1);

	char t_buffered2[14];
	dtostrf(bme_humidity, 7, 2, t_buffered2);

	char t_buffered3[14];
	dtostrf(currentPressure, 7, 2, t_buffered3);

	dp = (dew_point * 1.8) + 32;
	char t_buffered4[14];
	dtostrf(dp, 7, 2, t_buffered4);

	// Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
	// pieces of information in a channel.  Here, we write to field 1.
	// Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
	// pieces of information in a channel.  Here, we write to field 1.
	ThingSpeak.setField(1,t_buffered1);   //Temperature
	ThingSpeak.setField(2,t_buffered2);   //Humidity
	ThingSpeak.setField(3,t_buffered3);   //Barometric Pressure
	ThingSpeak.setField(4,t_buffered4);   //Dew Point F.

	// Write the fields that you've set all at once.
	ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

	Serial.println("Sent data to Thinkspeak.com");



}

/////////////
void newDay()   //Collect Data for twenty-four hours; then start a new day
{

	fileDownload = 1;

	//Do file maintence on 1st day of week at appointed time from RTC.  Assign new name to "log.txt."  Create new "log.txt."
	if (dayofWeek == 6) {
		fileStore();
	}

	//id = 1;   //Reset id for start of new day
	//Write log Header

	// Open file for appended writing
	File log = SPIFFS.open("/LOG.TXT", "a");

	if (!log) {
		Serial.println("file open failed");
	} else {
		delay(1000);
		log.println(", , , , , ,"); //Just a leading blank line, in case there was previous data
		log.println("Date, Time, Humidity, Dew Point, Temperature, Heat Index, in. Hg., Difference, millibars, atm, Altitude");
		log.close();
		Serial.println("");
		Serial.println("Date, Time, Humidity, Dew Point, Temperature, Heat Index, in. Hg., Difference, millibars, atm, Altitude");
	}

	fileDownload = 0;


}

////////////////
void fileStore()   //If 6th day of week, rename "log.txt" to ("log" + month + day + ".txt") and create new, empty "log.txt"
{

	// Open file for appended writing
	File log = SPIFFS.open("/LOG.TXT", "a");

	if (!log) {
		Serial.println("file open failed");
	}

	// rename the file "LOG.TXT"
	String logname;
	logname = "/LOG";
	logname += Clock.getMonth(Century);
	logname += Clock.getDate();
	logname += ".TXT";
	SPIFFS.rename("/LOG.TXT", logname.c_str());
	log.close();

	//For troubleshooting
	//Serial.println(logname.c_str());

}





