/*
  Скетч к проекту "Домашняя метеостанция"
  Страница проекта (схемы, описания): https://alexgyver.ru/meteoclock/
  Исходники на GitHub: https://github.com/AlexGyver/MeteoClock
  Нравится, как написан и закомментирован код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2018
  http://AlexGyver.ru/
*/

/*
  Время и дата устанавливаются атвоматически при загрузке прошивки (такие как на компьютере)
  График всех величин за час и за сутки (усреднённые за каждый час)
  В модуле реального времени стоит батарейка, которая продолжает отсчёт времени после выключения/сброса питания
  Как настроить время на часах. У нас есть возможность автоматически установить время на время загрузки прошивки, поэтому:
	- Ставим настройку RESET_CLOCK на 1
  - Прошиваемся
  - Сразу ставим RESET_CLOCK 0
  - И прошиваемся ещё раз
  - Всё
*/

/* Версия 1.3
  - Исправлен прогноз погоды
*/

// ------------------------- НАСТРОЙКИ --------------------
#define RESET_CLOCK 0     // сброс часов на время загрузки прошивки (для модуля с несъёмной батарейкой). Не забудь поставить 0 и прошить ещё раз!
#define SENS_TIME 30000   // время обновления показаний сенсоров на экране, миллисекунд
#define LED_MODE 0        // тип RGB светодиода: 0 - главный катод, 1 - главный анод
#define LED_BRIGHT 255    // яркость светодиода СО2 (0 - 255)
#define BLUE_YELLOW 1     // жёлтый цвет вместо синего (1 да, 0 нет) но из за особенностей подключения жёлтый не такой яркий
#define DISP_MODE 1       // в правом верхнем углу отображать: 0 - год, 1 - день недели, 2 - секунды
#define WEEK_LANG 0       // язык дня недели: 0 - английский, 1 - русский (транслит)
#define DEBUG 0           // вывод на дисплей лог инициализации датчиков при запуске. Для дисплея 1602 не работает! Но дублируется через порт!
#define PRESSURE 0        // 0 - график давления, 1 - график прогноза дождя (вместо давления). Не забудь поправить пределы гроафика
#define CO2_SENSOR 1      // включить или выключить поддержку/вывод с датчика СО2 (1 вкл, 0 выкл)
#define DISPLAY_TYPE 1    // тип дисплея: 1 - 2004 (большой), 0 - 1602 (маленький)
#define DISPLAY_ADDR 0x27 // адрес платы дисплея: 0x27 или 0x3f. Если дисплей не работает - смени адрес! На самом дисплее адрес не указан

// пределы отображения для графиков
#define TEMP_MIN 15
#define TEMP_MAX 35
#define HUM_MIN 0
#define HUM_MAX 100
#define PRESS_MIN 600
#define PRESS_MAX 800
//#define PRESS_MIN -100
//#define PRESS_MAX 100
#define CO2_MIN 300
#define CO2_MAX 2000

// Часы включения и выключения подсветки
#define BL_START 7 // включится в 07:00 утром
#define BL_STOP 0 // выключится в 01:00 ночью

// Яркость подсветки (255 ваксимум, 0 минимум)
int BL_HIGHT = 150; // день 
int BL_LOW   = 20;  // в ночном режиме

// Стостояние подсветки включено/выключено
boolean BL_SW_STATUS = true;


// адрес BME280 жёстко задан в файле библиотеки Adafruit_BME280.h
// стоковый адрес был 0x77, у китайского модуля адрес 0x76.
// Так что если юзаете не библиотеку из архива - не забудьте поменять

// если дисплей не заводится - поменяйте адрес (строка 54)

// пины
#define MHZ_RX 2
#define MHZ_TX 3

#define LED_COM 7
#define LED_R 9
#define LED_G 6
#define LED_B 10

#define BTN_PIN 4

#define BL_PIN 5 // BackLight PWM Pin

// библиотеки
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if (DISPLAY_TYPE == 1)
LiquidCrystal_I2C lcd(DISPLAY_ADDR, 20, 4);
#else
LiquidCrystal_I2C lcd(DISPLAY_ADDR, 16, 2);
#endif

#include "RTClib.h"
RTC_DS3231 rtc;
DateTime now;

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;

#if (CO2_SENSOR == 1)
#include <MHZ19_uart.h>
MHZ19_uart mhz19;
#endif

#include <GyverTimer.h>
GTimer_ms sensorsTimer(SENS_TIME);
GTimer_ms drawSensorsTimer(SENS_TIME);
GTimer_ms clockTimer(500);
GTimer_ms hourPlotTimer((long)4 * 60 * 1000);         // 4 минуты
GTimer_ms dayPlotTimer((long)1.6 * 60 * 60 * 1000);   // 1.6 часа
GTimer_ms plotTimer(240000);
GTimer_ms predictTimer((long)10 * 60 * 1000);         // 10 минут

#include "GyverButton.h"
GButton button(BTN_PIN, LOW_PULL, NORM_OPEN);

int8_t hrs, mins, secs;
byte mode = 0;
/*
  0 часы и данные
  1 график температуры за час
  2 график температуры за сутки
  3 график влажности за час
  4 график влажности за сутки
  5 график давления за час
  6 график давления за сутки
  7 график углекислого за час
  8 график углекислого за сутки
*/

// переменные для вывода
float dispTemp;
byte dispHum;
int dispPres;
int dispCO2;
int dispRain;


