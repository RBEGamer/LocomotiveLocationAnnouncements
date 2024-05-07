#include "Arduino.h"
#include <SPI.h>
#include "SD.h"
#include "FS.h"
#include "Wire.h"
#include "AC101.h" //https://github.com/schreibfaul1/AC101
// #include "ES8388.h"  // https://github.com/maditnerd/es8388
#include "Audio.h" //https://github.com/schreibfaul1/ESP32-audioI2S
#define CSV_PARSER_DONT_IMPORT_SD
#include <CSV_Parser.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>

#include "helper.h"

//
#define START_SYSTEM_SOUND "/__start.mp3"
#define GPS_SERIAL Serial1
#define GPS_SERIAL_BAUD 4800
#define GPS_POINT_TRIGGER_DISTANCE 0.5 // m
// SPI GPIOs
#define SD_CS 13
#define SPI_MOSI 15
#define SPI_MISO 2
#define SPI_SCK 14

// I2S GPIOs, the names refer on AC101, AS1 Audio Kit V2.2 2379
#define I2S_DSIN 35 // pin not used
#define I2S_BCLK 27
#define I2S_LRC 26
#define I2S_MCLK 0
#define I2S_DOUT 25

// I2C GPIOs
#define IIC_CLK 32
#define IIC_DATA 33

// buttons
// #define BUTTON_2_PIN 13             // shared mit SPI_CS
#define BUTTON_3_PIN 19
#define BUTTON_4_PIN 23
#define BUTTON_5_PIN 18 // Stop
#define BUTTON_6_PIN 5  // Play

// amplifier enable
#define GPIO_PA_EN 21

// Switch S1: 1-OFF, 2-ON, 3-ON, 4-OFF, 5-OFF
static AC101 dac;              // AC101
const int DAC_VOLUME = 100;    // 0 - 100%;
const int DAC_VOLUME_AMP = 21; // 0...100

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);
Audio audio;
TinyGPSPlus gps;
CSV_Parser *cp = NULL;

// CSV_Parser cp(/*format*/"ssff", /*has_header*/ true, /*delimiter*/ ','); // FORMAT string, has_header: true
//  #####################################################################

void set_anouncement(String _file, String _text)
{
  audio.stopSong();
  audio.setFileLoop(false);
  audio.connecttoFS(SD, ("/" + _file).c_str());
  Serial.println(_text);
}

void setup()
{
  Serial.begin(115200);
  Serial.printf_P(PSTR("Free mem=%d\n"), ESP.getFreeHeap());

  GPS_SERIAL.begin(GPS_SERIAL_BAUD);

  // Wire.begin(GPIO_NUM_22, GPIO_NUM_21);
  // display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  // display.display();
  // display.drawPixel(10, 10, WHITE);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SPI.setFrequency(1000000);

  SD.begin(SD_CS);

  Serial.printf("Connect to DAC codec... ");
  while (not dac.begin(IIC_DATA, IIC_CLK))
  {
    Serial.printf("Failed!\n");
    delay(1000);
  }
  Serial.printf("DAC INIT OK\n");

  dac.SetVolumeSpeaker(DAC_VOLUME);
  dac.SetVolumeHeadphone(DAC_VOLUME);

  // Enable amplifier
  pinMode(GPIO_PA_EN, OUTPUT);
  digitalWrite(GPIO_PA_EN, HIGH);
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  audio.setVolume(int(DAC_VOLUME * 0.21)); // 0...21 so map it to 0-100%

  // PRINT SD CARD FILE INFO
  // printDirectory(SD.open("/"), 0);

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

#ifdef START_SYSTEM_SOUND
  if (SD.exists(START_SYSTEM_SOUND))
  {
    set_anouncement(START_SYSTEM_SOUND, START_SYSTEM_SOUND);
  }
#endif

  // PARSE CSV FILE
  cp = new CSV_Parser(csv_content.c_str(), /*format*/ "sssf", true); // s = string, f = float true =headrr
  cp->parseLeftover();
  //  cp.print();
  /*
    const char **audiofile = (const char**)(*cp)["audiofile"];
    const char **text = (const char**)(*cp)["text"];
    const char **lat = (const char**)(*cp)["lat"];
    const char **lon = (const char**)(*cp)["lon"];
    for(int row = 0; row < cp->getRowsCount(); row++) {
     Serial.print(audiofile[row]);
     Serial.println(lat[row]);

    }
    Serial.println("----");
   */
}

void loop()
{

  const char **audiofile = (const char **)(*cp)["audiofile"];
  const char **text = (const char **)(*cp)["text"];
  const char **lat = (const char **)(*cp)["lat"];
  const char **lon = (const char **)(*cp)["lon"];

  // PLAY AUDIO
  audio.loop();
  // CHECK GPS
  while (GPS_SERIAL.available())
  {
    gps.encode(GPS_SERIAL.read());
  }
  // CALC POSITION
  if (gps.location.isValid())
  {

    for (int row = 0; row < cp->getRowsCount(); row++)
    {
      Serial.print(audiofile[row]);
      //Serial.println(lat[row]);

      const double point_to_check_lat = String(lat[row]).toDouble();
      const double point_to_check_lon = String(lon[row]).toDouble();
      const double distance_to_point = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), point_to_check_lat, point_to_check_lon);
      const double course_to_point = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), point_to_check_lat, point_to_check_lon);

      if (distance_to_point < GPS_POINT_TRIGGER_DISTANCE)
      {
        set_anouncement(audiofile[row], text[row]);
      }
    }
  }
}

// optional
void audio_info(const char *info)
{
  Serial.print("info        ");
  Serial.println(info);
}
void audio_id3data(const char *info)
{ // id3 metadata
  Serial.print("id3data     ");
  Serial.println(info);
}
void audio_eof_mp3(const char *info)
{ // end of file
  Serial.print("eof_mp3     ");
  Serial.println(info);
}
void audio_showstation(const char *info)
{
  Serial.print("station     ");
  Serial.println(info);
}
void audio_showstreamtitle(const char *info)
{
  Serial.print("streamtitle ");
  Serial.println(info);
}
void audio_bitrate(const char *info)
{
  Serial.print("bitrate     ");
  Serial.println(info);
}
void audio_commercial(const char *info)
{ // duration in sec
  Serial.print("commercial  ");
  Serial.println(info);
}
void audio_icyurl(const char *info)
{ // homepage
  Serial.print("icyurl      ");
  Serial.println(info);
}
void audio_lasthost(const char *info)
{ // stream URL played
  Serial.print("lasthost    ");
  Serial.println(info);
}
void audio_eof_speech(const char *info)
{
  Serial.print("eof_speech  ");
  Serial.println(info);
}
