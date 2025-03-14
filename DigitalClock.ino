#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <SPI.h>
#include <EEPROM.h>
#include <DHT.h>
#include <IRremote.h>

/// Definim variabile pentru ecranul Adafruit
#define TFT_CS 10
#define TFT_RST 9
#define TFT_DC 8

/// Definim cele 3 butoane
#define CURSOR_BUTTON A5
#define INCREMENT_BUTTON A4
#define DECREMENT_BUTTON A3

/// Definim pinul senzorului si tipul senzorului
#define DHTPIN 2
#define DHTTYPE DHT11

/// Definirea pinului pentru buzzer
#define BUZZER_PIN 7

/// Pinul pentru receptorul IR
#define IR_RECEIVER_PIN A2

/// Definirea hexazecimala a semnalelor primite de la telecomanda
const uint32_t IR_INCREMENT_CODE = 0xA3C8EDDB;
const uint32_t IR_CURSOR_CODE_ALT = 0xFF02FD;  
const uint32_t IR_DECREMENT_CODE = 0xF076C13B; 
const uint32_t IR_DECREMENT_CODE_ALT = 0xFFE01F;  
const uint32_t IR_CURSOR_CODE = 0xD7E84B1B;     
const uint32_t IR_INCREMENT_CODE_ALT = 0xFFA857;     

/// Secventa de cod pentru activarea receptorului IR
IRrecv irrecv(IR_RECEIVER_PIN);
decode_results results;

/// Declararea pinilor pentru Adafruit
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

/// Declararea pinilor pentru DHT
DHT dht(DHTPIN, DHTTYPE);

ThreeWire myWire(4, 5, 3); /// DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

/// Variabilele de pe prima pagina
int setHour = 0, setMinute = 0, setSecond = 0;
int alarmHour = 0, alarmMinute = 0;
bool Is24Format = true;

int cursorPosition = 0;

unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 1000;

unsigned long lastRtcCheckTime = 0;
const unsigned long rtcCheckInterval = 10000;

bool isSecondPageDisplayed = false;

unsigned long lastSecondPageUpdate = 0; /// Variabila pentru controlul actualizarii paginii secundare
const unsigned long secondPageUpdateInterval = 1000; /// Interval de actualizare pentru pagina secundara (1 secunda)

bool alarmActive = false;
unsigned long alarmStartTime = 0;
const unsigned long alarmDuration = 5000; /// Durata alarmei (10 secunde)

/// Declaratia variabilei pentru a urmări apasarea prin IR
bool irCursorPressed = false;

void setup() {
    Serial.begin(9600);
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(2);
    tft.fillScreen(ST7735_BLACK);

    pinMode(CURSOR_BUTTON, INPUT_PULLUP);
    pinMode(INCREMENT_BUTTON, INPUT_PULLUP);
    pinMode(DECREMENT_BUTTON, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT); 
    digitalWrite(BUZZER_PIN, LOW); 

    dht.begin();
    irrecv.enableIRIn();

    Rtc.Begin();

    if (Rtc.GetIsWriteProtected()) {
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning()) {
        Rtc.SetIsRunning(true);
    }

    if (isTimeSavedInEEPROM()) {
        loadTimeFromEEPROM();
    } else {
        RtcDateTime now = Rtc.GetDateTime();
        setHour = now.Hour();
        setMinute = now.Minute();
        setSecond = now.Second();
        Is24Format = true;
    }

    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(2);
}


void loop() {
    handleCursor();

    unsigned long currentMillis = millis();

    if (!isSecondPageDisplayed && currentMillis - lastUpdateTime >= updateInterval) {
        lastUpdateTime = currentMillis;

        RtcDateTime now = Rtc.GetDateTime();
        setHour = now.Hour();
        setMinute = now.Minute();
        setSecond = now.Second();

        tft.fillScreen(ST7735_BLACK);
        displayTime();
        displayAlarm();
        displayFormat();
        drawAnalogClock(setHour, setMinute, setSecond);
    }

    checkAndActivateAlarm(currentMillis);

    if (isSecondPageDisplayed) {
        displaySecondPage();
    }

    if (currentMillis - lastRtcCheckTime >= rtcCheckInterval) {
        lastRtcCheckTime = currentMillis;
        printRtcTime();
    }

    handleIR();
}

