#include <SPI.h>
#include <SdFat.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 1
/*
  Change the value of SD_CS_PIN if you are using SPI and
  your hardware does not use the default value, SS.
  Common values are:
  Arduino Ethernet shield: pin 4
  Sparkfun SD shield: pin 8
  Adafruit SD shields and modules: pin 10
*/

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(50)

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif  ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS

#if SD_FAT_TYPE == 0
SdFat sd;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile file;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

#define error(s) sd.errorHalt(&Serial, F(s))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


// датчики -->
OneWire oneWire(4);//Создаем объект oneWire, 4 - номер контакта, к которому подключены датчики
DallasTemperature sensors(&oneWire);//Создаем объект sensors
int numer = 1; //Номер измерения
uint8_t deviceCount;//Объявляем переменную для хранения количества найденных датчиков
DeviceAddress addr;//Объявляем массив для хранения 64-битного адреса датчика
// датчики <--

#define MAX_SENSORS 10

// измерительное
long int  lastMeasure = 0,
          firstMeasure = 0,
          sleepMillis = 0;

const int chipSelect = 10;

uint64_t savedSensors[MAX_SENSORS];
uint8_t  savedCount = 0;
uint8_t  index2saved[MAX_SENSORS];

int MEASURE_PERIOD = 15;
char COL_SEP = '\t', DEC_POINT=',';

char curcsv[15];

#define  CONFNAME  "sensor.cfg"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() 
{
  char s[200];

  Serial.begin(9600);
  
  // Initialize the SD.
  if (!sd.begin(SD_CONFIG)) {
   // Serial.println("CARD ERROR!!!!");
    delay(1000);
  }
  else
  {
  //  Serial.println("READ CONFIG!!!!");
    delay(1000);
    readConfig();
  }
  delay(100);
  
  ////////////////////////// стартуем градусники, сопоставляем, добавляем новые ///////////////////////////
  sensors.begin();//Инициализация датчиков. По умолчанию для них устанавливается максимальное разрешение - 12бит
  deviceCount = sensors.getDeviceCount();//сохраняем количество найденных на шине датчиков температуры
  DeviceAddress devaddr;

  int si, idx;
  for(si=0;si<MAX_SENSORS; si++)
    index2saved[si]=255;
    
  Serial.println("SENSORS MATCHING");

      
  for (si = 0; si < deviceCount; si++)
  {
    sensors.getAddress(devaddr, si);
    idx = getSensorIndex(devaddr, true);

    sprintf(s,"%d: %08lX -> %d",si,((uint32_t*)devaddr)[0], idx);
    Serial.println(s);
    
    if (idx < MAX_SENSORS && idx >= 0) // больше 10ти датчиков будем игнорить, если не влезли в конфиг - тоже игнорим
    {
      index2saved[idx] = si;
    }
  }
  sprintf(s,"Sensors inserted: %d; Sensors registred: %d", deviceCount, savedCount);
  Serial.println(s);

  if(!deviceCount) return;
  writeSensorArray();

  ////////////////////////// создаём файлик csv ///////////////////////////
  for(si=0;si<10000;si++)
  {
    sprintf(curcsv,"W%07d.CSV",si);
    if(!sd.exists(curcsv))
      break;
  }
  
  file = sd.open(curcsv, FILE_WRITE);

  if (file) {
    file.print("Time");
    for(si=0;si<savedCount;si++)
    {
      sprintf(s,"%cS:%d",COL_SEP,si+1);
      file.print(s);
    }
    file.print("\n");
    file.close();
  } else {
    Serial.println("error opening file");
  }
  delay(600);
}

