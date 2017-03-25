# Observations
Internet Data Logger and Dynamic Web Server --ESP8266EX, utilizing SPIFFS.

March 25, 2017 Current Features "Oberservations_SPIFFS.

Requires WeMOS D1 R2 Developement Board, Real time clock (DS3231) and  Barometric Pressure, Humidity/Temperature and sensor (BME280).

Features:

 1. Real Time Clock; used for 15 minute time interval, date and time stamping and dayofweek (every 7th day, log.txt file gets renamed to keep file size manageable.       Every Saturday (7th day of week)    log.txt gets renamed in the format "logxxyy" xx being the month and yy being the date; a new log.txt is created after every file renaming.

 2. Dynamic web page of current observations for Last update time and date, Humidity, dew point, temperature, heat index, barometric pressure (both inches of Mercury and millibars.)

 3. Server files are listed as web links; clicking link prompts for "Open with/Save as." "System~1/", "Favicon.ico", and "Access" are listed; however, they are for internal use and cannot "Opened with/Save as," result of clicking link produces "404 Page not found."

 4. LOG.TXT file is appended every 15 minutes with the latest update; storing data from Dynamic web page.

 5. ACCESS.TXT restricted file that logs client IP address. 

 6. DIFFER.TXT stores the difference in Barometric Pressure for the last fifteen minutes. Only a difference of equal to or greater than .020 inches of Mercury are logged with difference, date and time.

 7. URL file names other than ones defined in the Sketch produce "404 Page not found."

 8. Audible alert from Piezo electric buzzer when there is Barometric Pressure difference of .020 inches of Mercury. I am interested in sudden drop of Barometric Pressure in a 15 minute interval.  Serve weather more likely with a sudden drop. Difference of .020 inches of Mercury point is set for my observations to log and sound audible alert; not based on any known value to be associated  with serve weather.

 9. Two-line LCD Display of Barometric Pressure, in both inches of Mercury and millibars.

Server is a WeMOS D1 R2, Model 2.1.0 Development Board -- purchased on Ebay for ~ $ 9.00 and BME280 breakout board and DS3231 breakout board ~ $ 6.00; both are required for project. Sensor is located indoors, currently.