void handleIR() {
    if (irrecv.decode(&results)) {
        uint32_t irCode = results.value;

        Serial.print("Cod IR primit: ");
        Serial.println(irCode, HEX);

        if (irCode == IR_CURSOR_CODE || irCode == IR_CURSOR_CODE_ALT) {
            Serial.println("Comanda IR: Navigare");
            irCursorPressed = true;
        } else if (irCode == IR_INCREMENT_CODE || irCode == IR_INCREMENT_CODE_ALT) {
            Serial.println("Comanda IR: Incrementare");
            if (cursorPosition >= 6 && cursorPosition <= 8) {
                updateDatePar(1);
                saveTimeToEEPROM();
            } else {
                simulateButtonPress(INCREMENT_BUTTON);
            }
        } else if (irCode == IR_DECREMENT_CODE || irCode == IR_DECREMENT_CODE_ALT) {
            Serial.println("Comanda IR: Decrementare");
            if (cursorPosition >= 6 && cursorPosition <= 8) {
                updateDatePar(-1);
                saveTimeToEEPROM();
            } else {
                simulateButtonPress(DECREMENT_BUTTON);
            }
        } else {
            Serial.println("Comanda IR: Necunoscuta");
        }

        irrecv.resume();
    }
}


void simulateButtonPress(int buttonPin) {
    if (buttonPin == CURSOR_BUTTON) {
        handleCursor();
    } else if (buttonPin == INCREMENT_BUTTON) {
        handleIncrementButton();
    } else if (buttonPin == DECREMENT_BUTTON) {
        handleDecrementButton();
    }
}

void handleIncrementButton() {
    static bool incrementPressed = false;
    if (!incrementPressed) {
        incrementPressed = true;
        if (cursorPosition < 5) {
            updateDateTime(1);
        } else if (cursorPosition >= 6 && cursorPosition <= 8) {
            updateDatePar(1);
        } else {
            Is24Format = !Is24Format;
        }
        updateRtcTime();
        saveTimeToEEPROM();
    } else {
        incrementPressed = false;
    }
}

void handleDecrementButton() {
    static bool decrementPressed = false;
    if (!decrementPressed) {
        decrementPressed = true;
        if (cursorPosition < 5) {
            updateDateTime(-1);
        } else if (cursorPosition >= 6 && cursorPosition <= 8) {
            updateDatePar(-1);
        } else {
            Is24Format = !Is24Format;
        }
        updateRtcTime();
        saveTimeToEEPROM();
    } else {
        decrementPressed = false;
    }
}

void handleCursor() {
static bool cursorPressed = false;
    static bool incrementPressed = false;
    static bool decrementPressed = false;

    /// Verificam dacă butonul de navigare a fost apasat fie prin telecomanda sau fizic
    if ((digitalRead(CURSOR_BUTTON) == LOW && !cursorPressed) || irCursorPressed) {
        cursorPressed = true;
        irCursorPressed = false; // Resetam variabila după apasare pentru telecomanda

        if (isSecondPageDisplayed && cursorPosition < 6) { 
            // Revenim la prima pagina si setma cursorul la prima pozitie
            isSecondPageDisplayed = false;
            cursorPosition = 0;
            tft.fillScreen(ST7735_BLACK);
            displayTime();
            displayAlarm();
            displayFormat();
            drawAnalogClock(setHour, setMinute, setSecond);
        } else if (cursorPosition == 5) {
            isSecondPageDisplayed = true;
            cursorPosition = 6;
            displaySecondPage();
        } else if (cursorPosition == 8) {
            cursorPosition = 0; // Resetarea ciclica
            isSecondPageDisplayed = false;
            tft.fillScreen(ST7735_BLACK);
            displayTime();
            displayAlarm();
            displayFormat();
            drawAnalogClock(setHour, setMinute, setSecond);
        } else {
            cursorPosition = (cursorPosition + 1) % 9;
        }
    } else if (digitalRead(CURSOR_BUTTON) == HIGH) {
        cursorPressed = false;
    }

    if (!isSecondPageDisplayed) {
        if (digitalRead(INCREMENT_BUTTON) == LOW && !incrementPressed) {
            incrementPressed = true;
            if (cursorPosition < 5) {
                updateDateTime(1);
            } else {
                Is24Format = !Is24Format;
            }
            updateRtcTime();
            saveTimeToEEPROM();
        } else if (digitalRead(INCREMENT_BUTTON) == HIGH) {
            incrementPressed = false;
        }

        if (digitalRead(DECREMENT_BUTTON) == LOW && !decrementPressed) {
            decrementPressed = true;
            if (cursorPosition < 5) {
                updateDateTime(-1);
            } else {
                Is24Format = !Is24Format;
            }
            updateRtcTime();
            saveTimeToEEPROM();
        } else if (digitalRead(DECREMENT_BUTTON) == HIGH) {
            decrementPressed = false;
        }
    } else {
        if (digitalRead(INCREMENT_BUTTON) == LOW && !incrementPressed) {
            incrementPressed = true;
            updateDatePar(1);
            saveTimeToEEPROM();
        } else if (digitalRead(INCREMENT_BUTTON) == HIGH) {
            incrementPressed = false;
        }

        if (digitalRead(DECREMENT_BUTTON) == LOW && !decrementPressed) {
            decrementPressed = true;
            updateDatePar(-1);
            saveTimeToEEPROM();
        } else if (digitalRead(DECREMENT_BUTTON) == HIGH) {
            decrementPressed = false;
        }
    }
}


