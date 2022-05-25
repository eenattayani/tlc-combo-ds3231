/* 25/01/22 - perbaiki interval untuk 1 detik -> 930
 *          - matikan serial monitor
 *          
 *          - tlc-ptk-5.1_new_board-14-2-22 : rancang untuk jadwal khusus hari sabtu, minggu, tanggal merah
 * 24/03/22 - perbaiki cek_serial(): format data lampu keluar dan data masuk "(isidata)"
 * 
 * 26/03/22 - buat pengaturan tanggal merah
 *          
 * 25/05/22 - ganti menggunakan RTC DS3231         
 *  
 *  
 *  
 */

void(* auto_reset) (void) = 0;

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

//#include <virtuabotixRTC.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>

#include <DS3231.h>

#define r1 43
#define y1 42
#define g1 41
#define r2 40
#define y2 39
#define g2 38
#define r3 37
#define y3 36
#define g3 35
#define r4 34
#define y4 33
#define g4 32
#define pdR1 31
#define pdG1 30
#define pdR2 29
#define pdG2 28
#define pdR3 27
#define pdG3 26
#define pdR4 25
#define pdG4 24
#define fl1 23
#define fl2 22

#define on HIGH
#define off LOW
#define jm 0
#define mn 1
#define manFlash A0

#define tcp Serial1

const byte addrSgState = 0; // alamat EEPROM status ON/OFF Signal Group 4 slot ( 0 - 3 )
const byte addrDurR = 8;    // alamat EEPROM durasi AllRed
const byte addrDurY = 9;    // alamat EEPROM durasi lampu kuning & flashing
const byte addrDurG = 10;   // alamat EEPROM durasi green 36 slot ( 10 - 45 )
const byte addrJadwalStart = 100; // alamat EEPROM jam mulai normal & mulai jam sibuk 1 - 8  ( 100 - 116 )
const byte addrJadwalEnd = 120; // alamat EEPROM jam mulai flashing & akhir jam sibuk 1 - 8  ( 120 - 137 )
const byte addrHariLibur = 140; // alamat EEPROM hari libur 1-7 ( 140 - 146 )
const byte addrTglMerah = 150;  // alamat EEPROM 32 tanggal merah dalam satu tahun ( 150 - 213 )

const int addrLog = 1000;
byte dataLog;

int alamatEEPROM;

// setting nRF
RF24 rf24(48, 49); // CE, CSN
char pesan[]="00000000000000"; // 14 digit
int bacaLampu[14];
uint8_t alamat[][6] = {"1Node"}; // 5 bytes

LiquidCrystal_I2C lcd(0x27, 16, 2); //0x3F / 0x27

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'a'},
    {'4', '5', '6', 'b'},
    {'7', '8', '9', 'c'},
    {'*', '0', '#', 'd'}};
byte rowPins[ROWS] = {9, 10, 11, 12};
byte colPins[COLS] = {5, 6, 7, 8};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//Inisialisasi pin RTC (CLK, DAT, RST) DS1302
//virtuabotixRTC myRTC(4, 3, 2);

// Init the DS3231 using the hardware interface
DS3231  myRTC(SDA, SCL);
// Init a Time-data structure
Time  t;

String nama_hari[8] = {"===", "SEN", "SEL", "RAB", "KAM", "JUM", "SAB", "MIN"};
int hari, tanggal, bulan, tahun, jam, menit, detik;

char tombol;

bool sgState[4] = {on, on, on, on};

byte currSg;     // signal group sekarang
byte currSc;     // schedule sekarang

int durR;
int durY;
int durG[9][4]; // durasi hijau durG[0][] untuk durasi jadwal normal; durG[1-8][] untuk durasi jam sibuk

int counterNormalDurasi;
int counterNormalLampu;
int counterNormal;
int counterFlash;
char modeTL = 'a';
bool webState = on;
bool menuState = off;
bool flashState = off;
bool keluar = false;
const int autoOut = 60;
int counter;
int counterWeb;
int interval;
unsigned long currMillis;
unsigned long prevMillis;
unsigned long currMillisWeb;
unsigned long prevMillisWeb;
int intervalNrf;
unsigned long currMillisNrf;
unsigned long prevMillisNrf;

int incomingByte;
int delayWeb = 2000;

//byte startJadwal[9][2] = {{4, 56}, {5, 0}, {6, 10}, {7, 30}, {12, 0}, {16, 0}, {18, 30}, {22, 0}, {23, 0}};
//byte endJadwal[9][2] = {{23, 55}, {6, 10}, {7, 30}, {12, 0}, {16, 0}, {18, 30}, {22, 0}, {23, 0}, {23, 50}};

byte startJadwal[9][2] = {{4,57}};  // nilai default jam mulai normal
byte endJadwal[9][2] = {{23,30}};   // nilai default jam mulai flashing

// tersedia 30 slot untuk tanggal merah
byte tglmerahTanggal[30];
byte tglmerahBulan[30];

String kode[5][3] = {
    {"11", "1", "12"},
    {"21", "2", "22"},
    {"31", "3", "32"},
    {"41", "4", "42"},
    {"5", "50", "52"}
};

void setup()
{  
  byte logPage = cek_log();
  
  alamatEEPROM = 0;

  currSc = 0;

  counterNormalDurasi = 1;
  counterNormalLampu = 1;
  counterNormal = 1;
  counterFlash = 1;

  counter = 0;
  counterWeb = 0;
  interval = 930;
  currMillis = 0;
  prevMillis = 0;
  currMillisWeb = 0;
  prevMillisWeb = 0;
  intervalNrf = 200;
  currMillisNrf = 0;
  prevMillisNrf = 0;
  
  incomingByte = 0;

  
  
  for (int i = fl2; i <= r1; i++)
  {
    pinMode(i, OUTPUT);
  }

  pinMode(manFlash, INPUT);

  tcp.begin(19200);
  lcd.begin();

  Serial.begin(9600);
  delay(100);

  // Initialize the rtc object
  myRTC.begin();
  
  Serial.println("log page = " + String(logPage));
  Serial.println("jumlah on = " + String(dataLog));
  Serial.println("TLC-PTK 5.0");

  lcd.setCursor(0, 0);
  lcd.print("# TLC-PTK  5.0 #");
  lcd.setCursor(0, 1);
  lcd.print("#   01.2021    #");

  delay(1000);

  baca_alamat();

  tampil_waktu();

  all_off();

  baca_jadwal_start();

  baca_jadwal_end();

  baca_sg_state();

  baca_durasi();

  baca_jadwal();

  baca_tglmerah();

  delay(1000);

  flashing_awal();
}

void loop()
{
  menuState = off;

  // per detik
  tampil_waktu();
  if ( modeTL != 'b' ) lampu_flashing();
  
  lcd.setCursor(0, 1);
  lcd.print(currSc); // jadwal ke 0-8 ; 0 = jadwal normal
  lcd.setCursor(1, 1);
  lcd.print(modeTL);

  baca_traffic();     //-- selalu dipanggil
//  cek_serial();
//  tampil_durasi();    //-- tampilkan durasi di lcd

  currMillis = millis();
  prevMillis = currMillis;
  while (currMillis - prevMillis < interval)
  {
    currMillisNrf = currMillis;
    // instan
    if ( currMillisNrf - prevMillisNrf > intervalNrf ) {
      kirim_nrf();
      prevMillisNrf = currMillisNrf;   
    }
    
    cek_serial();
    baca_keypad();
    baca_flash_manual();
    currMillis = millis();

  }
  counter += 1;
  
  Serial.print("counter: ");Serial.println(counter);

  // per siklus / menit
  if (counter >= autoOut) 
  {
    baca_jadwal();

    counter = 0;
    lcd.noBacklight();
    baca_alamat();
  }
}

// ------------ == fungsi - fungsi == --------------- //
void kirim_nrf()
{
  bacaLampu[0] = digitalRead(r1);
  bacaLampu[1] = digitalRead(y1);
  bacaLampu[2] = digitalRead(g1);
  bacaLampu[3] = digitalRead(r2);
  bacaLampu[4] = digitalRead(y2);
  bacaLampu[5] = digitalRead(g2);
  bacaLampu[6] = digitalRead(r3);
  bacaLampu[7] = digitalRead(y3);
  bacaLampu[8] = digitalRead(g3);
  bacaLampu[9] = digitalRead(r4);
  bacaLampu[10] = digitalRead(y4);
  bacaLampu[11] = digitalRead(g4);
  bacaLampu[12] = digitalRead(fl1);
  bacaLampu[13] = digitalRead(fl2);

  for (int i = 0; i < sizeof(bacaLampu)/2; i++) {
    if (bacaLampu[i] == 1) {pesan[i] = '1';} else {pesan[i] = '0';}
  }

  rf24.write(&pesan,sizeof(pesan));

//  Serial.println(pesan);
//  Serial.println(durY);
//  Serial.println(durR);
//  Serial.println(counterFlash);
}

void baca_alamat()
{
  rf24.begin();
  delay(100);
  rf24.openWritingPipe(alamat[0]);
//  rf24.setPALevel(RF24_PA_MIN);
//  rf24.setPALevel(RF24_PA_LOW);
//  rf24.setPALevel(RF24_PA_HIGH);
  rf24.setPALevel(RF24_PA_MAX);
  rf24.stopListening();
  Serial.print("alamat nrf: ");
  Serial.println(alamat[0][0]);
  Serial.print("alamat nrf: ");
  Serial.print(alamat[0][0], HEX);
  Serial.print(alamat[0][1], HEX);
  Serial.print(alamat[0][2], HEX);
  Serial.print(alamat[0][3], HEX);
  Serial.println(alamat[0][4], HEX);
}

void tampil_durasi()
{
//  lcd.setCursor(5, 1);
  lcd.print(counterNormalDurasi);
//  lcd.setCursor(6, 1);
//  lcd.print(counterNormalLampu);
}

void baca_traffic()
{
  if (modeTL == 'a')
  { // a. mode normal
    normal();
  }
  else if (modeTL == 'b')
  { // b. mode flashing
    flashing();
  }
  else if (modeTL == 'c')
  { // c. mode hijau 1 statis
    hijau_satu_statis();
  }
  else if (modeTL == 'd')
  { // d. mode hijau 2 statis
    hijau_dua_statis();
  }
  else if (modeTL == 'e')
  { // e. mode hijau 3 statis
    hijau_tiga_statis();
  }
  else if (modeTL == 'f')
  { // f. mode hijau 4 statis
    hijau_empat_statis();
  }

  else
  {
    padam();
  }

  kirim_nrf();
}

void statis_kirim_nrf(int det)
{
  int sec = det * 5;
  for(int x = 0; x <= sec;  x++)
  {
    kirim_nrf();
    delay(intervalNrf); //200ms
  }
}

void hijau_satu_statis()
{
  webState = on;
  if (digitalRead(g1) == off)
  {
    lcd.clear();
    lcd.print("   ...wait...");
    lcd.setCursor(0, 1);
    lcd.print("SG 1 hijau");

    if (digitalRead(g2) == on || digitalRead(y2) == on)
    {
      currSg = 1; // SG II
      dua_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
    }
    else if (digitalRead(g3) == on || digitalRead(y3) == on)
    {
      currSg = 2; // SG III
      tiga_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }
    else if (digitalRead(g4) == on || digitalRead(y4) == on)
    {
      currSg = 3; // SG IV
      empat_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }

    currSg = 0; // SG I
    satu_merkun();
    kirim_data(kode[currSg][0]);
    statis_kirim_nrf(2);
//    delay(2000);
    satu_hijau();
    kirim_data(kode[currSg][1]);
    statis_kirim_nrf(1);  
//    delay(1000);
  }
  else
  {
    currSg = 0; // SG I
    satu_hijau();
    kirim_data(kode[currSg][1]);
  }
}

