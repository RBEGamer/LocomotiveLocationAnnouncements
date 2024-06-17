// Written by Nick Gammon
// May 2012

#include <Wire.h>
#include <I2C_Anything.h>

#include <CSV_Parser.h>
#include <TinyGPSPlus.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <Arduino_MKRGPS.h>
const int ENABLE_BUTTON_INPUT = 4;
const int REANOUNCE_BUTTON_INPUT = 5;
const double GPS_POINT_NEAR = 10.0;
const double GPS_POINT_TRIGGER_DISTANCE = 2.0;
const String waypoint_list_filename = "POINTS.CSV"; //10 char lenght limit!!!! file on sd card needs to be uppercase!
const byte SLAVE_ADDRESS = 42; // I2C ADDRESS OF THE MUSIC PLAYER
const long SOUND_RESET_TIMEOUT = 10 * 1000; //sound can be replayed after x seconds
const long last_anounement_row_id = -1;



LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
TinyGPSPlus gps;
CSV_Parser cp(/*format*/ "ssss", true); // s = string, f = float true =headrr


unsigned long time_now = 0;
unsigned long time_last = 0;
long last_requested_audiofile = 100;
long last_upcomming_audiofile = 100;
double last_upcomming_distance = -1.0;

void display_upcomming_waypoint(long _audiofile_id, String _text = "",double _dist = 0.0){
  if(_audiofile_id == last_upcomming_audiofile && abs(last_upcomming_distance - _dist) < GPS_POINT_TRIGGER_DISTANCE){
    return;
  }
  _audiofile_id = last_upcomming_audiofile;
  last_upcomming_distance = _dist;

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("NXT " + String(_dist) + " " + String(_audiofile_id));
  lcd.setCursor(0,1);
  lcd.print(_text);
}
void request_sound_playback(long _audiofile_id, String _text = "" ,long _volume = 100){
  if(_audiofile_id == last_requested_audiofile){
    return;
  }
  last_requested_audiofile = _audiofile_id;
  Serial.println("--- PLAYING ---");
  Serial.println(_text);
  Serial.println(_audiofile_id);
  Serial.println("---------------");
  //SEND REQUEST TO PLAY FILE
  Wire.beginTransmission (SLAVE_ADDRESS);
  I2C_writeAnything (_volume);
  I2C_writeAnything (_audiofile_id);
  Wire.endTransmission ();

  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(_text);
}

void setup()
{
  pinMode(ENABLE_BUTTON_INPUT, INPUT_PULLUP);
  pinMode(REANOUNCE_BUTTON_INPUT, INPUT_PULLUP);
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("SETUP");
  
  // INIT SOUND PLAYBACK INTERFACE
  Wire.begin();
  request_sound_playback(-1);
  // INIT SD CARD
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(SDCARD_SS_PIN)) {
    while (1){
      Serial.println("Card failed, or not present");
      delay(1000);
    }
  }
  
  Serial.println("SD initialized.");

  // INIT GPS
  Serial.print("Initializing GPS card...");
  if (!GPS.begin(GPS_MODE_SHIELD)) {
    while (1){
      Serial.println("Failed to initialize GPS!");
      delay(1000);
    }
  }
   Serial.println("GPS initialized.");


  // LOAD CSV FILE IN
  Serial.println("CHECK CSV EXISTS");
  File csvFile = SD.open(waypoint_list_filename, FILE_READ);
  if (csvFile)
  {
    Serial.println("CHECK CSV EXISTS OK");
    csvFile.close();
  }else{
    Serial.println("CHECK CSV EXISTS FAILED");
    File dataFile = SD.open(waypoint_list_filename, FILE_WRITE);
    if (dataFile) {
      dataFile.println("audiofile,text,lat,lon");
      dataFile.println("12,KAMAC 1,50.76500041132879,6.077895003460349");
      dataFile.close();
    }
  }


  if(cp.readSDfile((waypoint_list_filename).c_str())) {
    Serial.println("FILE: announcements.csv OK");
    cp.print();
  }else{
    Serial.println("FILE: CSV LOADING FAILED");
  }
 
  // INIT LCD
  lcd.begin();
  lcd.noCursor();
  lcd.backlight();
}  // end of setup

void loop()
{

  if(!digitalRead(REANOUNCE_BUTTON_INPUT) && last_anounement_row_id >= 0 && last_anounement_row_id < cp.getRowsCount()){
    request_sound_playback(String(audiofile[last_anounement_row_id]).toInt(), String(text[last_anounement_row_id]));
    Serial.println("REANOUNCE_BUTTON_INPUT");
  }

  const char **audiofile = (const char **)cp["audiofile"];
  const char **text = (const char **)cp["text"];
  const char **lat = (const char **)cp["lat"];
  const char **lon = (const char **)cp["lon"];

  if (GPS.available()) {
    // read GPS values
    const float latitude = GPS.latitude();
    const float longitude = GPS.longitude();
    //const float altitude = GPS.altitude();
    //const float speed = GPS.speed();
    //const int satellites = GPS.satellites();

    // NOW CHECK EACH WAYPOINT
    for (int row = 0; row < cp.getRowsCount(); row++)
    {
      // Serial.println(lat[row]);
      const double point_to_check_lat = String(lat[row]).toDouble();
      const double point_to_check_lon = String(lon[row]).toDouble();
     
      const double distance_to_point = TinyGPSPlus::distanceBetween(latitude, longitude, point_to_check_lat, point_to_check_lon);
      const double course_to_point = TinyGPSPlus::courseTo(latitude, longitude, point_to_check_lat, point_to_check_lon);


      Serial.print(latitude, 7);
      Serial.print(":");
      Serial.print(longitude, 7);
      Serial.print(" => ");
      Serial.print(point_to_check_lat, 7);
      Serial.print(":");
      Serial.print(point_to_check_lon, 7);
      Serial.print(" d=");
      Serial.print(distance_to_point, 7);
      Serial.print(" h=");
      Serial.println(course_to_point, 7);

      last_anounement_row_id = row;

      // TRIGGER
      if(digitalRead(ENABLE_BUTTON_INPUT)){
        if (distance_to_point < GPS_POINT_TRIGGER_DISTANCE)
        {
          request_sound_playback(String(audiofile[row]).toInt(), String(text[row]));
        }else if(distance_to_point < GPS_POINT_NEAR){
          display_upcomming_waypoint(String(audiofile[row]).toInt(),String(text[row]), distance_to_point);
          Serial.print(audiofile[row]);
          Serial.print(":");
          Serial.println(distance_to_point, 7);
        }
      }else{
        Serial.println("ENABLE_BUTTON_INPUT DISBALED");
      }
    }
  }

  time_now = millis();
  if (time_now - time_last >= SOUND_RESET_TIMEOUT) {
    time_last = time_now;
    last_requested_audiofile = -1;
    if(!GPS.available()){
      Serial.println("waiting for gps");
      display_upcomming_waypoint(0, "Waiting for GPS", 100);
    }
  }


  



}  // end of loop