void displaySecondPage() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastSecondPageUpdate >= secondPageUpdateInterval) {
        lastSecondPageUpdate = currentMillis;

        tft.fillScreen(ST7735_BLACK);

        RtcDateTime now = Rtc.GetDateTime();
        String DayOfWeek = getDayOfWeek(now.DayOfWeek());

        tft.setCursor(10, 10);
        tft.print("Data: ");
        tft.print(cursorPosition == 6 ? ">" : " ");
        tft.print(now.Year());
        tft.print("/");

        tft.print(cursorPosition == 7 ? ">" : " ");
        tft.print(now.Month());
        tft.print("/");

        tft.print(cursorPosition == 8 ? ">" : " ");
        tft.print(now.Day());

        tft.setCursor(10, 30);
        tft.print("Zi: ");
        tft.print(DayOfWeek);

        float temp = dht.readTemperature();
        float humidity = dht.readHumidity();

        tft.setCursor(10, 50);
        tft.print("Temp: ");
        tft.print(temp);
        tft.print(" C");

        tft.setCursor(10, 70);
        tft.print("Hum: ");
        tft.print(humidity);
        tft.print(" %");
    }
}

void updateDatePar(int direction) {
    RtcDateTime now = Rtc.GetDateTime();
    int year = now.Year();
    int month = now.Month();
    int day = now.Day();

    switch (cursorPosition) {
        case 6: year = year + direction; break;
        case 7: month = (month + direction - 1 + 12) % 12 + 1; break;
        case 8: day = (day + direction - 1 + 31) % 31 + 1; break;
    }

    RtcDateTime newTime(year, month, day, setHour, setMinute, setSecond);
    Rtc.SetDateTime(newTime);
}

String getDayOfWeek(int DayOfWeek) {
    switch (DayOfWeek) {
        case 0: return "Duminica";
        case 1: return "Luni";
        case 2: return "Marti";
        case 3: return "Miercuri";
        case 4: return "Joi";
        case 5: return "Vineri";
        case 6: return "Sambata";
        default: return "";
    }
}

void displayTime() {
    int displayHour = setHour;
    String period = "";

    if (!Is24Format) {
        if (displayHour >= 12) {
            period = " PM";
            displayHour = (displayHour > 12) ? displayHour - 12 : displayHour;
        } else {
            period = " AM";
            displayHour = (displayHour == 0) ? 12 : displayHour;
        }
    }

    tft.setCursor(3, 5);
    tft.print("Timp: ");
    tft.print(cursorPosition == 0 ? ">" : " ");
    tft.print(displayHour < 10 ? "0" : ""); tft.print(displayHour); tft.print(":");
    tft.print(cursorPosition == 1 ? ">" : " ");
    tft.print(setMinute < 10 ? "0" : ""); tft.print(setMinute); tft.print(":");
    tft.print(cursorPosition == 2 ? ">" : " ");
    tft.print(setSecond < 10 ? "0" : ""); tft.print(setSecond);
    tft.print(period);
}

void displayAlarm() {
    int displayHour = alarmHour;
    String period = "";

    if (!Is24Format) {
        if (displayHour >= 12) {
            period = " PM";
            displayHour = (displayHour > 12) ? displayHour - 12 : displayHour;
        } else {
            period = " AM";
            displayHour = (displayHour == 0) ? 12 : displayHour;
        }
    }

    tft.setCursor(10, 17);
    tft.print("Alarma: ");
    tft.print(cursorPosition == 3 ? ">" : " ");
    tft.print(displayHour < 10 ? "0" : ""); tft.print(displayHour); tft.print(":");
    tft.print(cursorPosition == 4 ? ">" : " ");
    tft.print(alarmMinute < 10 ? "0" : ""); tft.print(alarmMinute);
    tft.print(period);
}