void hijau_dua_statis()
{
  webState = on;
  if (digitalRead(g2) == off)
  {
    lcd.clear();
    lcd.print("   ...wait...");
    lcd.setCursor(0, 1);
    lcd.print("SG 2 hijau");

    if (digitalRead(g1) == on || digitalRead(y1) == on)
    {
      currSg = 0; // SG I
      satu_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }
    else if (digitalRead(g3) == on || digitalRead(y3) == on)
    {
      currSg = 2; // SG III
      tiga_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }
    else if (digitalRead(g4) == on || digitalRead(y4) == on)
    {
      currSg = 3; // SG IV
      empat_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }

    currSg = 1; // SG II
    dua_merkun();
    kirim_data(kode[currSg][0]);
    statis_kirim_nrf(2);
//    delay(2000);
    dua_hijau();
    kirim_data(kode[currSg][1]);
    statis_kirim_nrf(1);
//    delay(1000);
  }
  else
  {
    currSg = 1; // SG II
    dua_hijau();
    kirim_data(kode[currSg][1]);
  }
}

void hijau_tiga_statis()
{
  webState = on;
  if (digitalRead(g3) == off)
  {
    lcd.clear();
    lcd.print("   ...wait...");
    lcd.setCursor(0, 1);
    lcd.print("SG 3 hijau");

    if (digitalRead(g1) == on || digitalRead(y1) == on)
    {
      currSg = 0; // SG I
      satu_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }
    else if (digitalRead(g2) == on || digitalRead(y2) == on)
    {
      currSg = 1; // SG II
      dua_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }
    else if (digitalRead(g4) == on || digitalRead(y4) == on)
    {
      currSg = 3; // SG IV
      empat_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }

    currSg = 2; // SG III
    tiga_merkun();
    kirim_data(kode[currSg][0]);
    statis_kirim_nrf(2);
//    delay(2000);
    tiga_hijau();
    kirim_data(kode[currSg][1]);
    statis_kirim_nrf(1);
//    delay(1000);
  }
  else
  {
    currSg = 2; // SG III
    tiga_hijau();
    kirim_data(kode[currSg][1]);
  }
}

void hijau_empat_statis()
{
  webState = on;
  if (digitalRead(g4) == off)
  {
    lcd.clear();
    lcd.print("   ...wait...");
    lcd.setCursor(0, 1);
    lcd.print("SG 4 hijau");

    if (digitalRead(g1) == on || digitalRead(y1) == on)
    {
      currSg = 0; // SG I
      satu_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }
    else if (digitalRead(g2) == on || digitalRead(y2) == on)
    {
      currSg = 1; // SG II
      dua_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }
    else if (digitalRead(g3) == on || digitalRead(y3) == on)
    {
      currSg = 2; // SG III
      tiga_kuning();
      kirim_data(kode[currSg][2]);
      statis_kirim_nrf(2);
//      delay(2000);
      all_red();
      kirim_data(kode[4][0]);
      statis_kirim_nrf(2);
//      delay(2000);
    }

    currSg = 3; // SG IV
    empat_merkun();
    kirim_data(kode[currSg][0]);
    statis_kirim_nrf(2);
//    delay(2000);
    empat_hijau();
    kirim_data(kode[currSg][1]);
    statis_kirim_nrf(1);
//    delay(1000);
  }
  else
  {
    currSg = 3; // SG IV
    empat_hijau();
    kirim_data(kode[currSg][1]);
  }
}

void padam() {}

void normal()
{
  // SG 1
  if (counterNormal == 1)
  {
    currSg = 0;
    if (sgState[currSg] == on)
    {

      if (counterNormalLampu == 1)
      {
        all_red();
        kirim_data(kode[4][0]);

        pd_merah(1);

        pd_hijau(2);
        pd_hijau(3);
        pd_hijau(4);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durR)
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 2)
      {
        satu_merkun();
        kirim_data(kode[currSg][0]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durY)
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 3)
      {
        satu_hijau();
        kirim_data(kode[currSg][1]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durG[currSc][currSg])
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 4)
      {
        satu_kuning();
        kirim_data(kode[currSg][2]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durY)
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 1;
          counterNormal += 1;
        }
      }
    }
    else
    {
      
      all_red();
      kirim_data(kode[4][0]);

      counterNormalDurasi += 1;

      counterNormal += 1;
    }
  }

  // SG 2
  else if (counterNormal == 2)
  {
    currSg = 1;
    if (sgState[currSg] == on)
    {
      if (counterNormalLampu == 1)
      {
        all_red();
        kirim_data(kode[4][0]);

        pd_merah(2);
        
        pd_hijau(1);
        pd_hijau(3);
        pd_hijau(4);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durR)
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 2)
      {
        dua_merkun();
        kirim_data(kode[currSg][0]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durY)
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 3)
      {
        dua_hijau();
        kirim_data(kode[currSg][1]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durG[currSc][currSg])
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 4)
      {
        dua_kuning();
        kirim_data(kode[currSg][2]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durY)
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 1;
          counterNormal += 1;
        }
      }
    }
    else
    {
      all_red();
      kirim_data(kode[4][0]);

      counterNormalDurasi += 1;

      counterNormal += 1;
    }
  }

  // SG 3
  else if (counterNormal == 3)
  {
    currSg = 2;
    if (sgState[currSg] == on)
    {
      if (counterNormalLampu == 1)
      {
        all_red();
        kirim_data(kode[4][0]);
        
        pd_merah(3);

        pd_hijau(1);
        pd_hijau(2);
        pd_hijau(4);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durR)
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 2)
      {
        tiga_merkun();
        kirim_data(kode[currSg][0]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durY)
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 3)
      {
        tiga_hijau();
        kirim_data(kode[currSg][1]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durG[currSc][currSg])
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 4)
      {
        tiga_kuning();
        kirim_data(kode[currSg][2]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durY)
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 1;
          counterNormal += 1;
        }
      }
    }
    else
    {
      all_red();
      kirim_data(kode[4][0]);

      counterNormalDurasi += 1;

      counterNormal += 1;
    }
  }

  // SG 4
  else if (counterNormal == 4)
  {
    currSg = 3;
    if (sgState[currSg] == on)
    {
      if (counterNormalLampu == 1)
      {
        all_red();
        kirim_data(kode[4][0]);
        
        pd_merah(4);

        pd_hijau(1);
        pd_hijau(2);
        pd_hijau(3);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durR)
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 2)
      {
        empat_merkun();
        kirim_data(kode[currSg][0]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durY)
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 3)
      {
        empat_hijau();
        kirim_data(kode[currSg][1]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durG[currSc][currSg])
        {
          counterNormalDurasi = 0;
          counterNormalLampu += 1;
        }
      }
      else if (counterNormalLampu == 4)
      {
        empat_kuning();
        kirim_data(kode[currSg][2]);

        counterNormalDurasi += 1;
        if (counterNormalDurasi > durY)
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 1;
          counterNormal += 1;
        }
      }
    }
    else
    {
      all_red();
      kirim_data(kode[4][0]);

      counterNormalDurasi += 1;

      counterNormal += 1;
    }
  }

  if (counterNormal > 4)
    counterNormal = 1;
}

void baca_flash_manual()
{
  if (digitalRead(manFlash) == on)
  {
    while (digitalRead(manFlash) == on)
    {
      reset_nilai_mode_normal();
      
      // digitalRead(manFlash);
      lcd.clear();
      all_off();
      delay(1000);

      lcd.print("   FLASHING...");
      digitalWrite(y1, on);
      digitalWrite(y2, on);
      digitalWrite(y3, on);
      digitalWrite(y4, on);
      digitalWrite(fl1, on);
      digitalWrite(fl2, on);
      delay(1000);
    }
  }
}

void flashing_awal()
{
  for (int i = 1; i <= 5; i++)
  {
    digitalWrite(y1, on);
    digitalWrite(y2, on);
    digitalWrite(y3, on);
    digitalWrite(y4, on);
    digitalWrite(fl1, on);
    digitalWrite(fl2, on);
    kirim_data(kode[4][2]);
//    tcp.println(kode[4][2]);
    kirim_nrf();
    delay(1000);

    digitalWrite(y1, off);
    digitalWrite(y2, off);
    digitalWrite(y3, off);
    digitalWrite(y4, off);
    digitalWrite(fl1, off);
    digitalWrite(fl2, off);
    kirim_data(kode[4][1]);
//    tcp.println(kode[4][1]);
    kirim_nrf();
    delay(1000);
  }
}

void flashing()
{
  reset_nilai_mode_normal();
  
  if (counterFlash <= durY)
  {
    all_off();
    kirim_data(kode[4][1]);
//    Serial.println("flash off");
  }
  else
  {
    digitalWrite(y1, on);
    digitalWrite(y2, on);
    digitalWrite(y3, on);
    digitalWrite(y4, on);
    digitalWrite(fl1, on);
    digitalWrite(fl2, on);
//    Serial.print("flash on  ");
//    Serial.println(counterFlash);
    kirim_data(kode[4][2]);
  }
  counterFlash += 1;
  if (counterFlash > durY * 2)
    counterFlash = 1;
}

void lampu_flashing()
{
  if ( digitalRead(fl1) == off ) {
    digitalWrite(fl1, on);
    digitalWrite(fl2, on);
  } else {
    digitalWrite(fl1, off);
    digitalWrite(fl2, off);
  }
}

void reset_nilai_mode_normal()
{
  counterNormalDurasi = 0;
  counterNormalLampu = 1;
  counterNormal = 1;
}

void set_mode_tl()
{
  int kursor = 0;

  masuk_menu("MODE MANUAL");

  lcd.print("1.NORM  2.FLASH");
  lcd.setCursor(0, 1);
  lcd.print("3.I     4.II ");

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':
        if (modeTL == 'b')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 1;
          counterNormal = 1;
        }
        else if (modeTL == 'c')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 4;
          counterNormal = 1;
        }
        else if (modeTL == 'd')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 4;
          counterNormal = 2;
        }
        else if (modeTL == 'e')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 4;
          counterNormal = 3;
        }
        else if (modeTL == 'f')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 4;
          counterNormal = 4;
        }

        modeTL = 'a';
        masuk_menu("Mode Normal ON");
        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");

        keluar = true;
      break;
      case '2':
        modeTL = 'b';
        masuk_menu("Mode Flashing ON");
        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");

        keluar = true;
      break;
      case '3':
        modeTL = 'c';

        hijau_satu_statis();

        masuk_menu("Mode SG 1 hijau");
        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        keluar = true;
      break;
      case '4':
        modeTL = 'd';

        hijau_dua_statis();
        
        masuk_menu("Mode SG 2 hijau");
        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        keluar = true;
      break;
      case '5':
        modeTL = 'e';

        hijau_tiga_statis();

        masuk_menu("Mode SG 3 hijau");
        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        keluar = true;
      break;
      case '6':
        modeTL = 'f';

        hijau_empat_statis();

        masuk_menu("Mode SG 4 hijau");
        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        keluar = true;
      break;
      case '7':
        masuk_menu("Data tidak ada..");
        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        
        keluar = true;
      break;
      case '8':
        masuk_menu("Data tidak ada..");
        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
      
        keluar = true;
      break;
      case '9':
        masuk_menu("Data tidak ada..");
        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
      
        keluar = true;
      break;
      case 'a':
        kembali();

        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
      break;
      case 'b':
        kembali();

        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
      break;
      case 'c':
        kursor -= 1;

        if (kursor < 0)
          kursor = 0;

        lcd.clear();
        lcd.print("1.NORM  2.FLASH");
        lcd.setCursor(0, 1);
        lcd.print("3.I     4.II ");
      break;
      case 'd':
        kursor += 1;

        if (kursor > 1)
          kursor = 1;

        lcd.clear();
        lcd.print("5.III   6.IV");
        lcd.setCursor(0, 1);
        lcd.print("7.V     8.VI ");
      break;
      default:

      break;
      }
    }
  } while (keluar == false);

  keluar = false;
}

void menu_awal()
{
  menuState = on;
  int kursor = 0;

  masuk_menu("PENGATURAN");

  lcd.print("1.MODE  2.DURASI");
  lcd.setCursor(0, 1);
  lcd.print("3.JADWAL 4.TIME ");

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':
        set_mode_tl();
        
        break;
      case '2':
        set_durasi();

        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        break;
      case '3':
        set_jadwal();

        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        break;
      case '4':
        set_waktu();

        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        break;
      case '5':
        set_sg_state();

        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        break;
      case 'a':
        kembali();
        break;
      case 'b':
        kembali();
        break;
      case 'c':
        kursor -= 1;

        if (kursor < 0)
          kursor = 0;

        lcd.clear();
        lcd.print("1.MODE  2.DURASI");
        lcd.setCursor(0, 1);
        lcd.print("3.JADWAL 4.TIME ");
        break;
      case 'd':
        kursor += 1;

        if (kursor > 1)
          kursor = 1;

        lcd.clear();
        lcd.print("5.SG STATE     ");
        lcd.setCursor(0, 1);
        lcd.print("               ");
        break;
      default:

        break;
      }
    }
  } while (keluar == false);

  keluar = false;
}