void loop() {
  char s[22];
  float t;
  long int m = millis() / 1000;
  String d = "2000-01-";

  int day, hour, minute, sec;

  if (deviceCount && (m - lastMeasure - firstMeasure >= MEASURE_PERIOD || !firstMeasure))
  {
    if (!firstMeasure) firstMeasure = m;
    
    //    sensors.begin();
    
    lastMeasure = ((m - firstMeasure) / MEASURE_PERIOD) * MEASURE_PERIOD;

    day = lastMeasure / 24 / 3600 + 1;
    hour = (lastMeasure / 3600) % 24;
    minute = (lastMeasure / 60) % 60, lastMeasure % 60, lastMeasure;
    sec = lastMeasure % 60, lastMeasure;
    //sprintf(s, "2000-01-%02ld %02ld:%02ld:%02ld", day, hour, minute, sec);
    if(day<10) d += "0";
    d += day;
    d += " ";
    if(hour<10) d += "0";
    d += hour;
    d += ":";
    if(minute<10) d += "0";
    d += minute;
    d += ":";
    if(sec<10) d += "0";
    d += sec;

    sensors.requestTemperatures();//Запрос температуры от всех датчиков на шине
    int i;
    for (i = 0;  i < savedCount;  i++)
    {
      d += COL_SEP;
      //d += index2saved[i];
      if(index2saved[i] < MAX_SENSORS)
      {
        sensors.getAddress(addr, index2saved[i]);// получаем и сохраняем в массив 64-битный адрес датчика по его текущему индексу
        //printTemperature(addr);
        t = sensors.getTempC(addr);
        d += (int)(t);
        d += DEC_POINT;
        d += ((int)(t*100))%100;
      }
    }

    Serial.print(d);


    file = sd.open(curcsv, FILE_WRITE);

    if (file) {
      // записываем строку в файл
      file.println(d);
      file.close();
      Serial.println("...  written");
    } else {
      //Serial.println("Card failed, or not present");
    }

  }

  delay(500);

}
/*
void printTemperature(uint8_t deviceIndex)
{ 
  Serial.print("Датчик ");
  Serial.print(deviceIndex);//Индекс датчика
  sensors.getAddress(addr, deviceIndex);// получаем и сохраняем в массив 64-битный адрес датчика по его текущему индексу
  Serial.print(" : ");
  //посылаем в Serial Monitor 64-битный адрес датчика
  for (uint8_t i = 0; i < 8; i++) {
   Serial.write(' ');
   if (addr[i]<16){Serial.print("0");}
   Serial.print(addr[i], HEX);
   }
  Serial.print(" - ");
  Serial.println(sensors.getTempC(addr));//Получаем температуру датчика с нужным адресом и посылаем ее в Serial Monitor
}
*/
// Список зарегистрированных датчиков  и период
void readConfig() 
{
  char line[100];
  char r[] = "==============>";
  if (!file.open(CONFNAME, FILE_READ)) {
    Serial.println("Conf open failed");
    return;
  }
  Serial.println(r);    
  while (file.available()) {
    int n = file.fgets(line, sizeof(line));
    if (n <= 0) {
      error("fgets failed");
    }
    if (line[n-1] != '\n' && n == (sizeof(line) - 1)) {
      error("line too long");
    }
    parseLine(line);
  }
  file.close();
  if(COL_SEP!='\t' && COL_SEP!=',' && COL_SEP!=';')
    COL_SEP='\t';
  if(DEC_POINT!=',' && DEC_POINT!='.')
    DEC_POINT=',';
  
  Serial.print("DEC_POINT:");    Serial.println(DEC_POINT);
  Serial.print("COL_SEP:'");    Serial.print(COL_SEP); Serial.println("'");
  //sprintf(line, "\n\nPer: %d; Sensors conf: %d\nConfig was read!\n", MEASURE_PERIOD, savedCount);
  //Serial.println(F("Conf's read\n"));
}


int parseLine(char *str)
{  
  char b[80];
  uint16_t da[4];
  int period;
  uint64_t adr;
 
  //Serial.print(str);
 
  
  if(str[0]=='P') {
    sscanf(str, "P:%d", &period);
    if (period > 1 && period <= 1800)
      MEASURE_PERIOD = period;
      //sprintf(b,"MEASURE_PERIOD = %d",period);          
      //Serial.println(b);
      return 1;
  } else if(str[0]=='S') {
    sscanf(str, "S:%x-%x-%x-%x", da+3, da+2, da+1, da);
    adr=((uint64_t*)da)[0];
    if(savedCount<MAX_SENSORS && getSensorIndex((uint8_t*)da, false)>MAX_SENSORS) {
      savedSensors[savedCount++] = ((uint64_t*)da)[0];
      //sprintf(b,"Sensor %d: %04x %04x %04x %04x", savedCount, da[3], da[2], da[1], da[0]);          
      //Serial.println(b);
      //sprintf(b,"A64 %08lx %08lx", adr);          
      //Serial.println(b);
    }
  } else if(str[0]=='C') {
    COL_SEP=str[2];
    return 1;
  } else if(str[0]=='D') {
    DEC_POINT=str[2];
    return 1;
  }
  return 0;
}

// запись адресов датчиков в файл
void writeSensorArray()
{
  boolean f = true;
  char s[80];
  uint16_t* d;

  //Serial.println("Write config");

  // Remove any existing file.
  sd.remove(CONFNAME);
  
  // Create the file.
  
  if (!file.open(CONFNAME, FILE_WRITE)) {
    error("open failed");
    f = false;
  }
  sprintf(s, "P:%d\nC:%c\nD:%c", MEASURE_PERIOD, COL_SEP, DEC_POINT);
  if (f) {
    file.println(s);
  }
  Serial.println(s);  
  
  for (int i = 0; i < savedCount; i++) {
    d = (uint16_t*)(savedSensors+i);
    sprintf(s, "S:%04X-%04X-%04X-%04X", d[3], d[2], d[1], d[0]);
    if (f)
      file.println(s);
    //Serial.println(s);
  }
  /*
  if (f)
    Serial.println("Config written");
  else
    Serial.println("Config not written");
  */
  if(f)
    file.close();
}

// получение нашего индекса датчика; если не зарегистрирован - регистрируем
int getSensorIndex(uint8_t* addr, boolean append)
{
  int i = 0;
  //char s[80];
  uint16_t* d = (uint16_t*)addr;
  uint64_t a = ((uint64_t*)addr)[0];

  //sprintf(s, "Look for sensor: %04X-%04X-%04X-%04X (%lx-%lx)...", d[3], d[2], d[1], d[0], a);
  //Serial.print(s);

  for (i = 0; i < savedCount; i++)
  {
    //sprintf(s, "      %lx%lx =? %lx%lx", a, savedSensors[i]);
    //Serial.println(s);

    if (savedSensors[i] == a)
    {
      //sprintf(s, "Found: %d (%lx-%lx)", i, a);
  //    Serial.print("Found:");Serial.println(i);
      return i;
    }
  }
  
  if(!append) {
  //  Serial.println("Not found");
    return 255;
  }
  if (i >= MAX_SENSORS) {
  //  Serial.println("Not added");
    return 255;
  }

  savedSensors[i] = a;
  savedCount = i + 1;

 // Serial.print("Added:");Serial.println(i);

  return i;
}