// массивы графиков
int tempHour[15], tempDay[15];
int humHour[15], humDay[15];
int pressHour[15], pressDay[15];
int co2Hour[15], co2Day[15];
int delta;
uint32_t pressure_array[6];
uint32_t sumX, sumY, sumX2, sumXY;
float a, b;
byte time_array[6];


#if (WEEK_LANG == 0)
static const char *dayNames[]  = {
  "Sun",
  "Mon",
  "Tue",
  "Wed",
  "Thu",
  "Fri",
  "Sat",
};
#else
static const char *dayNames[]  = {
  "BOCK",
  "POND",
  "BTOP",
  "CPED",
  "4ETB",
  "5YAT",
  "CYBB",
};
#endif

#if (LED_MODE == 0)
#define LED_ON (LED_BRIGHT)
#else
#define LED_ON (255 - LED_BRIGHT)
#endif

void setup() {
  Serial.begin(9600);

  pinMode(LED_COM, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  setLED(0);

  // Инициализируем пин подсветки
  pinMode(BL_PIN, OUTPUT);
  // Включаем подсветку
  setBL();

  digitalWrite(LED_COM, LED_MODE);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  

#if (DEBUG == 1 && DISPLAY_TYPE == 1)
  boolean status = true;

  setLED(1);

#if (CO2_SENSOR == 1)
  lcd.setCursor(0, 0);
  lcd.print(F("MHZ-19... "));
  Serial.print(F("MHZ-19... "));
  mhz19.begin(MHZ_TX, MHZ_RX);
  mhz19.setAutoCalibration(false);
  mhz19.getStatus();    // первый запрос, в любом случае возвращает -1
  delay(500);
  if (mhz19.getStatus() == 0) {
    lcd.print(F("OK"));
    Serial.println(F("OK"));
  } else {
    lcd.print(F("ERROR"));
    Serial.println(F("ERROR"));
    status = false;
  }
#endif

  setLED(2);
  lcd.setCursor(0, 1);
  lcd.print(F("RTC... "));
  Serial.print(F("RTC... "));
  delay(50);
  if (rtc.begin()) {
    lcd.print(F("OK"));
    Serial.println(F("OK"));
  } else {
    lcd.print(F("ERROR"));
    Serial.println(F("ERROR"));
    status = false;
  }

  setLED(3);
  lcd.setCursor(0, 2);
  lcd.print(F("BME280... "));
  Serial.print(F("BME280... "));
  delay(50);
  if (bme.begin(&Wire)) {
    lcd.print(F("OK"));
    Serial.println(F("OK"));
  } else {
    lcd.print(F("ERROR"));
    Serial.println(F("ERROR"));
    status = false;
  }

  setLED(0);
  lcd.setCursor(0, 3);
  if (status) {
    lcd.print(F("All good"));
    Serial.println(F("All good"));
    delay(1000);
    lcd.clear();
  } else {
    lcd.print(F("Check wires!"));
    Serial.println(F("Check wires!"));
    delay(1000);
    while (1);
  }
#else

#if (CO2_SENSOR == 1)
  mhz19.begin(MHZ_TX, MHZ_RX);
  mhz19.setAutoCalibration(false);
#endif
  rtc.begin();
  bme.begin(&Wire);
#endif

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temperature
                  Adafruit_BME280::SAMPLING_X1, // pressure
                  Adafruit_BME280::SAMPLING_X1, // humidity
                  Adafruit_BME280::FILTER_OFF   );

  if (RESET_CLOCK || rtc.lostPower())
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  now = rtc.now();
  secs = now.second();
  mins = now.minute();
  hrs = now.hour();

  bme.takeForcedMeasurement();
  uint32_t Pressure = bme.readPressure();
  for (byte i = 0; i < 6; i++) {   // счётчик от 0 до 5
    pressure_array[i] = Pressure;  // забить весь массив текущим давлением
    time_array[i] = i;             // забить массив времени числами 0 - 5
  }

  if (DISPLAY_TYPE == 1) {
    loadClock();
    drawClock(hrs, mins, 0, 0, 1);
    drawData();
  }
  readSensors();
  drawSensors();
}

void loop() {
  // Настройка часов через компорт
  if (Serial.available() > 0) {
      String buf_txt = Serial.readString();
      
      now = rtc.now();
      
      if (buf_txt.substring(0,1) == "H"){
        hrs = buf_txt.substring(1,3).toInt();
        Serial.println("Set Hours -> " + hrs);
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), hrs, now.minute(), now.second()));
        drawClock(hrs, mins, 0, 0, 1);
      }

      if (buf_txt.substring(0,1) == "M"){
        mins = buf_txt.substring(1,3).toInt();
        Serial.println("Set Minute -> "+buf_txt.substring(1,3));
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), mins, now.second()));
        drawClock(hrs, mins, 0, 0, 1);
      }

  }

  
  if (sensorsTimer.isReady()) readSensors();    // читаем показания датчиков с периодом SENS_TIME

#if (DISPLAY_TYPE == 1)
  if (clockTimer.isReady()) clockTick();        // два раза в секунду пересчитываем время и мигаем точками
  plotSensorsTick();                            // тут внутри несколько таймеров для пересчёта графиков (за час, за день и прогноз)
  modesTick();                                  // тут ловим нажатия на кнопку и переключаем режимы
  if (mode == 0) {                                  // в режиме "главного экрана"
    if (drawSensorsTimer.isReady()) drawSensors();  // обновляем показания датчиков на дисплее с периодом SENS_TIME
  } else {                                          // в любом из графиков
    if (plotTimer.isReady()) redrawPlot();          // перерисовываем график
  }
#else
  if (drawSensorsTimer.isReady()) drawSensors();
#endif
}