void displayFormat() {
    tft.setCursor(10, 28);
    tft.print("Format: ");
    tft.print(cursorPosition == 5 ? ">" : " ");
    tft.print(Is24Format ? "24H" : "AM/PM");
}

void updateDateTime(int direction) {
    switch (cursorPosition) {
        case 0: setHour = (setHour + direction + 24) % 24; break;
        case 1: setMinute = (setMinute + direction + 60) % 60; break;
        case 2: setSecond = (setSecond + direction + 60) % 60; break;
        case 3: alarmHour = (alarmHour + direction + 24) % 24; break;
        case 4: alarmMinute = (alarmMinute + direction + 60) % 60; break;
    }
}

void updateRtcTime() {
  RtcDateTime now = Rtc.GetDateTime();
    int currentYear = now.Year();
    int currentMonth = now.Month(); 
    int currentDay = now.Day(); 

    RtcDateTime newTime(currentYear, currentMonth, currentDay, setHour, setMinute, setSecond);
    Rtc.SetDateTime(newTime);
}

void printRtcTime() {
    RtcDateTime now = Rtc.GetDateTime();
    Serial.print("Timpul din RTC: ");
    Serial.print(now.Year(), DEC);
    Serial.print("-");
    Serial.print(now.Month(), DEC);
    Serial.print("-");
    Serial.print(now.Day(), DEC);
    Serial.print(" ");
    Serial.print(now.Hour(), DEC);
    Serial.print(":");
    Serial.print(now.Minute(), DEC);
    Serial.print(":");
    Serial.println(now.Second(), DEC);
}

void drawAnalogClock(int hour, int minute, int second) {
    int centerX = 64, centerY = 80;
    int clockRadius = 40;

    tft.drawCircle(centerX, centerY, clockRadius, ST7735_WHITE);

    for (int i = 1; i <= 12; i++) {
        float angle = (i * 30) * 0.0174533;
        int xPos = centerX + (clockRadius - 10) * sin(angle);
        int yPos = centerY - (clockRadius - 10) * cos(angle);
        tft.setCursor(xPos - 4, yPos - 4);
        tft.setTextColor(ST7735_WHITE);
        tft.setTextSize(1);
        tft.print(i);
    }

    float hourAngle = (hour % 12 + minute / 60.0) * 30;
    float minuteAngle = minute * 6;
    float secondAngle = second * 6;

    drawHand(centerX, centerY, hourAngle, clockRadius - 15, ST7735_WHITE);
    drawHand(centerX, centerY, minuteAngle, clockRadius - 10, ST7735_GREEN);
    drawHand(centerX, centerY, secondAngle, clockRadius - 5, ST7735_RED);
}

void drawHand(int x, int y, float angle, int length, uint16_t color) {
    float rad = angle * 0.0174533;
    int x2 = x + length * sin(rad);
    int y2 = y - length * cos(rad);
    tft.drawLine(x, y, x2, y2, color);
}

void saveTimeToEEPROM() {
    EEPROM.write(0, setHour);
    EEPROM.write(1, setMinute);
    EEPROM.write(2, setSecond);
    EEPROM.write(3, Is24Format ? 1 : 0);
}

void loadTimeFromEEPROM() {
    setHour = EEPROM.read(0);
    setMinute = EEPROM.read(1);
    setSecond = EEPROM.read(2);
    Is24Format = EEPROM.read(3) == 1;
}

bool isTimeSavedInEEPROM() {
    return EEPROM.read(3) != 255;
}

void checkAndActivateAlarm(unsigned long currentMillis) {
    // Verifică dacă alarma este activă și timpul curent se potrivește cu timpul alarmei
    if (alarmActive && currentMillis - alarmStartTime >= alarmDuration) {
        alarmActive = false;
        digitalWrite(BUZZER_PIN, LOW); // Dezactivează alarma
    }
    // Verifică dacă timpul curent este egal cu timpul setat pentru alarmă
    RtcDateTime now = Rtc.GetDateTime();
    if (now.Hour() == alarmHour && now.Minute() == alarmMinute && !alarmActive) {
        alarmActive = true;
        alarmStartTime = currentMillis;
        digitalWrite(BUZZER_PIN, HIGH); // Activează buzzerul
    }
}


