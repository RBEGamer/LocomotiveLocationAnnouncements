#define CSV_PARSER_DONT_IMPORT_SD
#include <CSV_Parser.h>

//#define DEBUG
//#define ENABLE_DSIPLAY
#ifdef ENABLE_DSIPLAY
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
#endif
#include <TinyGPSPlus.h>


#ifdef ENABLE_DSIPLAY
  #define SCREEN_WIDTH 128 // OLED display width, in pixels
  #define SCREEN_HEIGHT 64 // OLED display height, in pixels
  #define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);
#endif


#define GPS_SERIAL Serial
#define GPS_SERIAL_BAUD 9600

#define GPS_POINT_TRIGGER_DISTANCE 3 // m
#define GPS_POINT_NEAR 10



TinyGPSPlus gps;
CSV_Parser *cp = NULL;






#ifdef ENABLE_DSIPLAY
  #define SCREEN_WIDTH 128 // OLED display width, in pixels
  #define SCREEN_HEIGHT 64 // OLED display height, in pixels
  #define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);
#endif
//  #####################################################################
String last_anoucement = "";
void set_anouncement(String _file, String _text)
{
  if(last_anoucement == _file){
    return;
  }
  last_anoucement = _file;
  audio.stopSong();
  audio.setFileLoop(false);
  audio.connecttoFS(SD, ("/" + _file).c_str());
  Serial.println(_text);

#ifdef ENABLE_DSIPLAY
  display.clearDisplay();
  display.println(_text);
  display.display();
#endif
}


#ifdef DEBUG
  printDirectory(SD.open("/"), 0);
#endif
  // READ CSV FILE IN
  String csv_content = "";
  File csvFile = SD.open("/announcements.csv", FILE_READ);
  if (csvFile)
  {

    Serial.println("FILE: announcements.csv LOADING");
    while (csvFile.available())
    {
      csv_content += (char)csvFile.read();
    }
    csvFile.close();

    Serial.println("FILE: announcements.csv OK");
  }
  else
  {
    Serial.println("FILE: announcements.csv LOADING FAILED");
  }





  // PARSE CSV FILE
  cp = new CSV_Parser(csv_content.c_str(), /*format*/ "sssf", true); // s = string, f = float true =headrr
  cp->parseLeftover();
#ifdef DEBUG
  cp.print();
#endif

#ifdef ENABLE_DSIPLAY
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.drawPixel(10, 10, WHITE);
    display.display();
#endif



 const char **audiofile = (const char **)(*cp)["audiofile"];
  const char **text = (const char **)(*cp)["text"];
  const char **lat = (const char **)(*cp)["lat"];
  const char **lon = (const char **)(*cp)["lon"];



 while (GPS_SERIAL.available())
  {
    #ifdef DEBUG
    char c = GPS_SERIAL.read();
    gps.encode(c);
    Serial.print(c);
    #else
     gps.encode(GPS_SERIAL.read());
    #endif
  }


 // CALC POSITION
  if (gps.location.isValid())
  {

    for (int row = 0; row < cp->getRowsCount(); row++)
    {
      
      // Serial.println(lat[row]);

      const double point_to_check_lat = String(lat[row]).toDouble();
      const double point_to_check_lon = String(lon[row]).toDouble();
      Serial.println(point_to_check_lat);
      const double distance_to_point = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), point_to_check_lat, point_to_check_lon);
      const double course_to_point = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), point_to_check_lat, point_to_check_lon);

      if(distance_to_point < GPS_POINT_NEAR){
        Serial.print(audiofile[row]);
        Serial.print(":");
        Serial.println(distance_to_point);
      }else{
        Serial.print(gps.location.lat());
        Serial.print("-");
        Serial.println(gps.location.lng());
      }
      

      if (distance_to_point < GPS_POINT_TRIGGER_DISTANCE)
      {
        set_anouncement(audiofile[row], text[row]);
      }
    }
  }