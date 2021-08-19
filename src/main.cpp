#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include "DHT.h"
#include <SPI.h>
#include <time.h>
#include <Servo.h>

// cấu hình cho device
#define LED1pin 26
#define LED2pin 27
#define LED3pin 25
#define SERVOpin 13
#define MOTORpin 33

// cấu hình cho RFID
#define RFID_SCK 18
#define RFID_MISO 19
#define RFID_SDA 4
#define RFID_RST 5
#define RFID_MOSI 23
#define RFID_LED 2

// cấu hình cho DHT11 với LCD
#define DHT11pin 32
#define LCD_SDA 21
#define LCD_SCL 22

// config firebase
#define FB_HOST "rfid-bighomework-default-rtdb.firebaseio.com" // ten project
#define FB_Authen "OWSMnS9FT5FegZdbq3sgdezel3O9qnb0IuJpYuaC"   // authen

const char *ntpServer = "asia.pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;
Servo myservo;

struct
{
    bool LED1_status = 0;
    bool LED2_status = 0;
    bool LED3_status = 0;

    bool LCD_status = 0;
    String LCD_line1;
    String LCD_line2;

    int MOTOR_speed;
    bool door;
} smarthome;

LiquidCrystal_I2C lcd(0x27, 16, 2);

MFRC522 rfid(RFID_SDA, RFID_RST); // Instance of the class
MFRC522::MIFARE_Key key;

StaticJsonDocument<200> ajson;
FirebaseData data;
FirebaseJson json;
String date_n, time_n, sum_time;
bool doorOpen = false;

DHT dht(DHT11pin, DHT11);

void device_control();
void receive_firebase_status_device();
void check_rfid();
void send_dht11();
void send_rfid(String ID);
void get_datetime();
void led2_flash()
{
    digitalWrite(2, 1);
    delay(80);
    digitalWrite(2, 0);
}

float dht11_t = 0, dht11_h = 0;

void setup()
{
    Serial.begin(115200);
    WiFi.begin("Dung Manh", "dungmanh123");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("...");
        delay(100);
    }
    Serial.println();
    Serial.println(WiFi.localIP());

    // khoi tao lcd

    lcd.init();
    lcd.backlight();

    //  Wire.begin(4, 5, 400000);
    // khởi tạo giao tiếp firebase
    Firebase.begin(FB_HOST, FB_Authen);

    SPI.begin();     // Init SPI bus
    rfid.PCD_Init(); // Init MFRC522

    // khởi tạo mặc định RFID
    for (byte i = 0; i < 6; i++)
    {
        key.keyByte[i] = 0xFF;
    }

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    pinMode(2, OUTPUT);

    myservo.attach(SERVOpin);
    // setup dht11
    dht.begin();


    pinMode(LED1pin, OUTPUT);
    pinMode(LED2pin, OUTPUT);
    pinMode(LED3pin, OUTPUT);
    pinMode(MOTORpin, OUTPUT);
    // pinMode(LED1pin, OUTPUT);

}
unsigned int t_delay_1 = millis();
unsigned int t_delay_2 = millis();

void loop()
{
    if (millis() - t_delay_1 > 1000)
    {
        receive_firebase_status_device();
        device_control();
        t_delay_1 = millis();
    }
    if (millis() - t_delay_2 > 2000)
    {

        dht11_t = dht.readTemperature();
        dht11_h = dht.readHumidity();
        send_dht11();
        t_delay_2 = millis();
    }
    //  readDHT();
    check_rfid();
}

void receive_firebase_status_device()
{
    if (Firebase.getJSON(data, "/Device"))
    {
        String s = data.jsonString();
        Serial.println(s);
        deserializeJson(ajson, s);
        smarthome.LED1_status = ajson["led1"];
        smarthome.LED2_status = ajson["led2"];
        smarthome.LED3_status = ajson["led3"];
        smarthome.LCD_status = ajson["lcd_status"];
        String s1 = ajson["lcd_line1"];
        smarthome.LCD_line1 = s1;
        //       Serial.println(smarthome.LCD_line1);
        String s2 = ajson["lcd_line2"];
        smarthome.LCD_line2 = s2;

        smarthome.MOTOR_speed = ajson["motor"];
        smarthome.door = ajson["door"];
    }
    else
    {
        Serial.println("ERROR !!!");
    }
}

