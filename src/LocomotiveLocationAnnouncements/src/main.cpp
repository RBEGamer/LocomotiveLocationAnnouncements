#include "Arduino.h"
#include <SPI.h>
#include "SD.h"
#include "FS.h"
#include "Wire.h"
#include "AC101.h" //https://github.com/schreibfaul1/AC101
// #include "ES8388.h"  // https://github.com/maditnerd/es8388
#include "Audio.h" //https://github.com/schreibfaul1/ESP32-audioI2S
#include <I2C_Anything.h>

#include "helper.h"

#define START_SYSTEM_SOUND "/__start.mp3"

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

Audio audio;

const byte MY_ADDRESS = 42;
volatile boolean haveData = false;
volatile long volume;
volatile long fileid;

String last_anoucement = "";
void play_file(String _file)
{
  if (last_anoucement == _file)
  {
    return;
  }
  last_anoucement = _file;
  audio.stopSong();
  audio.setFileLoop(false);
  audio.connecttoFS(SD, ("" + _file).c_str());
  Serial.println(_file);
}

void receiveEvent(int howMany)
{
  if (howMany >= (sizeof volume) + (sizeof fileid))
  {
    I2C_readAnything(volume, &Wire1);
    I2C_readAnything(fileid, &Wire1);
    haveData = true;
  } // end if have enough data
} // end of receiveEvent
void setup()
{
  Serial.begin(115200);
  Serial.printf_P(PSTR("Free mem=%d\n"), ESP.getFreeHeap());

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

#ifdef DEBUG
  printDirectory(SD.open("/"), 0);
#endif

  Wire1.begin(MY_ADDRESS, GPIO_NUM_22, GPIO_NUM_21, 0);
  Wire1.onReceive(receiveEvent);

#ifdef START_SYSTEM_SOUND
  if (SD.exists(START_SYSTEM_SOUND))
  {
    play_file(START_SYSTEM_SOUND);
  }
#endif
}

void loop()
{
  // PLAY AUDIO
  audio.loop();

  if (haveData)
  {
    Serial.print("Received volume = ");
    Serial.println(volume);
    Serial.print("Received fileid = ");
    Serial.println(fileid);
    haveData = false;

    if (fileid >= 0 && fileid < 1000)
    {
      play_file("/" + String(fileid) + ".mp3");
    }
  } // end if haveData
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
