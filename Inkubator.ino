#include <LiquidCrystal_I2C.h>//дисплей 16:2
#include <OneWire.h>          //датчик температури DS18B20
#include <DHT.h>              //датчик вологості
#include <EEPROM.h>           //для збереження даних
#include <Servo.h>            //сервомотор

//назви пінів:
#define PIN_BUTTON_UP 8
#define PIN_BUTTON_DOWN 9
#define PIN_BUTTON_LEFT 10
#define PIN_BUTTON_RIGHT 11
#define PIN_TEMPERATURE 6
#define PIN_HUMIDITY_LEVEL 4
#define PIN_RELE_VENTILATOR 3
#define PIN_RELE_HEATER 2
#define PIN_SERVO 5

const int8_t COUNT_PERIODS = 4;
const unsigned long DAY = 86400000; // день в мілісекундах
const unsigned long HOUR = 3600000; // година в мілісекундах
const unsigned int MIN = 60000;     // хвилина в мілісекундах
const unsigned int SEK = 1000;      // секунда в мілісекундах

LiquidCrystal_I2C LCD = {0x27, 16, 2};
OneWire ds(PIN_TEMPERATURE);
DHT dht(PIN_HUMIDITY_LEVEL, DHT11);
Servo servo;

enum TypeBird {
  CHICKEN,
  DUCK,
  GOOSE,
  TURKEY,
  COUNT_TYPE_BIRD
};

enum Menu {
  HOME,
  SET_TEMPERATURE,
  SET_COUNT_ROTATE,
  SET_COUNT_AIRING,
  SET_TIME_AIRING,
  SET_TYPE_BIRD,
  COUNT_OPTIONS
};

struct Egg {
  uint8_t durationInkubation;
  float temperature;
  uint8_t humi;
  uint8_t rotate;
  uint8_t airing;
  uint8_t timeAiring;
};

char nameBird[COUNT_TYPE_BIRD][10] {
  "Chicken",
  "Duck",
  "Goose",
  "Turkey"
};

Egg currentParamsEgg;
Egg listParams[COUNT_TYPE_BIRD][COUNT_PERIODS]
{
  {
    //4 періоди для курей
    {11, 37.9, 66, 4, 0, 1},
    {17, 37.3, 53, 4, 2, 5},
    {19, 37.3, 47, 4, 2, 20},
    {21, 37, 66, 0, 2, 5}
  },
  {
    //4 періоди для качок
    {8, 38, 70, 0, 0, 1},
    {13, 37.5, 60, 4, 1, 5},
    {24, 37.2, 56, 4, 2, 20},
    {28, 37, 70, 0, 1, 10}
  },
  {
    //4 періоди для гусей
    {3, 37.8, 63, 0, 0, 1},
    {12, 37.8, 54, 4, 1, 5},
    {24, 37.5, 56, 4, 3, 20},
    {27, 37.2, 57, 0, 1, 10},
  },
  {
    //4 періоди для індиків
    {6, 37.8, 56, 4, 0, 1},
    {12, 37.5, 52, 4, 1, 5},
    {26, 37.2, 52, 4, 2, 20},
    {28, 37, 70, 0, 1, 10}
  }
};

// допоміжні змінні
int temp = 0;
uint8_t menu = 0;
uint8_t typeEgg;
uint8_t currentDay = 0;
uint8_t currentPeriod = 0;
bool stateServo = 0;
bool temperatureConversion = 1;

// змінні для роботи з кнопками
bool lastClickUp = LOW;
bool lastClickDown = LOW;
bool lastClickLeft = LOW;
bool lastClickRight = LOW;

bool currentClickUp;
bool currentClickDown;
bool currentClickLeft;
bool currentClickRight;

// змінні відліку
unsigned long lastTimeDeleyDS18B20 = 0;
unsigned long lastTimeDay = 0;
unsigned long lastTimeDeleyEEPROM = 0;
unsigned long lastTimeDeleyVentilatorOFF = 0;
unsigned long lastTimeDeleyVentilatorON = 0;
unsigned long lastTimeDeleyServo = 0;