void device_control()
{
    if (smarthome.LCD_status)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(smarthome.LCD_line1.c_str());
        lcd.setCursor(0, 1);
        lcd.print(smarthome.LCD_line2.c_str());
    }
    else
        lcd.clear();
    digitalWrite(LED1pin, smarthome.LED1_status);
    digitalWrite(LED2pin, smarthome.LED2_status);
    digitalWrite(LED3pin, smarthome.LED3_status);
    digitalWrite(MOTORpin, !smarthome.MOTOR_speed);
    if(smarthome.door) myservo.write(120);
    else myservo.write(20);
}

void get_datetime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }

    String month = (timeinfo.tm_mon + 1) >= 10 ? String(timeinfo.tm_mon + 1) : ('0' + String(timeinfo.tm_mon + 1));
    date_n = String(timeinfo.tm_mday) + ":" + month + ":" + String(timeinfo.tm_year + 1900);
    Serial.println(date_n);
    sum_time = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    time_n = String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min);
    Serial.println(time_n);
}

void send_rfid(String ID)
{
    //  String pat = "/Deviot/" + ID + "/Work-Time/" + date_n + "/" + String(t);
    //   String path = "/RFID/" + ID + "/" + date_n + "/" + time_n;
    String path = "/RFID/history/" + date_n + "/" + String(sum_time);
    Firebase.set(data, path, ID);
    path = "/RFID/user/" + ID +  "/timeline/" + date_n + "/" + time_n;
    Firebase.set(data, path, "open door");
                  // String s = "{\"" + String(t) + "\": \"oke\"}";
                  // json.setJsonData(s);
                  // Serial.println(s);
                  //  Firebase.set(data, path, "done");

    //               if (Firebase.set(data, path, "oke"))
    // {
    //     Serial.println("SET JSON --------------------");
    //     Serial.println("PASSED");
    //     Serial.println("PATH: " + data.dataPath());
    //     Serial.println("TYPE: " + data.dataType());
    //     Serial.println("ETag: " + data.ETag());
    //     Serial.println();
    // }
    // else
    // {
    //     Serial.println("FAILED");
    //     Serial.println("REASON: " + data.errorReason());
    //     Serial.println("------------------------------------");
    //     Serial.println();
    // }
}

void check_rfid()
{
    // kiem tra xem co the nao duoc quet khong
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
    {
        led2_flash();
        get_datetime();
        Serial.print(F("PICC type: "));
        MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
        Serial.println(rfid.PICC_GetTypeName(piccType));

        // Check is the PICC of Classic MIFARE type
        if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
            piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
            piccType != MFRC522::PICC_TYPE_MIFARE_4K)
        {
            Serial.println(F("Your tag is not of type MIFARE Classic."));
            return;
        }

        String UID = String(rfid.uid.uidByte[0]);
        for (int i = 1; i < rfid.uid.size; i++)
        {
            UID += ":";
            UID += String(rfid.uid.uidByte[i]);
        }
        Serial.println(UID);
        Serial.println();


        myservo.write(smarthome.door ? 20 : 120);
        smarthome.door = !smarthome.door;

        send_rfid(UID);

        // Halt PICC  (tam dung PICC)
        rfid.PICC_HaltA();

        // Stop encryption on PCD (dung ma hoa PICC)
        rfid.PCD_StopCrypto1();
    }
}

void send_dht11()
{
    String path = "/DHT11/humidity";
    Firebase.set(data, path, String(dht11_h));
    path = "/DHT11/temperature";
    Firebase.set(data, path, String(dht11_t));
}
