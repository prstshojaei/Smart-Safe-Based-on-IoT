#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <stdio.h>
#include <stdlib.h>
#include <TimerOne.h>

static volatile int num = 0;

#define Password_Length 5
char Data[Password_Length];
char Number[14];
char password[5];
char Save_Number[14];
char Save_pass[5];
char* id;
char Master[Password_Length] = "123A";

const int sensorPin = 13;
unsigned long pulseCount = 0;
int lastSensorState = LOW;
int lockOutput = 11;
byte data_count = 0;
byte num_count = 0;
byte pass_count = 0;
byte step = 0, motion = 0, vibration = 0;
byte invalid_pass_count = 0;
int i = 0;
unsigned long t_vibration = 0;
unsigned long t_motion = 0;
unsigned long t_location = 0;
unsigned long t_unlock = 0;
unsigned long startMillis = 0;
bool timerStarted = false;
const unsigned long interval = 120000;
int count_location = 0;
char location[100];
String buffer_text;

char customKey;

const byte ROWS = 4;
const byte COLS = 4;

char hexaKeys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {6, 7, 8, 9};
byte colPins[COLS] = {10, A0, A1, A2};

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

SoftwareSerial Sim_serial(2, 3);
SoftwareSerial Gps_serial(5, 4);

TinyGPSPlus gps;

#define TIME_OUT 5000
#define Pir_sensor 12
#define Rst_Sim800 1
#define buzzer A3
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
    Serial.begin(9600);
    Sim_serial.begin(9600);
    lcd.init();
    lcd.clear();
    lcd.backlight();

    pinMode(sensorPin, INPUT);
    pinMode(lockOutput, OUTPUT);
    digitalWrite(lockOutput, LOW);
    pinMode(Pir_sensor, INPUT);
    pinMode(buzzer, OUTPUT);
    pinMode(Rst_Sim800, OUTPUT);
    digitalWrite(Rst_Sim800, LOW);
    lcd.setCursor(0, 0);
    lcd.print("Loading...");

    while (i < 4000) {
        i++;
        lcd.setCursor(0, 1);
        lcd.print(i / 40);
        lcd.print("%");
        delay(1);
    }

    send_command("ATE0");
    send_command("AT+CSMP=49,167,0,0");
    send_command("AT+CNMI=2,2,0,0,0");
    send_command("AT+CMGF=1");
    delet_sms();
    EEPROM.get(0, Save_Number);
    delay(100);
    EEPROM.get(20, Save_pass);
    chek_number(Save_Number);
    chek_password(Save_pass);
    motion = EEPROM.read(200);
    vibration = EEPROM.read(201);
    if (!chek_number(Save_Number) | !chek_password(Save_pass))
        step = 2;

    lcd.clear();
    lcd.setCursor(0, 0);
    chek_location();
    send_sms(Number, location);
    Timer1.initialize(1000);
    Timer1.attachInterrupt(checkPulse);
}