void set_durasi_merah_kuning()
{
  int nilaiDurR = durR;
  int nilaiDurY = durY;
  Serial.println(nilaiDurR);
  Serial.println(nilaiDurY);
  
  masuk_menu("SET MERAH KUNING");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" MERAH   KUNING ");
  
  lcd.setCursor(2, 1);
  lcd.print(nilaiDurR);
  lcd.print("  ");
  lcd.setCursor(11, 1);
  lcd.print(nilaiDurY);
  lcd.print("  ");

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':

      break;
      case '2':

      break;
      case '3':

      break;
      case '4':

      break;
      case '5':

      break;
      case '6':

      break;
      case '7':

      break;
      case '*':

      break;
      case '8':

      break;
      case '0':

      break;
      case '9':
        nilaiDurR += 1;
        if ( nilaiDurR > 20 ) nilaiDurR = 0;
        
        lcd.setCursor(2, 1);
        lcd.print(nilaiDurR);
        lcd.print("  ");
      break;
      case '#':
        nilaiDurR -= 1;
        if ( nilaiDurR < 0 ) nilaiDurR = 20;
        
        lcd.setCursor(2, 1);
        lcd.print(nilaiDurR);
        lcd.print("  ");
      break;
      case 'a':
        batal();
      break;
      case 'b':
        alamatEEPROM = addrDurR;
        EEPROM.update(alamatEEPROM, nilaiDurR);
        durR = nilaiDurR;

        alamatEEPROM = 0;
        
        alamatEEPROM = addrDurY;
        EEPROM.update(alamatEEPROM, nilaiDurY);
        durY = nilaiDurY;
        
        Serial.print(durR);
        Serial.print("  ");
        Serial.print(durY);
          
        alamatEEPROM = 0;
      
        simpan();
      break;
      case 'c':
        nilaiDurY += 1;
        if ( nilaiDurY > 5 ) nilaiDurY = 0;
        
        lcd.setCursor(11, 1);
        lcd.print(nilaiDurY);
        lcd.print("  ");
      break;
      case 'd':
        nilaiDurY -= 1;
        if ( nilaiDurY < 0 ) nilaiDurY = 5;
        
        lcd.setCursor(11, 1);
        lcd.print(nilaiDurY);
        lcd.print("  ");
      break;
      default:

      break;
      }
    }
  } while (keluar == false);
  
  keluar = false;
}

void set_durasi_selected(int durasiSelected)
{
  int nilaiDur1 = durG[durasiSelected][0];
  int nilaiDur2 = durG[durasiSelected][1];
  int nilaiDur3 = durG[durasiSelected][2];
  int nilaiDur4 = durG[durasiSelected][3];

  Serial.println(nilaiDur1);
  Serial.println(nilaiDur2);
  Serial.println(nilaiDur3);
  Serial.println(nilaiDur4);
  
//  masuk_menu("SET DURASI");
  lcd.clear();
  if ( durasiSelected == 0 ) {
    lcd.print("DURASI NORMAL   ");
  } else { 
    lcd.print("DURASI JADWAL ");
    lcd.setCursor(14,0);
    lcd.print(durasiSelected);
  }
  
  lcd.setCursor(0, 1);
  lcd.print(nilaiDur1);
  lcd.print(" ");
  lcd.setCursor(4, 1);
  lcd.print(nilaiDur2);
  lcd.print(" ");
  lcd.setCursor(8, 1);
  lcd.print(nilaiDur3);
  lcd.print(" ");
  lcd.setCursor(12, 1);
  lcd.print(nilaiDur4);
  lcd.print(" ");

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':

        keluar = true;
      break;
      case '2':

        keluar = true;
      break;
      case '3':

        keluar = true;
      break;
      case '4':

        keluar = true;
      break;
      case '5':

        keluar = true;
      break;
      case '6':

        keluar = true;
      break;
      case '7':
        nilaiDur1 += 1;
        if ( nilaiDur1 > 120 ) nilaiDur1 = 0;
        
        lcd.setCursor(0, 1);
        lcd.print(nilaiDur1);
        lcd.print(" ");
        if ( nilaiDur1 < 10 ) lcd.print(" ");
      break;
      case '*':
        nilaiDur1 -= 1;
        if ( nilaiDur1 < 0 ) nilaiDur1 = 120;
        
        lcd.setCursor(0, 1);
        lcd.print(nilaiDur1);
        lcd.print(" ");
        if ( nilaiDur1 < 10 ) lcd.print(" ");
      break;
      case '8':
        nilaiDur2 += 1;
        if ( nilaiDur2 > 120 ) nilaiDur2 = 0;
        
        lcd.setCursor(4, 1);
        lcd.print(nilaiDur2);
        lcd.print(" "); 
        if ( nilaiDur2 < 10 ) lcd.print(" ");
      break;
      case '0':
        nilaiDur2 -= 1;
        if ( nilaiDur2 < 0 ) nilaiDur2 = 120;
        
        lcd.setCursor(4, 1);
        lcd.print(nilaiDur2);
        lcd.print(" ");
        if ( nilaiDur2 < 10 ) lcd.print(" ");
      break;
      case '9':
        nilaiDur3 += 1;
        if ( nilaiDur3 > 120 ) nilaiDur3 = 0;
        
        lcd.setCursor(8, 1);
        lcd.print(nilaiDur3);
        lcd.print(" ");
        if ( nilaiDur3 < 10 ) lcd.print(" ");
      break;
      case '#':
        nilaiDur3 -= 1;
        if ( nilaiDur3 < 0 ) nilaiDur3 = 120;
        
        lcd.setCursor(8, 1);
        lcd.print(nilaiDur3);
        lcd.print(" ");
        if ( nilaiDur3 < 10 ) lcd.print(" ");
      break;
      case 'a':
        batal();
      break;
      case 'b':
        alamatEEPROM = addrDurG;
        alamatEEPROM = addrDurG + ( durasiSelected * 4 );
        EEPROM.update(alamatEEPROM, nilaiDur1);
        EEPROM.update(alamatEEPROM + 1, nilaiDur2);
        EEPROM.update(alamatEEPROM + 2, nilaiDur3);
        EEPROM.update(alamatEEPROM + 3, nilaiDur4);

        durG[durasiSelected][0] = nilaiDur1;
        durG[durasiSelected][1] = nilaiDur2;
        durG[durasiSelected][2] = nilaiDur3;
        durG[durasiSelected][3] = nilaiDur4;

        Serial.print(durG[durasiSelected][0]);
        Serial.print("  ");
        Serial.print(durG[durasiSelected][1]);
        Serial.print("  ");
        Serial.print(durG[durasiSelected][2]);
        Serial.print("  ");
        Serial.print(durG[durasiSelected][3]);
          
        alamatEEPROM = 0;
      
        simpan();
      break;
      case 'c':
        nilaiDur4 += 1;
        if ( nilaiDur4 > 120 ) nilaiDur4 = 0;
        
        lcd.setCursor(12, 1);
        lcd.print(nilaiDur4);
        lcd.print(" "); 
        if ( nilaiDur4 < 10 ) lcd.print(" ");
      break;
      case 'd':
        nilaiDur4 -= 1;
        if ( nilaiDur4 < 0 ) nilaiDur4 = 120;
        
        lcd.setCursor(12, 1);
        lcd.print(nilaiDur4);
        lcd.print(" ");
        if ( nilaiDur4 < 10 ) lcd.print(" ");
      break;
      default:

      break;
      }
    }
  } while (keluar == false);
  
  keluar = false;
}

void view_for_set_durasi(int kursor)
{
  if ( kursor != 4 ) {
    int baris1 = (kursor * 2) + 1;  
    int baris2 = (kursor * 2) + 2;
  
    lcd.setCursor(4,0);
    lcd.print(" ");
    if ( startJadwal[baris1][jm] < 10 ) lcd.print("0");
    lcd.print(startJadwal[baris1][jm]);
    lcd.print(":");
    if ( startJadwal[baris1][mn] < 10 ) lcd.print("0");
    lcd.print(startJadwal[baris1][mn]);
    lcd.print("-");
    if ( endJadwal[baris1][jm] < 10 ) lcd.print("0");
    lcd.print(endJadwal[baris1][jm]);
    lcd.print(":");
    if ( endJadwal[baris1][mn] < 10 ) lcd.print("0");
    lcd.print(endJadwal[baris1][mn]);
  
    lcd.setCursor(4,1);
    lcd.print(" ");
    if ( startJadwal[baris2][jm] < 10 ) lcd.print("0");
    lcd.print(startJadwal[baris2][jm]);
    lcd.print(":");
    if ( startJadwal[baris2][mn] < 10 ) lcd.print("0");
    lcd.print(startJadwal[baris2][mn]);
    lcd.print("-");
    if ( endJadwal[baris2][jm] < 10 ) lcd.print("0");
    lcd.print(endJadwal[baris2][jm]);
    lcd.print(":");
    if ( endJadwal[baris2][mn] < 10 ) lcd.print("0");
    lcd.print(endJadwal[baris2][mn]);
  }
}

void set_durasi()
{
  int kursor = 0;

  masuk_menu("ATUR DURASI");

  lcd.print("1.Dur Jadwal 1  ");
  lcd.setCursor(0, 1);
  lcd.print("2.Dur Jadwal 2  ");
  view_for_set_durasi(kursor);

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':
        set_durasi_selected(1);

        lcd.clear();
        lcd.print("1.Dur Jadwal 1");
        lcd.setCursor(0, 1);
        lcd.print("2.Dur Jadwal 2");

        kursor = 0;
        view_for_set_durasi(kursor);
      break;
      case '2':
        set_durasi_selected(2);

        lcd.clear();
        lcd.print("1.Dur Jadwal 1");
        lcd.setCursor(0, 1);
        lcd.print("2.Dur Jadwal 2");

        kursor = 0;
        view_for_set_durasi(kursor);
      break;
      case '3':
        set_durasi_selected(3);

        lcd.clear();
        lcd.print("3.Dur Jadwal 3");
        lcd.setCursor(0, 1);
        lcd.print("4.Dur Jadwal 4");

        kursor = 1;
        view_for_set_durasi(kursor);
      break;
      case '4':
        set_durasi_selected(4);

        lcd.clear();
        lcd.print("3.Dur Jadwal 3");
        lcd.setCursor(0, 1);
        lcd.print("4.Dur Jadwal 4");

        kursor = 1;
        view_for_set_durasi(kursor);
      break;
      case '5':
        set_durasi_selected(5);

        lcd.clear();
        lcd.print("5.Dur Jadwal 5");
        lcd.setCursor(0, 1);
        lcd.print("6.Dur Jadwal 6");

        kursor = 2;
        view_for_set_durasi(kursor);
      break;
      case '6':
        set_durasi_selected(6);

        lcd.clear();
        lcd.print("5.Dur Jadwal 5");
        lcd.setCursor(0, 1);
        lcd.print("6.Dur Jadwal 6");

        kursor = 2;
        view_for_set_durasi(kursor);
      break;
      case '7':
        set_durasi_selected(7);

        lcd.clear();
        lcd.print("7.Dur Jadwal 7");
        lcd.setCursor(0, 1);
        lcd.print("8.Dur Jadwal 8");

        kursor = 3;
        view_for_set_durasi(kursor);
      break;
      case '8':
        set_durasi_selected(8);

        lcd.clear();
        lcd.print("7.Dur Jadwal 7");
        lcd.setCursor(0, 1);
        lcd.print("8.Dur Jadwal 8");

        kursor = 3;
        view_for_set_durasi(kursor);
      break;
      case '9':
        set_durasi_merah_kuning();

        lcd.clear();
        lcd.print("9.All Red/Yellow");
        lcd.setCursor(0, 1);
        lcd.print("0.Dur Normal");

        kursor = 4;
      break;
      case '0':
        set_durasi_selected(0);

        lcd.clear();
        lcd.print("9.All Red/Yellow");
        lcd.setCursor(0, 1);
        lcd.print("0.Dur Normal");

        kursor = 4;
      break;
      case 'a':
        kembali();
      break;
      case 'b':
        kembali();
      break;
      case 'c':
        kursor -= 1;

        if ( kursor < 0 ) kursor = 0;
        lcd.clear();
        
        if ( kursor == 0 ) {
          lcd.print("1.Dur Jadwal 1");
          lcd.setCursor(0, 1);
          lcd.print("2.Dur Jadwal 2");
        } else if ( kursor == 1 ){
          lcd.print("3.Dur Jadwal 3");
          lcd.setCursor(0, 1);
          lcd.print("4.Dur Jadwal 4");
        } else if ( kursor == 2 ){
          lcd.print("5.Dur Jadwal 5");
          lcd.setCursor(0, 1);
          lcd.print("6.Dur Jadwal 6");
        } else if ( kursor == 3 ){
          lcd.print("7.Dur Jadwal 7");
          lcd.setCursor(0, 1);
          lcd.print("8.Dur Jadwal 8");
        } else if ( kursor == 4 ){
          lcd.print("9.All Red/Yellow");
          lcd.setCursor(0, 1);
          lcd.print("0.Dur Normal");
        }   
           
        view_for_set_durasi(kursor);
      break;
      case 'd':
        kursor += 1;

        if (kursor > 4) kursor = 4;
        lcd.clear();

        if ( kursor == 0 ) {
          lcd.print("1.Dur Jadwal 1");
          lcd.setCursor(0, 1);
          lcd.print("2.Dur Jadwal 2");
        } else if ( kursor == 1 ){
          lcd.print("3.Dur Jadwal 3");
          lcd.setCursor(0, 1);
          lcd.print("4.Dur Jadwal 4");
        } else if ( kursor == 2 ){
          lcd.print("5.Dur Jadwal 5");
          lcd.setCursor(0, 1);
          lcd.print("6.Dur Jadwal 6");
        } else if ( kursor == 3 ){
          lcd.print("7.Dur Jadwal 7");
          lcd.setCursor(0, 1);
          lcd.print("8.Dur Jadwal 8");
        } else if ( kursor == 4 ){
          lcd.print("9.All Red/Yellow");
          lcd.setCursor(0, 1);
          lcd.print("0.Dur Normal");
        }
        view_for_set_durasi(kursor); 
      break;
      default:

      break;
      }
    }
  } while (keluar == false);
  
  keluar = false;
}