//функція для усунення брязкоту контактів
bool debounce(bool lastClick, uint8_t pinButton)
{
  if (lastClick != digitalRead(pinButton))
    delay(5);
  return digitalRead(pinButton);
}

void setup() {
  LCD.init();
  LCD.backlight();

  pinMode(PIN_RELE_HEATER, OUTPUT);
  pinMode(PIN_RELE_VENTILATOR, OUTPUT);

  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);

  dht.begin();

  servo.attach(PIN_SERVO);

  if (EEPROM.read(0) != 'K')
  {
    EEPROM.write(0, 'K');
    typeEgg = 0;
    currentDay = 0;
    currentPeriod = 0;
    currentParamsEgg = listParams[0][0];
    EEPROM.put(1, typeEgg);
    EEPROM.put(2, currentDay);
    EEPROM.put(3, currentPeriod);
    EEPROM.put(4, currentParamsEgg);
  }
  else
  {
    EEPROM.get(1, typeEgg);
    EEPROM.get(2, currentDay);
    EEPROM.get(3, currentPeriod);
    EEPROM.get(4, currentParamsEgg);
  }
}

void loop() {

  //зміна періодів інкубації
  if (millis() - lastTimeDay > DAY)
  {
    if (++currentDay > currentParamsEgg.durationInkubation)
    {
      if (++currentPeriod >= COUNT_PERIODS)
      {
        currentDay = 0;
        currentPeriod = 0;
      }

      currentParamsEgg = listParams[typeEgg % COUNT_TYPE_BIRD][currentPeriod];
    }
    lastTimeDay = millis();
  }

  //зчитування сигналу з кнопок
  currentClickUp = debounce(lastClickUp, PIN_BUTTON_UP);
  currentClickDown = debounce(lastClickDown, PIN_BUTTON_DOWN);
  currentClickLeft = debounce(lastClickLeft, PIN_BUTTON_LEFT);
  currentClickRight = debounce(lastClickRight, PIN_BUTTON_RIGHT);

  //навігація по меню
  if (lastClickUp == LOW && currentClickUp == HIGH)
    ++menu;
  if (lastClickDown == LOW && currentClickDown == HIGH)
    --menu;

  //установка значень
  switch (menu % COUNT_OPTIONS)
  {
    case SET_TEMPERATURE:
      if (lastClickLeft == LOW && currentClickLeft == HIGH && currentParamsEgg.temperature > 10.1)
        currentParamsEgg.temperature -= 0.1;
      if (lastClickRight == LOW && currentClickRight == HIGH && currentParamsEgg.temperature < 39.9)
        currentParamsEgg.temperature += 0.1;
      break;

    case SET_COUNT_ROTATE:
      if (lastClickLeft == LOW && currentClickLeft == HIGH && currentParamsEgg.rotate > 0)
        --currentParamsEgg.rotate;
      if (lastClickRight == LOW && currentClickRight == HIGH && currentParamsEgg.rotate < 10)
        ++currentParamsEgg.rotate;
      break;

    case SET_COUNT_AIRING:
      if (lastClickLeft == LOW && currentClickLeft == HIGH && currentParamsEgg.airing > 0)
        --currentParamsEgg.airing;
      if (lastClickRight == LOW && currentClickRight == HIGH && currentParamsEgg.airing < 10)
        ++currentParamsEgg.airing;
      break;

    case SET_TIME_AIRING:
      if (lastClickLeft == LOW && currentClickLeft == HIGH && currentParamsEgg.timeAiring > 1)
        --currentParamsEgg.timeAiring;
      if (lastClickRight == LOW && currentClickRight == HIGH && currentParamsEgg.timeAiring < 50)
        ++currentParamsEgg.timeAiring;
      break;

    case SET_TYPE_BIRD:
      if (lastClickLeft == LOW && currentClickLeft == HIGH)
      {
        --typeEgg;
        currentDay = 0;
        currentPeriod = 0;
        currentParamsEgg = listParams[typeEgg % COUNT_TYPE_BIRD][0];
      }
      if (lastClickRight == LOW && currentClickRight == HIGH)
      {
        ++typeEgg;
        currentDay = 0;
        currentPeriod = 0;
        currentParamsEgg = listParams[typeEgg % COUNT_TYPE_BIRD][0];
      }
      break;
  }
  
  //збереження попереднього стану кнопки
  lastClickUp = currentClickUp;
  lastClickDown = currentClickDown;
  lastClickLeft = currentClickLeft;
  lastClickRight = currentClickRight;

  // отримання температури з датчика DS18B20
  if (millis() - lastTimeDeleyDS18B20 > 1000)
  {
    ds.reset();
    ds.write(0xCC);

    if (temperatureConversion)
      ds.write(0x44); // вказуємо датчику обчислити температуру
    else
    {
      ds.write(0xBE); // отримуємо нові дані температури
      temp = ds.read();
      temp = (ds.read() << 8) | temp;
    }
    temperatureConversion = !temperatureConversion;
    lastTimeDeleyDS18B20 = millis();
  }

  //вивід інформації на дисплей
  LCD.setCursor(0, 0);
  switch (menu % COUNT_OPTIONS)
  {
    case HOME:
      LCD.print("Temp |Humi%|Day");
      LCD.setCursor(0, 1);
      LCD.print(temp / 16.0);
      LCD.print("|");
      LCD.print((int8_t)dht.readHumidity());
      LCD.print("/");
      LCD.print(currentParamsEgg.humi);
      LCD.print("|");
      LCD.print(currentDay);
      LCD.print("   ");
      break;

    case SET_TEMPERATURE:
      LCD.print("Temperature    ");
      LCD.setCursor(0, 1);
      LCD.print("<");
      LCD.print(currentParamsEgg.temperature);
      LCD.print(">         ");
      break;

    case SET_COUNT_ROTATE:
      LCD.print("Count rotate    ");
      LCD.setCursor(0, 1);
      LCD.print("<");
      LCD.print(currentParamsEgg.rotate);
      LCD.print(">            ");
      break;

    case SET_COUNT_AIRING:
      LCD.print("Count airing    ");
      LCD.setCursor(0, 1);
      LCD.print("<");
      LCD.print(currentParamsEgg.airing);
      LCD.print(">           ");
      break;

    case SET_TIME_AIRING:
      LCD.print("Time airing    ");
      LCD.setCursor(0, 1);
      LCD.print("<");
      LCD.print(currentParamsEgg.timeAiring);
      LCD.print(">            ");
      break;

    case SET_TYPE_BIRD:
      LCD.print("Type bird       ");
      LCD.setCursor(0, 1);
      LCD.print("<");
      LCD.print(nameBird[typeEgg % COUNT_TYPE_BIRD]);
      LCD.print(">          ");
      break;
  }

  //робота обігрівача
  if (temp / 16.0 < currentParamsEgg.temperature)
    digitalWrite(PIN_RELE_HEATER, 1);
  if (temp / 16.0 > currentParamsEgg.temperature)
    digitalWrite(PIN_RELE_HEATER, 0);

  //робота вентилятора
  if (currentParamsEgg.airing != 0)
  {
    if (millis() - lastTimeDeleyVentilatorON > DAY / currentParamsEgg.airing)
    {
      digitalWrite(PIN_RELE_VENTILATOR, 1);
      lastTimeDeleyVentilatorON = millis();
      lastTimeDeleyVentilatorOFF = millis();
    }
  }
  if (millis() - lastTimeDeleyVentilatorOFF > currentParamsEgg.timeAiring * MIN)
    digitalWrite(PIN_RELE_VENTILATOR, 0);

  //робота сервомотора
  if (currentParamsEgg.rotate > 0)
  {
    if (millis() - lastTimeDeleyServo > DAY / currentParamsEgg.rotate)
    {
      stateServo = !stateServo;
      lastTimeDeleyServo = millis();
    }
    servo.write(180 * stateServo);
  }

  //збереження даних
  if (millis() - lastTimeDeleyEEPROM > HOUR)
  {
    EEPROM.put(1, typeEgg);
    EEPROM.put(2, currentDay);
    EEPROM.put(3, currentPeriod);
    EEPROM.put(4, currentParamsEgg);
  }
}