void loop() {
    unsigned long currentMillis = millis();

    chek_sms();

    if (pulseCount > 10 && currentMillis - t_vibration >= 300000) {
        send_sms(Number, "Vibration Detect");
        Serial.print("Pulse ");
        Serial.println(pulseCount);
        pulseCount = 0;
        t_vibration = millis();
    }

    if (digitalRead(Pir_sensor) == HIGH && motion == 1 && currentMillis - t_motion >= 300000) {
        send_sms(Number, "Motion Detect");
        Serial.println("send sms pir");
        t_motion = millis();
    }
    if (currentMillis - t_location >= 600000) {
        t_location = millis();
        chek_location();
        send_sms(Number, location);
    }

    lcd.setCursor(0, 0);
    if (step == 0)
        lcd.print("Enter Password:");
    else if (step == 1)
        lcd.print("Enter Id SMS  :");
    else if (step == 2)
        lcd.print("UNLOCK");
    else if (step == 3)
        lcd.print("Enter Number :");

    customKey = customKeypad.getKey();
    if (customKey) {
        beeb();
        if (step < 2) {
            Data[data_count] = customKey;
            lcd.setCursor(data_count, 1);
            lcd.print(Data[data_count]);
            data_count++;
        } else if (step == 2) {
            if (customKey == '*') step = 0;
            if (customKey == 'A') {
                step = 3;
                num_count = 0;
                lcd.setCursor(0, 0);
                lcd.print("Enter Number :");
                lcd.setCursor(0, 1);
                lcd.print(Number);
            }
            if (customKey == 'B') {
                step = 4;
                pass_count = 0;
                lcd.setCursor(0, 0);
                lcd.print("Enter New Pass:");
                lcd.setCursor(0, 1);
                lcd.print(password);
            }
        } else if (step == 3) {
            if (customKey != 'A' && customKey != 'B' && customKey != 'C' && customKey != 'D' && customKey != '*' && customKey != '#') {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Enter Number :");
                if (num_count > 11) {
                    lcd.clear();
                    clearNumber();
                }
                Number[num_count] = customKey;
                Number[num_count + 1] = '\0';
                lcd.setCursor(0, 1);
                lcd.print(Number);
                num_count++;
            } else if (customKey == '*') {
                lcd.clear();
                step = 2;
                num_count = 0;
            } else if (customKey == '#') {
                lcd.clear();
                if (num_count == 12) {
                    lcd.print("Save Number");
                    EEPROM.put(0, Number);
                } else {
                    lcd.print("NO Save Number");
                    EEPROM.get(0, Number);
                }
                delay(3000);
                step = 2;
                lcd.clear();
            }
        } else if (step == 4) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Enter New Pass:");
            if (customKey != '*' && customKey != '#') {
                if (pass_count > 3) {
                    pass_count = 0;
                    password[0] = password[1] = password[2] = password[3] = 0;
                    password[4] = 0;
                }
                password[pass_count] = customKey;
                password[pass_count + 1] = '\0';
                pass_count++;
                lcd.setCursor(0, 1);
                lcd.print(password);
            } else if (customKey == '*') {
                lcd.clear();
                step = 2;
                pass_count = 0;
            } else if (customKey == '#') {
                lcd.clear();
                if (pass_count == 4) {
                    lcd.print("Save Password");
                    EEPROM.put(20, password);
                } else {
                    lcd.print("NO Save Pass");
                    EEPROM.get(20, password);
                }
                delay(3000);
                step = 2;
                lcd.clear();
            }
        }
    }

    if (data_count == Password_Length - 1) {
        lcd.clear();
        if (step == 0) {
            if (!strcmp(Data, password)) {
                lcd.print("Correct Pass");
                step = 1;
                invalid_pass_count = 0;
                id = random_num(4);
                send_sms(Number, id);
            } else {
                invalid_pass_count++;
                Alarm();
                if (invalid_pass_count > 2) {
                    lcd.print("LOCK FOR 15 MINE");
                    send_sms(Number, "LOCK FOR 15 MINE");
                    wait(900000);
                } else {
                    lcd.print("Incorrect");
                    send_sms(Number, "Incorrect Password");
                    delay(1000);
                }
            }
        } else if (step == 1) {
            if (!strcmp(Data, id)) {
                lcd.print("Correct id");
                digitalWrite(lockOutput, HIGH);
                delay(3000);
                digitalWrite(lockOutput, LOW);
                step = 2;
                send_sms(Number, "Unlock");
                invalid_pass_count = 0;
            } else {
                invalid_pass_count++;
                Alarm();
                if (invalid_pass_count > 2) {
                    lcd.print("LOCK FOR 15 MINE");
                    send_sms(Number, "LOCK FOR 15 MINE");
                    wait(900000);
                } else {
                    lcd.print("Incorrect id");
                    id = random_num(4);
                    send_sms(Number, id);
                    delay(1000);
                }
            }
        }
        lcd.clear();
        clearData();
    }
}

void checkPulse() {
    static int lastState = LOW;
    int currentState = digitalRead(sensorPin);

    if (currentState != lastState && vibration == 1) {
        pulseCount++;
        lastState = currentState;
        Serial.print("Pulse ");
        Serial.println(pulseCount);
    }
}

void Alarm() {
    digitalWrite(buzzer, HIGH);
    delay(1000);
    digitalWrite(buzzer, LOW);
    delay(100);
}

void beeb() {
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
}

void clearData() {
    while (data_count != 0) {
        Data[data_count--] = 0;
    }
    return;
}

void clearNumber() {
    while (num_count != 0) {
        Number[num_count--] = 0;
    }
    return;
}