void set_jadwal_normal()
{
  int nilaiJam = startJadwal[0][0];
  int nilaiMenit = startJadwal[0][1];

  Serial.println(nilaiJam);
  Serial.println(nilaiMenit);
  Serial.println(startJadwal[0][0]);
  Serial.println(startJadwal[0][1]);
  
  masuk_menu("SET JAM NORMAL");

  lcd.print("   JAM NORMAL   ");
  lcd.setCursor(0, 1);
  lcd.print("9/#  ");
  lcd.setCursor(5, 1);
  if ( nilaiJam < 10 ) lcd.print("0");
  lcd.print(nilaiJam);
  lcd.print(":");
  lcd.setCursor(8, 1);
  if ( nilaiMenit < 10 ) lcd.print("0");
  lcd.print(nilaiMenit);
  lcd.setCursor(12, 1);
  lcd.print("C/D ");

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':

        keluar = true;
      break;
      case '2':

        keluar = true;
      break;
      case '3':

        keluar = true;
      break;
      case '4':

        keluar = true;
      break;
      case '5':

        keluar = true;
      break;
      case '6':

        keluar = true;
      break;
      case '7':
        keluar = true;
      break;
      case '9':
        nilaiJam += 1;
        if ( nilaiJam > 23 ) nilaiJam = 0;
        
        lcd.setCursor(5, 1);
        if ( nilaiJam < 10 ) lcd.print("0");
        lcd.print(nilaiJam);
      break;
      case '#':
        nilaiJam -= 1;
        if ( nilaiJam < 0 ) nilaiJam = 23;
        
        lcd.setCursor(5, 1);
        if ( nilaiJam < 10 ) lcd.print("0");
        lcd.print(nilaiJam);
      break;
      case 'a':
        batal();
      break;
      case 'b':
        alamatEEPROM = addrJadwalStart;
        
        EEPROM.update(alamatEEPROM, nilaiJam);
        EEPROM.update(alamatEEPROM + 1, nilaiMenit);

        startJadwal[0][0] = nilaiJam;
        startJadwal[0][1] = nilaiMenit;
          
        alamatEEPROM = 0;
      
        simpan();
      break;
      case 'c':
        nilaiMenit += 1;
        if ( nilaiMenit > 59 ) nilaiMenit = 0;
        
        lcd.setCursor(8, 1);
        if ( nilaiMenit < 10 ) lcd.print("0");
        lcd.print(nilaiMenit); 
      break;
      case 'd':
        nilaiMenit -= 1;
        if ( nilaiMenit < 0 ) nilaiMenit = 59;
        
        lcd.setCursor(8, 1);
        if ( nilaiMenit < 10 ) lcd.print("0");
        lcd.print(nilaiMenit);
      break;
      default:

      break;
      }
    }
  } while (keluar == false);
  
  keluar = false;
}

void set_jadwal_malam()
{
  int nilaiJam = endJadwal[0][0];
  int nilaiMenit = endJadwal[0][1];

  Serial.println(nilaiJam);
  Serial.println(nilaiMenit);
  Serial.println(endJadwal[0][0]);
  Serial.println(endJadwal[0][1]);
  
  masuk_menu("SET JAM MALAM");

  lcd.print("   JAM MALAM   ");
  lcd.setCursor(0, 1);
  lcd.print("9/#  ");
  lcd.setCursor(5, 1);
  if ( nilaiJam < 10 ) lcd.print("0");
  lcd.print(nilaiJam);
  lcd.print(":");
  lcd.setCursor(8, 1);
  if ( nilaiMenit < 10 ) lcd.print("0");
  lcd.print(nilaiMenit);
  lcd.setCursor(12, 1);
  lcd.print("C/D ");

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':

        keluar = true;
      break;
      case '2':

        keluar = true;
      break;
      case '3':

        keluar = true;
      break;
      case '4':

        keluar = true;
      break;
      case '5':

        keluar = true;
      break;
      case '6':

        keluar = true;
      break;
      case '7':
        keluar = true;
      break;
      case '9':
        nilaiJam += 1;
        if ( nilaiJam > 23 ) nilaiJam = 0;
        
        lcd.setCursor(5, 1);
        if ( nilaiJam < 10 ) lcd.print("0");
        lcd.print(nilaiJam);
      break;
      case '#':
        nilaiJam -= 1;
        if ( nilaiJam < 0 ) nilaiJam = 23;
        
        lcd.setCursor(5, 1);
        if ( nilaiJam < 10 ) lcd.print("0");
        lcd.print(nilaiJam);
      break;
      case 'a':
        batal();
      break;
      case 'b':
        alamatEEPROM = addrJadwalEnd;
        
        EEPROM.update(alamatEEPROM, nilaiJam);
        EEPROM.update(alamatEEPROM + 1, nilaiMenit);

        endJadwal[0][0] = nilaiJam;
        endJadwal[0][1] = nilaiMenit;
          
        alamatEEPROM = 0;
      
        simpan();
      break;
      case 'c':
        nilaiMenit += 1;
        if ( nilaiMenit > 59 ) nilaiMenit = 0;
        
        lcd.setCursor(8, 1);
        if ( nilaiMenit < 10 ) lcd.print("0");
        lcd.print(nilaiMenit); 
      break;
      case 'd':
        nilaiMenit -= 1;
        if ( nilaiMenit < 0 ) nilaiMenit = 59;
        
        lcd.setCursor(8, 1);
        if ( nilaiMenit < 10 ) lcd.print("0");
        lcd.print(nilaiMenit);
      break;
      default:

      break;
      }
    }
  } while (keluar == false);
  
  keluar = false;
}

void view_for_set_jadwal(int kursor)
{
  int baris1 = (kursor * 2) + 1;  
  int baris2 = (kursor * 2) + 2;
  
  lcd.setCursor(2,0);
  if ( startJadwal[baris1][jm] < 10 ) lcd.print("0");
  lcd.print(startJadwal[baris1][jm]);
  lcd.print(":");
  if ( startJadwal[baris1][mn] < 10 ) lcd.print("0");
  lcd.print(startJadwal[baris1][mn]);
  lcd.print("-");
  if ( endJadwal[baris1][jm] < 10 ) lcd.print("0");
  lcd.print(endJadwal[baris1][jm]);
  lcd.print(":");
  if ( endJadwal[baris1][mn] < 10 ) lcd.print("0");
  lcd.print(endJadwal[baris1][mn]);

  lcd.setCursor(2,1);
  if ( startJadwal[baris2][jm] < 10 ) lcd.print("0");
  lcd.print(startJadwal[baris2][jm]);
  lcd.print(":");
  if ( startJadwal[baris2][mn] < 10 ) lcd.print("0");
  lcd.print(startJadwal[baris2][mn]);
  lcd.print("-");
  if ( endJadwal[baris2][jm] < 10 ) lcd.print("0");
  lcd.print(endJadwal[baris2][jm]);
  lcd.print(":");
  if ( endJadwal[baris2][mn] < 10 ) lcd.print("0");
  lcd.print(endJadwal[baris2][mn]);
}

void set_tglmerah(int tglmerah)
{
  
}

void view_for_set_tglmerah(int kursor)
{
  int tgl1 = tglmerahTanggal[(kursor*2) + 1];
  int bln1 = tglmerahBulan[(kursor*2) + 1];
  int tgl2 = tglmerahTanggal[kursor + 1];
  int bln2 = tglmerahBulan[kursor];
  int tgl3 = tglmerahTanggal[kursor];
  int bln3 = tglmerahBulan[kursor];
  int tgl4 = tglmerahTanggal[kursor];
  int bln4 = tglmerahBulan[kursor];
  
  lcd.clear();                

  if ( kursor == 0 ) {
    lcd.print("a.--/-- b.--/--");
    lcd.setCursor(0, 1);
    lcd.print("c.--/-- d.--/--");
  } else if ( kursor == 1 ){
    lcd.print("e.--/-- f.--/--");
    lcd.setCursor(0, 1);
    lcd.print("g.--/-- h.--/--");
  } else if ( kursor == 2 ){
    lcd.print("i.--/-- j.--/--");
    lcd.setCursor(0, 1);
    lcd.print("k.--/-- l.--/--");
  } else if ( kursor == 3 ){
    lcd.print("m.--/-- n.--/--");
    lcd.setCursor(0, 1);
    lcd.print("o.--/-- p.--/--");
  } else if ( kursor == 4 ){
    lcd.print("q.--/-- r.--/--");
    lcd.setCursor(0, 1);
    lcd.print("s.--/-- t.--/--");
  } else if ( kursor == 5 ){
    lcd.print("u.--/-- v.--/--");
    lcd.setCursor(0, 1);
    lcd.print("w.--/-- x.--/--");
  } else if ( kursor == 6 ){
    lcd.print("y.--/-- z.--/--");
    lcd.setCursor(0, 1);
    lcd.print("A.--/-- B.--/--");
  } else if ( kursor == 7 ){
    lcd.print("C.--/-- D.--/--");
    lcd.setCursor(0, 1);
    lcd.print("E.--/-- F.--/--");
  } else if ( kursor == 8 ){
    lcd.print("G.--/-- H.--/--");
    lcd.setCursor(0, 1);
    lcd.print("I.--/-- J.--/--");
  }
  
  lcd.setCursor(2,0);
  if ( tgl1 < 10 ) lcd.print("0");
  lcd.print(tgl1);
  lcd.print("/");
  if ( bln1 < 10 ) lcd.print("0");
  lcd.print(bln1);
  
  lcd.setCursor(10,0);
  if ( tgl2 < 10 ) lcd.print("0");
  lcd.print(tgl2);
  lcd.print("/");
  if ( bln2 < 10 ) lcd.print("0");
  lcd.print(bln2);

  lcd.setCursor(2,1);
  if ( tgl3 < 10 ) lcd.print("0");
  lcd.print(tgl3);
  lcd.print("/");
  if ( bln3 < 10 ) lcd.print("0");
  lcd.print(bln3);
  
  lcd.setCursor(10,1);
  if ( tgl4 < 10 ) lcd.print("0");
  lcd.print(tgl4);
  lcd.print("/");
  if ( bln4 < 10 ) lcd.print("0");
  lcd.print(bln4);
}

void pilih_set_jadwal_tglmerah()
{
  int kursor = 0;

  masuk_menu(" TANGGAL MERAH");

  view_for_set_tglmerah(kursor);

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':
        set_tglmerah(1);        
        kursor = 0;
        view_for_set_tglmerah(kursor);
      break;
      case '2':
        set_tglmerah(2);      
        kursor = 0;
        view_for_set_tglmerah(kursor);
      break;
      case '3':
        set_tglmerah(3);
        kursor = 0;
        view_for_set_tglmerah(kursor);
      break;
      case '4':
        set_tglmerah(4);
        kursor = 0;
        view_for_set_tglmerah(kursor);
      break;
      case '5':
        set_tglmerah(5);
        kursor = 1;
        view_for_set_tglmerah(kursor);
      break;
      case '6':
        set_tglmerah(6);
        kursor = 1;
        view_for_set_tglmerah(kursor);
      break;
      case '7':
        set_tglmerah(7);
        kursor = 1;
        view_for_set_tglmerah(kursor);
      break;
      case '8':
        set_tglmerah(8);
        kursor = 1;
        view_for_set_tglmerah(kursor);
      break;
      case '9':
        keluar = true;
      break;
      case 'a':
        kembali();
      break;
      case 'b':
        kembali();
      break;
      case 'c':
        kursor -= 1;
        if ( kursor < 0 ) kursor = 8;
        
        view_for_set_tglmerah(kursor);
      break;
      case 'd':
        kursor += 1;        
        if ( kursor > 8 ) kursor = 0;
        
        view_for_set_tglmerah(kursor);
      break;
      default:

      break;
      }
    }
  } while (keluar == false);
  
  keluar = false;
}

void pilih_set_jadwal_sibuk()
{
  int kursor = 0;

  masuk_menu("ATUR JADWAL");

  lcd.print("1.Jadwal 1");
  lcd.setCursor(0, 1);
  lcd.print("2.Jadwal 2");

  view_for_set_jadwal(kursor);

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':
        set_jadwal_sibuk(1);

        lcd.clear();
        lcd.print("1.Jadwal 1");
        lcd.setCursor(0, 1);
        lcd.print("2.Jadwal 2");
        
        kursor = 0;
        view_for_set_jadwal(kursor);
      break;
      case '2':
        set_jadwal_sibuk(2);

        lcd.clear();
        lcd.print("1.Jadwal 1");
        lcd.setCursor(0, 1);
        lcd.print("2.Jadwal 2");

        kursor = 0;
        view_for_set_jadwal(kursor);
      break;
      case '3':
        set_jadwal_sibuk(3);

        lcd.clear();
        lcd.print("3.Jadwal 3");
        lcd.setCursor(0, 1);
        lcd.print("4.Jadwal 4");

        kursor = 1;
        view_for_set_jadwal(kursor);
      break;
      case '4':
        set_jadwal_sibuk(4);

        lcd.clear();
        lcd.print("3.Jadwal 3");
        lcd.setCursor(0, 1);
        lcd.print("4.Jadwal 4");

        kursor = 1;
        view_for_set_jadwal(kursor);
      break;
      case '5':
        set_jadwal_sibuk(5);

        lcd.clear();
        lcd.print("5.Jadwal 5");
        lcd.setCursor(0, 1);
        lcd.print("6.Jadwal 6");

        kursor = 2;
        view_for_set_jadwal(kursor);
      break;
      case '6':
        set_jadwal_sibuk(6);

        lcd.clear();
        lcd.print("5.Jadwal 5");
        lcd.setCursor(0, 1);
        lcd.print("6.Jadwal 6");

        kursor = 2;
        view_for_set_jadwal(kursor);
      break;
      case '7':
        set_jadwal_sibuk(7);

        lcd.clear();
        lcd.print("7.Jadwal 7");
        lcd.setCursor(0, 1);
        lcd.print("8.Jadwal 8");

        kursor = 3;
        view_for_set_jadwal(kursor);
      break;
      case '8':
        set_jadwal_sibuk(8);

        lcd.clear();
        lcd.print("7.Jadwal 7");
        lcd.setCursor(0, 1);
        lcd.print("8.Jadwal 8");

        kursor = 3;
        view_for_set_jadwal(kursor);
      break;
      case '9':
        keluar = true;
      break;
      case 'a':
        kembali();
      break;
      case 'b':
        kembali();
      break;
      case 'c':
        kursor -= 1;

        if ( kursor < 0 ) kursor = 0;
        lcd.clear();
        
        if ( kursor == 0 ) {
          lcd.print("1.JADWAL 1      ");
          lcd.setCursor(0, 1);
          lcd.print("2.JADWAL 2      ");
        } else if ( kursor == 1 ){
          lcd.print("3.JADWAL 3      ");
          lcd.setCursor(0, 1);
          lcd.print("4.JADWAL 4      ");
        } else if ( kursor == 2 ){
          lcd.print("5.JADWAL 5      ");
          lcd.setCursor(0, 1);
          lcd.print("6.JADWAL 6      ");
        } else if ( kursor == 3 ){
          lcd.print("7.JADWAL 7      ");
          lcd.setCursor(0, 1);
          lcd.print("8.JADWAL 8      ");
        } 
        view_for_set_jadwal(kursor);
      break;
      case 'd':
        kursor += 1;

        if (kursor > 3) kursor = 3;
        lcd.clear();

        if ( kursor == 0 ) {
          lcd.print("1.JADWAL 1      ");
          lcd.setCursor(0, 1);
          lcd.print("2.JADWAL 2      ");
        } else if ( kursor == 1 ){
          lcd.print("3.JADWAL 3      ");
          lcd.setCursor(0, 1);
          lcd.print("4.JADWAL 4      ");
        } else if ( kursor == 2 ){
          lcd.print("5.JADWAL 5      ");
          lcd.setCursor(0, 1);
          lcd.print("6.JADWAL 6      ");
        } else if ( kursor == 3 ){
          lcd.print("7.JADWAL 7      ");
          lcd.setCursor(0, 1);
          lcd.print("8.JADWAL 8      ");
        }
        view_for_set_jadwal(kursor);
      break;
      default:

      break;
      }
    }
  } while (keluar == false);
  
  keluar = false;
}

void set_jadwal_sibuk(int jamSibuk)
{
  int nilaiJamStart = startJadwal[jamSibuk][0];
  int nilaiMenitStart = startJadwal[jamSibuk][1];
  int nilaiJamEnd = endJadwal[jamSibuk][0];
  int nilaiMenitEnd = endJadwal[jamSibuk][1];

  Serial.println(nilaiJamStart);
  Serial.println(nilaiMenitStart);
  Serial.println(endJadwal[jamSibuk][0]);
  Serial.println(endJadwal[jamSibuk][1]);
  
  masuk_menu_jadwal("SET JADWAL",jamSibuk);

  lcd.print(" JADWAL  ");
  lcd.setCursor(8,0);
  lcd.print(jamSibuk);
  
  lcd.setCursor(1, 1);
  if ( nilaiJamStart < 10 ) lcd.print("0");
  lcd.print(nilaiJamStart);
  lcd.setCursor(3, 1);
  lcd.print(":");
  lcd.setCursor(4, 1);
  if ( nilaiMenitStart < 10 ) lcd.print("0");
  lcd.print(nilaiMenitStart);
  lcd.setCursor(6, 1);
  lcd.print("-");
  lcd.setCursor(7, 1);
  if ( nilaiJamEnd < 10 ) lcd.print("0");
  lcd.print(nilaiJamEnd);
  lcd.setCursor(9, 1);
  lcd.print(":");
  lcd.setCursor(10, 1);
  if ( nilaiMenitEnd < 10 ) lcd.print("0");
  lcd.print(nilaiMenitEnd);

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':

        keluar = true;
      break;
      case '2':

        keluar = true;
      break;
      case '3':

        keluar = true;
      break;
      case '4':

        keluar = true;
      break;
      case '5':

        keluar = true;
      break;
      case '6':

        keluar = true;
      break;
      case '7':
        nilaiJamStart += 1;
        if ( nilaiJamStart > 23 ) nilaiJamStart = 0;
        
        lcd.setCursor(1, 1);
        if ( nilaiJamStart < 10 ) lcd.print("0");
        lcd.print(nilaiJamStart);
      break;
      case '*':
        nilaiJamStart -= 1;
        if ( nilaiJamStart < 0 ) nilaiJamStart = 23;
        
        lcd.setCursor(1, 1);
        if ( nilaiJamStart < 10 ) lcd.print("0");
        lcd.print(nilaiJamStart);
      break;
      case '8':
        nilaiMenitStart += 1;
        if ( nilaiMenitStart > 59 ) nilaiMenitStart = 0;
        
        lcd.setCursor(4, 1);
        if ( nilaiMenitStart < 10 ) lcd.print("0");
        lcd.print(nilaiMenitStart); 
      break;
      case '0':
        nilaiMenitStart -= 1;
        if ( nilaiMenitStart < 0 ) nilaiMenitStart = 59;
        
        lcd.setCursor(4, 1);
        if ( nilaiMenitStart < 10 ) lcd.print("0");
        lcd.print(nilaiMenitStart);
      break;
      case '9':
        nilaiJamEnd += 1;
        if ( nilaiJamEnd > 23 ) nilaiJamEnd = 0;
        
        lcd.setCursor(7, 1);
        if ( nilaiJamEnd < 10 ) lcd.print("0");
        lcd.print(nilaiJamEnd);
      break;
      case '#':
        nilaiJamEnd -= 1;
        if ( nilaiJamEnd < 0 ) nilaiJamEnd = 23;
        
        lcd.setCursor(7, 1);
        if ( nilaiJamEnd < 10 ) lcd.print("0");
        lcd.print(nilaiJamEnd);
      break;
      case 'a':
        batal();
      break;
      case 'b':
        alamatEEPROM = addrJadwalStart;
        
        EEPROM.update(alamatEEPROM + ( jamSibuk * 2 ), nilaiJamStart);
        EEPROM.update(alamatEEPROM + ( jamSibuk * 2 ) + 1, nilaiMenitStart);

        startJadwal[jamSibuk][0] = nilaiJamStart;
        startJadwal[jamSibuk][1] = nilaiMenitStart;

        Serial.print(startJadwal[jamSibuk][0]);
        Serial.print(":");
        Serial.println(startJadwal[jamSibuk][1]);
          
        alamatEEPROM = 0;
      
        alamatEEPROM = addrJadwalEnd;
        
        EEPROM.update(alamatEEPROM + ( jamSibuk * 2 ), nilaiJamEnd);
        EEPROM.update(alamatEEPROM + ( jamSibuk * 2 ) + 1, nilaiMenitEnd);

        endJadwal[jamSibuk][0] = nilaiJamEnd;
        endJadwal[jamSibuk][1] = nilaiMenitEnd;

        Serial.print(endJadwal[jamSibuk][0]);
        Serial.print(":");
        Serial.println(endJadwal[jamSibuk][1]);
          
        alamatEEPROM = 0;
      
        simpan();
      break;
      case 'c':
        nilaiMenitEnd += 1;
        if ( nilaiMenitEnd > 59 ) nilaiMenitEnd = 0;
        
        lcd.setCursor(10, 1);
        if ( nilaiMenitEnd < 10 ) lcd.print("0");
        lcd.print(nilaiMenitEnd); 
      break;
      case 'd':
        nilaiMenitEnd -= 1;
        if ( nilaiMenitEnd < 0 ) nilaiMenitEnd = 59;
        
        lcd.setCursor(10, 1);
        if ( nilaiMenitEnd < 10 ) lcd.print("0");
        lcd.print(nilaiMenitEnd);
      break;
      default:

      break;
      }
    }
  } while (keluar == false);
  
  keluar = false;
}