char* random_num(int num) {
    static char id[16];
    const char digits[] = "0123456789ABCD";
    id[0] = '\0';
    srand(millis());
    for (int i = 0; i < num; ++i) {
        id[i] = digits[rand() % 14];
    }
    id[num] = '\0';
    return id;
}

void chek_location() {
    int t = 0;
    char str_a[20];
    char str_b[20];
    double a = 0, b = 0;
    Gps_serial.begin(9600);
    while (t < 1000) {
        while (Gps_serial.available() > 0) {
            if (gps.encode(Gps_serial.read())) {
                if (gps.location.isValid()) {
                    a = gps.location.lat();
                    b = gps.location.lng();
                    dtostrf(a, 10, 6, str_a);
                    dtostrf(b, 10, 6, str_b);
                    sprintf(location, "%s,%s ", str_a, str_b);
                } else {
                    sprintf(location, "%s ", "INVALID");
                }
            }
        }
        delay(1);
        t++;
    }
    Serial.println(location);
    Sim_serial.begin(9600);
}

void delet_sms() {
    Sim_serial.println("AT+CMGDA=\"DEL ALL\"");
    delay(10000);
}

bool send_sms(char* number, String msg) {
    Sim_serial.begin(9600);
    delay(1000);
    Sim_serial.println("AT+CMGF=1");
    delay(1000);
    Sim_serial.print("AT+CMGS=\"");
    Sim_serial.print(number);
    Sim_serial.println("\"");
    delay(1000);
    Sim_serial.println(msg);
    delay(1000);
    Sim_serial.print(char(26));
    return 0;
}

void chek_sms() {
    String buffer = read_command(1000);

    if (buffer.length() > 0) {
        if (buffer.indexOf("Motion_on") != -1) {
            Serial.println("Motion ON");
            send_sms(Number, "Motion ON");
            EEPROM.write(200, 1);
            motion = 1;
        } else if (buffer.indexOf("Motion_off") != -1) {
            Serial.println("Motion OFF");
            send_sms(Number, "Motion OFF");
            EEPROM.write(200, 0);
            motion = 0;
        } else if (buffer.indexOf("Location") != -1) {
            Serial.println("Loaction search");
            chek_location();
            send_sms(Number, location);
        } else if (buffer.indexOf("Vibr_on") != -1) {
            Serial.println("vibration ON");
            send_sms(Number, "Vibration ON");
            EEPROM.write(201, 1);
            vibration = 1;
        } else if (buffer.indexOf("Vibr_off") != -1) {
            Serial.println("vibration OFF");
            send_sms(Number, "Vibration OFF");
            EEPROM.write(201, 0);
            vibration = 0;
        }
        Serial.println(buffer);
    }
}

void wait(uint32_t timewait) {
    uint64_t timeOld = millis();
    while (!(millis() > timeOld + timewait));
}

bool chek_number(char* num) {
    const char digits[] = "0123456789";
    int len = strlen(num);
    int ok = 0;

    if (len >= 12) {
        for (int j = 0; j < 12; j++) {
            ok = 0;
            for (int n = 0; n < 10; n++) {
                if (num[j] == digits[n]) {
                    Number[j] = num[j];
                    ok = 1;
                    break;
                }
            }
            if (ok == 0) return 0;
        }
        Number[12] = '\0';
    }

    len = strlen(Number);
    if (len != 12) return 0;

    return 1;
}

bool chek_password(char* num) {
    const char digits[] = "0123456789ABCD";
    int len = strlen(num);
    int ok = 0;

    if (len >= 4) {
        for (int j = 0; j < 4; j++) {
            ok = 0;
            for (int n = 0; n < 14; n++) {
                if (num[j] == digits[n]) {
                    password[j] = num[j];
                    ok = 1;
                    break;
                }
            }
            if (ok == 0) return 0;
        }
        password[4] = '\0';
    }

    len = strlen(password);
    if (len != 4) return 0;

    return 1;
}

void send_command(String cmd) {
    Sim_serial.println(cmd);
    Serial.println(cmd);
    delay(2000);
}

void read_sensor() {
    int currentSensorState = digitalRead(sensorPin);

    if (currentSensorState != lastSensorState) {
        pulseCount++;
        lastSensorState = currentSensorState;
    }
}

String read_command(uint16_t timeout) {
    String str = "";

    if (Sim_serial.available()) {
        while (Sim_serial.available()) {
            char c = Sim_serial.read();
            str += c;
        }
    }

    return str;
}