void set_jadwal()
{
  int kursor = 0;

  masuk_menu("ATUR JADWAL");

  lcd.print("1.NORMAL      ");
  lcd.setCursor(0, 1);
  lcd.print("2.MALAM/FLASHING");

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':
        set_jadwal_normal();

        lcd.clear();
        lcd.print("1.NORMAL      ");
        lcd.setCursor(0, 1);
        lcd.print("2.MALAM/FLASHING");
      break;
      case '2':
        set_jadwal_malam();

        lcd.clear();
        lcd.print("1.NORMAL      ");
        lcd.setCursor(0, 1);
        lcd.print("2.MALAM/FLASHING");
      break;
      case '3':
        pilih_set_jadwal_sibuk();

        lcd.clear();
        lcd.print("3.WAKTU JADWAL  ");
        lcd.setCursor(0, 1);
        lcd.print("4.HARI LIBUR    ");
      break;
      case '4':

        keluar = true;
      break;
      case '5':
        pilih_set_jadwal_tglmerah();

        lcd.clear();
        lcd.print("5.TANGGAL MERAH ");
        lcd.setCursor(0, 1);
        lcd.print("                ");
      break;
      case '6':

        keluar = true;
      break;
      case '7':
        keluar = true;
      break;
      case '8':
        keluar = true;
      break;
      case '9':
        keluar = true;
      break;
      case 'a':
        kembali();
      break;
      case 'b':
        kembali();
      break;
      case 'c':
        kursor -= 1;

        if ( kursor < 0 ) kursor = 0;
        lcd.clear();
        
        if ( kursor == 0 ) {
          lcd.print("1.NORMAL        ");
          lcd.setCursor(0, 1);
          lcd.print("2.MALAM/FLASHING");
        } else if ( kursor == 1 ){
          lcd.print("3.WAKTU JADWAL  ");
          lcd.setCursor(0, 1);
          lcd.print("4.HARI LIBUR    ");
        } else if ( kursor == 2 ){
          lcd.print("5.TANGGAL MERAH ");
          lcd.setCursor(0, 1);
          lcd.print("                ");
        } 
      break;
      case 'd':
        kursor += 1;

        if (kursor > 2) kursor = 2;
        lcd.clear();

        if ( kursor == 0 ) {
          lcd.print("1.NORMAL        ");
          lcd.setCursor(0, 1);
          lcd.print("2.MALAM/FLASHING");
        } else if ( kursor == 1 ){
          lcd.print("3.WAKTU JADWAL  ");
          lcd.setCursor(0, 1);
          lcd.print("4.HARI LIBUR    ");
        } else if ( kursor == 2 ){
          lcd.print("5.TANGGAL MERAH ");
          lcd.setCursor(0, 1);
          lcd.print("                ");
        }
      break;
      default:

      break;
      }
    }
  } while (keluar == false);
  
  keluar = false;
}

void start_counter()
{
  currMillis = millis();
  if (currMillis - prevMillis > interval)
  {
    baca_traffic();
    lampu_flashing();

    prevMillis = currMillis;
    counter += 1;
    // Serial.println(counter);

    if (counter > autoOut)
    {
      counter = 0;
      keluar = true;
    }
  }
}

void start_counter_web()
{
  currMillisWeb = millis();
  if (currMillisWeb - prevMillisWeb > interval)
  {
    baca_traffic();

    lcd.clear();
    lcd.print("wait data web...");
    lcd.setCursor(0,1);
    lcd.print(counterWeb);
    
    prevMillisWeb = currMillisWeb;
    counterWeb += 1;
    // Serial.println(counter);
  }
}

void print_lcd(bool state) //-- untuk pengaturan status SG => on / off 
{
  if (state == on)
    lcd.print("on ");
  else
    lcd.print("off ");
}

void set_sg_state() //-- aktif/nonaktifkan SG
{
  baca_sg_state();

  masuk_menu("ATUR SG");

  lcd.print("SG1=    SG3=");
  lcd.setCursor(0, 1);
  lcd.print("SG2=    SG4=");
  lcd.setCursor(4, 0);
  print_lcd(sgState[0]);
  lcd.setCursor(12, 0);
  print_lcd(sgState[2]);
  lcd.setCursor(4, 1);
  print_lcd(sgState[1]);
  lcd.setCursor(12, 1);
  print_lcd(sgState[3]);

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':
        if (sgState[0] == on)
        {
          sgState[0] = off;
          lcd.setCursor(4, 0);
          print_lcd(sgState[0]);
        }
        else
        {
          sgState[0] = on;
          lcd.setCursor(4, 0);
          print_lcd(sgState[0]);
        }
        break;
      case '2':
        if (sgState[1] == on)
        {
          sgState[1] = off;
          lcd.setCursor(4, 1);
          print_lcd(sgState[1]);
        }
        else
        {
          sgState[1] = on;
          lcd.setCursor(4, 1);
          print_lcd(sgState[1]);
        }
        break;
      case '3':
        if (sgState[2] == on)
        {
          sgState[2] = off;
          lcd.setCursor(12, 0);
          print_lcd(sgState[2]);
        }
        else
        {
          sgState[2] = on;
          lcd.setCursor(12, 0);
          print_lcd(sgState[2]);
        }
        break;
      case '4':
        if (sgState[3] == on)
        {
          sgState[3] = off;
          lcd.setCursor(12, 1);
          print_lcd(sgState[3]);
        }
        else
        {
          sgState[3] = on;
          lcd.setCursor(12, 1);
          print_lcd(sgState[3]);
        }
        break;
      case 'a':
        batal();
        break;
      case 'b':
        alamatEEPROM = addrSgState;
        for (int i = 0; i < sizeof(sgState); i++)
        {
          EEPROM.update(alamatEEPROM, sgState[i]);
          alamatEEPROM += 1;
        }
        alamatEEPROM = 0;

        simpan();
        break;
      default:

        break;
      }
    }
  } while (keluar == false);

  keluar = false;
}

char baca_keypad() //-- baca input keypad Halaman Utama
{
  tombol = keypad.getKey();
  if (tombol != NO_KEY)
  {
    counter = 0;
    lcd.backlight();
    Serial.print("tombol : ");
    Serial.println(tombol);

    switch (tombol)
    {
    case '*':
      menu_awal();
      break;
    case '#':
      menu_awal();
      break;
    default:
      break;
    }
  }
}


void notif_from_web(char notif[])
{
  Serial.print("Web: ");
  Serial.println(notif);
  lcd.clear();
  lcd.print("Web: ");
  lcd.setCursor(0,1);
  lcd.print(notif);
  delay(1000);
}

void cek_serial()  //-- cek input dari aplikasi web
{
  /** jika ada input dari web masuk, 
   *  cek apakah data dimulai dari tanda '(' (dec=40)
   *  jika ya, maka tampung semua data dalam tanda kurung ke dalam string
   *  hingga tanda ')' (dec=41) diterima
   *  jika tidak, maka abaikan data input
   */
  if( tcp.available() > 0 )
  {
      incomingByte = tcp.read();          

      if ( incomingByte == 40 )
      {
        prevMillisWeb = millis();
        String dataString = "";  
        dataString = dataString + char(incomingByte);
                      
        do {
          start_counter_web();                 
          if( tcp.available() > 0 )
          {
            counterWeb = 0;
            incomingByte = tcp.read();          
            dataString = dataString + char(incomingByte);
          }  
          if (counterWeb > 1)
          {
            counterWeb = 0;
            lcd.setCursor(0,1);
            lcd.print("connection lost..");
            Serial.println("Lost Connection...");
            delay(1000);            
            break;
          }        
        } while(incomingByte != 41);
  
        int strLength = dataString.length();
        char opener = dataString[0];
        char closer = dataString[strLength - 1];
        String data = dataString.substring(1,strLength - 1);
  
        Serial.print("opener: ");
        Serial.println(opener);
        Serial.print("data string: ");
        Serial.println(data);
  
        if ( opener == '(' ) 
        {
          if( data == "0" ){} 
          else {
            delay(500);
            cek_input(data);
          } 
        }
      }               
  }
}


void cek_input(String data)
{  
    Serial.print("data terima: ");
    Serial.println(data[0]);
    
    
    lcd.backlight();    
    webState = off; //-- nonaktifkan mode PENGIRIM via ethernet; karena sedang dalam mode PENERIMA
    
    switch (data[0])
    {
      case '1' : // nilai ASCII = 49
        if (modeTL == 'b')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 1;
          counterNormal = 1;
        }
        else if (modeTL == 'c')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 4;
          counterNormal = 1;
        }
        else if (modeTL == 'd')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 4;
          counterNormal = 2;
        }
        else if (modeTL == 'e')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 4;
          counterNormal = 3;
        }
        else if (modeTL == 'f')
        {
          counterNormalDurasi = 0;
          counterNormalLampu = 4;
          counterNormal = 4;
        }

        modeTL = 'a';
        
        notif_from_web("mode normal");
      break;
      case '2':
        modeTL = 'b';
        
        notif_from_web("mode flashing");
      break;
      case '3':
        modeTL = 'c';

        menuState = on;
        notif_from_web("mode hijau 1");
        hijau_satu_statis();
      break;
      case '4':
        modeTL = 'd';

        menuState = on;
        notif_from_web("mode hijau 2");
        hijau_dua_statis();
      break;
      case '5':
        modeTL = 'e';

        menuState = on;
        notif_from_web("mode hijau 3");
        hijau_tiga_statis();
      break;
      case '6':
        modeTL = 'f';

        menuState = on;
        notif_from_web("mode hijau 4");
        hijau_empat_statis();
      break;
      case 'a':
        for( int i = 0; i < 4; i++ ) {
          tcp.print(durG[0][i]);
          tcp.print(",");
        }
        delay(delayWeb);
      break;
      case 'b':
        for( int i = 0; i < 4; i++ ) {
          tcp.print(durG[1][i]);
          tcp.print(",");
        }
        delay(delayWeb);
      break;
      case 'c':
        for( int i = 0; i < 4; i++ ) {
          tcp.print(durG[2][i]);
          tcp.print(",");
        }
        delay(delayWeb);
      break;
      case 'd':
        for( int i = 0; i < 4; i++ ) {
          tcp.print(durG[3][i]);
          tcp.print(",");
        }
        delay(delayWeb);
      break;
      case 'e':
        for( int i = 0; i < 4; i++ ) {
          tcp.print(durG[4][i]);
          tcp.print(",");
        }
        delay(delayWeb);
      break;
      case 'f':
        for( int i = 0; i < 4; i++ ) {
          tcp.print(durG[5][i]);
          tcp.print(",");
        }
        delay(delayWeb);
      break;
      case 'g':
        for( int i = 0; i < 4; i++ ) {
          tcp.print(durG[6][i]);
          tcp.print(",");
        }
        delay(delayWeb);
      break;
      case 'h':
        for( int i = 0; i < 4; i++ ) {
          tcp.print(durG[7][i]);
          tcp.print(",");
        }
        delay(delayWeb);
      break;
      case 'i':
        for( int i = 0; i < 4; i++ ) {
          tcp.print(durG[8][i]);
          tcp.print(",");
        }
        delay(delayWeb);
      break;
      case 'z':
        Serial.println("baca dur allred yellow");       
        tcp.print(durR);
        tcp.print(",");
        tcp.print(durY);
        tcp.print(",");
        delay(delayWeb);
      break;
      case 'j': 
        simpan_jam_web(data);
      break;
      case 'k':
        Serial.println("baca jam");
        tcp.print(jam);
        tcp.print(",");
        tcp.print(menit);
        tcp.print(",");
        tcp.print(detik);
        tcp.print(",");
        delay(delayWeb);
      break;
      case 'l':
        tcp.print(startJadwal[0][0]);
        tcp.print(",");
        tcp.print(startJadwal[0][1]);
        tcp.print(",");
        delay(delayWeb);
      break;
      case 'm':
        tcp.print(endJadwal[0][0]);
        tcp.print(",");
        tcp.print(endJadwal[0][1]);
        tcp.print(",");
        delay(delayWeb);
      break;
      case 'n':
        send_baca_jam_sibuk(1);
      break;
      case 'o':
        send_baca_jam_sibuk(2);
      break;
      case 'p':
        send_baca_jam_sibuk(3);
      break;
      case 'q':
        send_baca_jam_sibuk(4);
      break;
      case 'r':
        send_baca_jam_sibuk(5);
      break;
      case 's':
        send_baca_jam_sibuk(6);
      break;
      case 't':
        send_baca_jam_sibuk(7);
      break;
      case 'u':
        send_baca_jam_sibuk(8);
      break;
      case 'A':
        simpan_durasi_web(0, data);
      break;
      case 'B':
        simpan_durasi_web(1, data);
      break;
      case 'C':
        simpan_durasi_web(2, data);
      break;
      case 'D':
        simpan_durasi_web(3, data);
      break;
      case 'E':
        simpan_durasi_web(4, data);
      break;
      case 'F':
        simpan_durasi_web(5, data);
      break;
      case 'G':
        simpan_durasi_web(6, data);
      break;
      case 'H':
        simpan_durasi_web(7, data);
      break;
      case 'I':
        simpan_durasi_web(8, data);
      break;
      case 'J': // atur jam normal
        simpan_jadwal_web(0, data);
      break;
      case 'K': // atur jam malam
        simpan_jadwal_web(1, data);
      break;
      case 'L': // atur jam sibuk
        simpan_jadwal_web(2, data);
      break;
      case 'Z':
        simpan_dur_red_yellow(data);
      break;
      default:
        Serial.println("ini default, kode tidak ada yang cocok");
      break;
    }

    // say what you got:
    Serial.print("I received: ");
    Serial.println(data[0]);
    
  webState = on;
}

void simpan_dur_red_yellow(String inputStringWeb)
{
  Serial.print("data durasi Red Yellow: ");
  Serial.println(inputStringWeb);
  
//  int nilaiDurR = durR;
//  int nilaiDurY = durY;


    int ix1 = inputStringWeb.indexOf(",");
    int ix2 = inputStringWeb.indexOf(".");
    
    int nilaiDurR = inputStringWeb.substring(1, ix1).toInt();
    int nilaiDurY = inputStringWeb.substring(ix1 + 1 , ix2).toInt();

    alamatEEPROM = addrDurR;
    EEPROM.update(alamatEEPROM, nilaiDurR);
    durR = nilaiDurR;

    alamatEEPROM = 0;
    
    alamatEEPROM = addrDurY;
    EEPROM.update(alamatEEPROM, nilaiDurY);
    durY = nilaiDurY;

    alamatEEPROM = 0;
    inputStringWeb = "";
}


void simpan_jadwal_web(int kodeScWeb, String inputStringWeb)
{
    Serial.print("input String:");
    Serial.println(inputStringWeb);
    
  int x = 0;
  int panjangStr = inputStringWeb.length();
  inputStringWeb = inputStringWeb.substring(1, panjangStr);    // hilangkan kode jadwal di depan

    Serial.print("input String:");
    Serial.println(inputStringWeb);
    
    String kodeSgWeb = inputStringWeb.substring(0,1);
    int startJam = inputStringWeb.substring(1,3).toInt();
    int startMenit = inputStringWeb.substring(3,5).toInt();
    int endJam = inputStringWeb.substring(5,7).toInt();
    int endMenit = inputStringWeb.substring(7,9).toInt();

    if ( kodeSgWeb == "A" ) x = 1;
    else if ( kodeSgWeb == "B" ) x = 2;
    else if ( kodeSgWeb == "C" ) x = 3;
    else if ( kodeSgWeb == "D" ) x = 4;
    else if ( kodeSgWeb == "E" ) x = 5;
    else if ( kodeSgWeb == "F" ) x = 6;
    else if ( kodeSgWeb == "G" ) x = 7;
    else if ( kodeSgWeb == "H" ) x = 8;
    else if ( kodeSgWeb == "I" ) x = 0;
    
    Serial.print("kode tiang: ");
    Serial.println(kodeSgWeb);
    Serial.print("nilai x: ");
    Serial.println(x);
    Serial.print("data Jam: ");
    Serial.print(startJam);
    Serial.print(":");
    Serial.print(startMenit);
    Serial.print(" - ");
    Serial.print(endJam);
    Serial.print(":");
    Serial.println(endMenit);
    Serial.print("kode Sc Web:");
    Serial.println(kodeScWeb);

    if ( kodeScWeb == 0 ) {
        alamatEEPROM = addrJadwalStart;
        EEPROM.update(alamatEEPROM, startJam);
        EEPROM.update(alamatEEPROM + 1, startMenit);
        startJadwal[0][0] = startJam;
        startJadwal[0][1] = startMenit;
    } else if (kodeScWeb == 1 ) {
        alamatEEPROM = addrJadwalEnd;
        EEPROM.update(alamatEEPROM, startJam);
        EEPROM.update(alamatEEPROM + 1, startMenit);
        endJadwal[0][0] = startJam;
        endJadwal[0][1] = startMenit;
    } else {
        alamatEEPROM = addrJadwalStart;
        alamatEEPROM = alamatEEPROM + ( 2 * x );
        EEPROM.update(alamatEEPROM, startJam);
        EEPROM.update(alamatEEPROM + 1, startMenit);
    
        alamatEEPROM = addrJadwalEnd;
        alamatEEPROM = alamatEEPROM + ( 2 * x );
        EEPROM.update(alamatEEPROM, endJam);
        EEPROM.update(alamatEEPROM + 1, endMenit);

        startJadwal[x][0] = startJam;
        startJadwal[x][1] = startMenit;
        endJadwal[x][0] = endJam;
        endJadwal[x][1] = endMenit;
    }


    alamatEEPROM = 0;
    inputStringWeb = "";
}


void simpan_durasi_web(int kodeScWeb, String inputStringWeb)
{
  Serial.print("data durasi: ");
  Serial.println(inputStringWeb);

  int panjangStr = inputStringWeb.length();
  inputStringWeb = inputStringWeb.substring(1, panjangStr - 1);    // hilangkan kode jadwal di depan
  
  int nilaiDur[4];
          
  for(int i = 0; i < 4; i++) {
    nilaiDur[i] = getValue(inputStringWeb, ',' , i).toInt();   
    Serial.print("nilai ");
    Serial.print(i);
    Serial.print(" ");
    Serial.println(nilaiDur[i]);
  }

  alamatEEPROM = addrDurG + (kodeScWeb * 4);
  Serial.println(alamatEEPROM);
  for( int i = 0; i < 4; i++) {
    EEPROM.update(alamatEEPROM + i, nilaiDur[i]);
    durG[kodeScWeb][i] = nilaiDur[i];
    Serial.print(durG[kodeScWeb][i]);
    Serial.print("  ");  
  }   

  alamatEEPROM = 0;
  inputStringWeb = "";
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;
 
  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  } 
 
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void simpan_jam_web(String dataJam)
{
    int jam = dataJam.substring(1,3).toInt();
    int menit = dataJam.substring(3,5).toInt();
    
//    myRTC.setDS1302Time(00, menit, jam, hari, tanggal, bulan, tahun);
    myRTC.setDOW(hari);
    myRTC.setTime(jam, menit, 0);
    myRTC.setDate(tanggal, bulan, tahun);
}

void send_baca_jam_sibuk(int jamSibuk)
{
  tcp.print(startJadwal[jamSibuk][0]);
  tcp.print(",");
  tcp.print(startJadwal[jamSibuk][1]);
  tcp.print(",");
  tcp.print(endJadwal[jamSibuk][0]);
  tcp.print(",");
  tcp.print(endJadwal[jamSibuk][1]);
  tcp.print(",");
  delay(delayWeb);
}

void cek_mode_tl(String dataMode)
{
  
}

void kirim_data(String data)
{
  String dataSend = "(" + data + ")";
  
  if ( webState == on ) {
    tcp.print(dataSend);
    Serial.print("dataSend: ");
    Serial.println(dataSend);
    Serial.println();
  }

  if (menuState == off)
  {
    lcd.setCursor(3, 1);
    lcd.print(data);
    lcd.print(" ");

    tampil_durasi();
  }
}

void all_red()
{
  digitalWrite(r1, on);
  digitalWrite(y1, off);
  digitalWrite(g1, off);
  digitalWrite(r2, on);
  digitalWrite(y2, off);
  digitalWrite(g2, off);
  digitalWrite(r3, on);
  digitalWrite(y3, off);
  digitalWrite(g3, off);
  digitalWrite(r4, on);
  digitalWrite(y4, off);
  digitalWrite(g4, off);
}

void pd_hijau(int pd)
{
  switch ( pd )
  {
    case 1 :
      hidup(pdG1);
      padam(pdR1);
    break;
    case 2 :
      hidup(pdG2);
      padam(pdR2);
    break;
    case 3 :
      hidup(pdG3);
      padam(pdR3);
    break;
    case 4 :
      hidup(pdG4);
      padam(pdR4);
    break;
    default:
    
    break;
  }
}

void pd_merah(int pd)
{
  switch ( pd )
  {
    case 1 :
      hidup(pdR1);
      padam(pdG1);
    break;
    case 2 :
      hidup(pdR2);
      padam(pdG2);
    break;
    case 3 :
      hidup(pdR3);
      padam(pdG3);
    break;
    case 4 :
      hidup(pdR4);
      padam(pdG4);
    break;
    default:
    
    break;
  }
}

void padam(int pin)
{
  digitalWrite(pin, off);
}

void hidup(int pin)
{
  digitalWrite(pin, on);
}

void satu_merkun()
{
  digitalWrite(r1, on);
  digitalWrite(y1, on);
  digitalWrite(g1, off);
}

void satu_hijau()
{
  digitalWrite(r1, off);
  digitalWrite(y1, off);
  digitalWrite(g1, on);
}

void satu_kuning()
{
  digitalWrite(r1, off);
  digitalWrite(y1, on);
  digitalWrite(g1, off);
}

void dua_merkun()
{
  digitalWrite(r2, on);
  digitalWrite(y2, on);
  digitalWrite(g2, off);
}

void dua_hijau()
{
  digitalWrite(r2, off);
  digitalWrite(y2, off);
  digitalWrite(g2, on);
}

void dua_kuning()
{
  digitalWrite(r2, off);
  digitalWrite(y2, on);
  digitalWrite(g2, off);
}

void tiga_merkun()
{
  digitalWrite(r3, on);
  digitalWrite(y3, on);
  digitalWrite(g3, off);
}

void tiga_hijau()
{
  digitalWrite(r3, off);
  digitalWrite(y3, off);
  digitalWrite(g3, on);
}

void tiga_kuning()
{
  digitalWrite(r3, off);
  digitalWrite(y3, on);
  digitalWrite(g3, off);
}

void empat_merkun()
{
  digitalWrite(r4, on);
  digitalWrite(y4, on);
  digitalWrite(g4, off);
}

void empat_hijau()
{
  digitalWrite(r4, off);
  digitalWrite(y4, off);
  digitalWrite(g4, on);
}

void empat_kuning()
{
  digitalWrite(r4, off);
  digitalWrite(y4, on);
  digitalWrite(g4, off);
}

void masuk_menu_jadwal(char menu[], int jad)
{
  keluar = false;
  lcd.clear();
  lcd.print(menu);
  lcd.print(" ");
  lcd.print(jad);
  delay(800);
  lcd.clear();
}

void masuk_menu(char menu[])
{
  keluar = false;
  lcd.clear();
  lcd.print(menu);
  delay(800);
  lcd.clear();
}

void simpan()
{
  keluar = true;
  lcd.clear();
  lcd.print("disimpan...");
  delay(500);
}

void batal()
{
  keluar = true;
  lcd.clear();
  lcd.print("dibatalkan...");
  delay(500);
}

void kembali()
{
  keluar = true;
  lcd.clear();
  lcd.print("kembali...");
  delay(500);
}

void set_waktu()
{
  masuk_menu("PENGATURAN WAKTU");

  //  lcd.setCursor(0,1);
  lcd.setCursor(0, 0);
  lcd.print("<");
  lcd.print(nama_hari[hari]);
  lcd.setCursor(4, 0);
  lcd.print("> ");

  if (tanggal < 10)
    lcd.print("0");
  lcd.print(tanggal);
  lcd.print("/");
  if (bulan < 10)
    lcd.print("0");
  lcd.print(bulan);
  lcd.print("/");
  lcd.print(tahun);

  lcd.setCursor(0, 1);
  lcd.print("      ");
  if (jam < 10)
    lcd.print("0");
  lcd.print(jam);
  lcd.print(":");
  if (menit < 10)
    lcd.print("0");
  lcd.print(menit);
  lcd.print(":");
  if (detik < 10)
    lcd.print("0");
  lcd.print(detik);
  lcd.print("  ");

  do
  {
    start_counter();

    char tombol = keypad.getKey();
    if (tombol != NO_KEY)
    {
      counter = 0;
      switch (tombol)
      {
      case '1':
        tanggal += 1;
        if (tanggal > 31)
          tanggal = 1;
        lcd.setCursor(6, 0);
        if (tanggal < 10)
          lcd.print("0");
        lcd.print(tanggal);
        break;
      case '4':
        tanggal -= 1;
        if (tanggal < 1)
          tanggal = 31;
        lcd.setCursor(6, 0);
        if (tanggal < 10)
          lcd.print("0");
        lcd.print(tanggal);
        break;
      case '2':
        bulan += 1;
        if (bulan > 12)
          bulan = 1;
        lcd.setCursor(9, 0);
        if (bulan < 10)
          lcd.print("0");
        lcd.print(bulan);
        break;
      case '5':
        bulan -= 1;
        if (bulan < 1)
          bulan = 12;
        lcd.setCursor(9, 0);
        if (bulan < 10)
          lcd.print("0");
        lcd.print(bulan);
        break;
      case '3':
        tahun += 1;
        if (tahun > 2100)
          tahun = 2010;
        lcd.setCursor(12, 0);
        lcd.print(tahun);
        break;
      case '6':
        tahun -= 1;
        if (tahun < 2010)
          tahun = 2100;
        lcd.setCursor(12, 0);
        lcd.print(tahun);
        break;
      case '7':
        jam += 1;
        if (jam > 23)
          jam = 0;
        lcd.setCursor(6, 1);
        if (jam < 10)
          lcd.print("0");
        lcd.print(jam);
        break;
      case '*':
        jam -= 1;
        if (jam < 0)
          jam = 23;
        lcd.setCursor(6, 1);
        if (jam < 10)
          lcd.print("0");
        lcd.print(jam);
        break;
      case '8':
        menit += 1;
        if (menit > 59)
          menit = 0;
        lcd.setCursor(9, 1);
        if (menit < 10)
          lcd.print("0");
        lcd.print(menit);
        break;
      case '0':
        menit -= 1;
        if (menit < 0)
          menit = 59;
        lcd.setCursor(9, 1);
        if (menit < 10)
          lcd.print("0");
        lcd.print(menit);
        break;
      case 'c':
        hari += 1;
        if (hari > 7)
          hari = 1;
        lcd.setCursor(1, 0);
        lcd.print(nama_hari[hari]);
        break;
      case 'd':
        hari -= 1;
        if (hari < 1)
          hari = 7;
        lcd.setCursor(1, 0);
        lcd.print(nama_hari[hari]);
        break;
      case 'a':
        batal();
        break;
      case 'b':
//        myRTC.setDS1302Time(00, menit, jam, hari, tanggal, bulan, tahun);
        myRTC.setDOW(hari);
        myRTC.setTime(jam, menit, 0);
        myRTC.setDate(tanggal, bulan, tahun);

        simpan();
        baca_jadwal();
        break;
      default:

        break;
      }
    }
  } while (keluar == false);

  //myRTC.setDS1302Time(00,8,12,1,23,03,2020);
  //detik, menit, jam, hari dalam seminggu, tanggal, bulan, tahun
  //00:59:23 "Jumat" 10-Oktober-2017

  keluar = false;
}

void baca_waktu()
{
//  myRTC.updateTime();
//  hari = myRTC.dayofweek;
//  tanggal = myRTC.dayofmonth;
//  bulan = myRTC.month;
//  tahun = myRTC.year;
//  jam = myRTC.hours;
//  menit = myRTC.minutes;
//  detik = myRTC.seconds;

  t = myRTC.getTime();
  hari = t.dow;
  if ( hari >= 7 ) hari = 0;
  tanggal = t.date;
  bulan = t.mon;
  tahun= t.year;
  jam = t.hour;
  menit =  t.min;
  detik = t.sec;

  cek_reset();
}

void tampil_waktu()
{
  baca_waktu();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("<");
  lcd.print(nama_hari[hari]);
  lcd.setCursor(4, 0);
  lcd.print("> ");
  if (tanggal < 10)
    lcd.print("0");
  lcd.print(tanggal);
  lcd.print("/");
  if (bulan < 10)
    lcd.print("0");
  lcd.print(bulan);
  lcd.print("/");
  lcd.print(tahun);

  lcd.setCursor(8, 1);
  if (jam < 10)
    lcd.print("0");
  lcd.print(jam);
  lcd.print(":");
  if (menit < 10)
    lcd.print("0");
  lcd.print(menit);
  lcd.print(":");
  if (detik < 10)
    lcd.print("0");
  lcd.print(detik);
  lcd.print("  ");
}

void all_off()
{
  for (int i = fl2; i <= r1; i++)
  {
    digitalWrite(i, off);
  }
}

void baca_durasi()
{
  alamatEEPROM = addrDurR;
  durR = EEPROM.read(alamatEEPROM);
  if (durR == 255) durR = 2;
//  Serial.print(alamatEEPROM);
//  Serial.print("\t");
//  Serial.println(durR);


  alamatEEPROM = addrDurY;
  durY = EEPROM.read(alamatEEPROM);
  if (durY == 255) durY = 2;
//  Serial.print(alamatEEPROM);
//  Serial.print("\t");
//  Serial.println(durY);
  
  
  alamatEEPROM = addrDurG;
  for (int i = 0; i < 9; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      durG[i][j] = EEPROM.read(alamatEEPROM);
      if (durG[i][j] == 255)
        durG[i][j] = 20;

//      Serial.print(alamatEEPROM);
//      Serial.print("\t");
//      Serial.println(durG[i][j]);
      alamatEEPROM += 1;
    }
  }
  alamatEEPROM = 0;
}

void baca_tglmerah()
{
  alamatEEPROM = addrTglMerah;
  for (int i = 0; i < 32; i++)
  {    
      int nilaiEEPROM = EEPROM.read(alamatEEPROM);
      if (nilaiEEPROM == 255) tglmerahTanggal[i] = 0;
      else tglmerahTanggal[i]= nilaiEEPROM;        
      alamatEEPROM += 1;  

      nilaiEEPROM = EEPROM.read(alamatEEPROM);
      if (nilaiEEPROM == 255) tglmerahBulan[i] = 0;
      else tglmerahBulan[i] = nilaiEEPROM;
      alamatEEPROM += 1;   
  }
  alamatEEPROM = 0;
}

void baca_jadwal_start()
{
  alamatEEPROM = addrJadwalStart;
  for (int i = 0; i < 9; i++)
  {
    for (int j = 0; j < 2; j++)
    {
//    startJadwal[i][j] = EEPROM.read(alamatEEPROM);
      int nilaiEEPROM = EEPROM.read(alamatEEPROM);

      if (nilaiEEPROM == 255) {
        startJadwal[i][j] = startJadwal[i][j];
      } else {
        startJadwal[i][j] = nilaiEEPROM;
      }
      
      Serial.print(alamatEEPROM);
      Serial.print("\t");
      Serial.print(nilaiEEPROM);
      Serial.print("\t");
      Serial.println(startJadwal[i][j]);
      alamatEEPROM += 1;
    }
  }
  alamatEEPROM = 0;
}

void baca_jadwal_end()
{
  alamatEEPROM = addrJadwalEnd;
  for (int i = 0; i < 9; i++)
  {
    for (int j = 0; j < 2; j++)
    {
//    startJadwal[i][j] = EEPROM.read(alamatEEPROM);
      int nilaiEEPROM = EEPROM.read(alamatEEPROM);

      if (nilaiEEPROM == 255) {
        endJadwal[i][j] = endJadwal[i][j];
      } else {
        endJadwal[i][j] = nilaiEEPROM;
      }
      
//      Serial.print(alamatEEPROM);
//      Serial.print("\t");
//      Serial.print(nilaiEEPROM);
//      Serial.print("\t");
//      Serial.println(endJadwal[i][j]);
      alamatEEPROM += 1;
    }
  }
  alamatEEPROM = 0;
}

void baca_sg_state()
{
  alamatEEPROM = addrSgState;
  for (int i = 0; i < sizeof(sgState); i++)
  {
    sgState[i] = EEPROM.read(alamatEEPROM);
    if (sgState[i] == 255)
      sgState[i] = on;

//    Serial.print(alamatEEPROM);
//    Serial.print("\t");
//    Serial.println(sgState[i]);
    alamatEEPROM += 1;
  }
  alamatEEPROM = 0;
}

void baca_jadwal()
{
  baca_waktu();

  if (jam == endJadwal[0][jm] && menit >= endJadwal[0][mn])
    modeTL = 'b';
  else if (jam > endJadwal[0][jm] && menit < endJadwal[0][mn])
    modeTL = 'b';

//  else if (jam == 00 && menit > startJadwal[2][mn])
    

  else if (jam == startJadwal[2][jm] && menit > startJadwal[2][mn])
    currSc = 2;
  else if (jam > startJadwal[2][jm] && jam < endJadwal[2][jm])
    currSc = 2;
  else if (jam == endJadwal[2][jm] && menit < endJadwal[2][mn])
    currSc = 2;

  else if (jam == startJadwal[1][jm] && menit > startJadwal[1][mn])
    currSc = 1;
  else if (jam > startJadwal[1][jm] && jam < endJadwal[1][jm])
    currSc = 1;
  else if (jam == endJadwal[1][jm] && menit < endJadwal[1][mn])
    currSc = 1;

  else if (jam == startJadwal[3][jm] && menit > startJadwal[3][mn])
    currSc = 3;
  else if (jam > startJadwal[3][jm] && jam < endJadwal[3][jm])
    currSc = 3;
  else if (jam == endJadwal[3][jm] && menit < endJadwal[3][mn])
    currSc = 3;

  else if (jam == startJadwal[4][jm] && menit > startJadwal[4][mn])
    currSc = 4;
  else if (jam > startJadwal[4][jm] && jam < endJadwal[4][jm])
    currSc = 4;
  else if (jam == endJadwal[4][jm] && menit < endJadwal[4][mn])
    currSc = 4;

  else if (jam == startJadwal[5][jm] && menit > startJadwal[5][mn])
    currSc = 5;
  else if (jam > startJadwal[5][jm] && jam < endJadwal[5][jm])
    currSc = 5;
  else if (jam == endJadwal[5][jm] && menit < endJadwal[5][mn])
    currSc = 5;

  else if (jam == startJadwal[6][jm] && menit > startJadwal[6][mn])
    currSc = 6;
  else if (jam > startJadwal[6][jm] && jam < endJadwal[6][jm])
    currSc = 6;
  else if (jam == endJadwal[6][jm] && menit < endJadwal[6][mn])
    currSc = 6;

  else if (jam == startJadwal[7][jm] && menit > startJadwal[7][mn])
    currSc = 7;
  else if (jam > startJadwal[7][jm] && jam < endJadwal[7][jm])
    currSc = 7;
  else if (jam == endJadwal[7][jm] && menit < endJadwal[7][mn])
    currSc = 7;

  else if (jam == startJadwal[8][jm] && menit > startJadwal[8][mn])
    currSc = 8;
  else if (jam > startJadwal[8][jm] && jam < endJadwal[8][jm])
    currSc = 8;
  else if (jam == endJadwal[8][jm] && menit < endJadwal[8][mn])
    currSc = 8;

  else if (jam == startJadwal[0][jm] && menit >= startJadwal[0][mn])
    modeTL = 'a';

  else
  {
    currSc = 0;
  }
}


byte cek_log()
{
  int logPage = 0;
  
  for ( int x = 0; x <= 15; x++ )
  {
    dataLog = EEPROM.read(addrLog + logPage);
    if ( dataLog >= 255 )
    { 
      dataLog = EEPROM.read(addrLog + logPage);
    } else {
      logPage = x;
      break;
    }
  }

  dataLog += 1;
  EEPROM.write(addrLog + logPage, dataLog);

  return logPage;
}

void cek_reset()
{
  if ( jam == endJadwal[0][jm] && menit == endJadwal[0][mn] && detik < 2 ) 
  {
    auto_reset();
  }
}

void cek_via_aplikasi()
{
  
}
