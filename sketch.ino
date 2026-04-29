#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- PIN ---
#define PIN_TRIG    7
#define PIN_ECHO    6
#define PIN_LED     9    // PWM
#define PIN_BUZZER  10   // PWM
#define PIN_BUTTON  2    // Interrupt

// --- LCD 16x2 alamat I2C 0x27 ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// volatile karena diubah oleh Interrupt
volatile bool isMuted = false;

// Untuk beep non-blocking tanpa delay()
unsigned long previousMillis = 0;
bool buzzerState = false;

// ======================
void setup() {
  Serial.begin(9600);

  pinMode(PIN_TRIG,   OUTPUT);
  pinMode(PIN_ECHO,   INPUT);
  pinMode(PIN_LED,    OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Parking Sensor");
  lcd.setCursor(0, 1); lcd.print("Siap...");
  delay(2000);
  lcd.clear();

  // Interrupt: panggil tombolDitekan() saat tombol ditekan (FALLING = HIGH ke LOW)
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), tombolDitekan, FALLING);
}

// Dipanggil otomatis saat tombol ditekan, harus singkat
void tombolDitekan() {
  isMuted = !isMuted; // toggle mute
}

// Kirim pulsa TRIG, ukur durasi ECHO, konversi ke cm
float bacaJarak() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long durasi = pulseIn(PIN_ECHO, HIGH, 30000); // timeout 30ms
  if (durasi == 0) return -1; // tidak ada objek

  return durasi * 0.0343 / 2.0; // rumus jarak (cm)
}

// Tampilkan jarak dan zona ke LCD
void tampilLCD(float jarak, String zona) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (jarak < 0) lcd.print("Tidak ada objek");
  else { lcd.print("Jarak: "); lcd.print(jarak, 1); lcd.print(" cm"); }

  lcd.setCursor(0, 1);
  lcd.print(isMuted ? "[MUTE] " + zona : "Zona: " + zona);
}

// Atur LED dan buzzer sesuai jarak
void kontrolOutput(float jarak) {
  unsigned long sekarang = millis();

  int ledVal = 0, freq = 0;
  unsigned long interval = 0;
  String zona = "";

  // Tentukan zona
  if      (jarak < 0)   { zona = "Bebas";    }
  else if (jarak <= 5)  { zona = "< 5cm";    ledVal = 255; } // sensor tidak akurat
  else if (jarak <= 15) { zona = "KRITIS";   ledVal = 255; freq = 2500; interval = 80;   }
  else if (jarak <= 30) { zona = "Dekat";    ledVal = 180; freq = 2000; interval = 200;  }
  else if (jarak <= 50) { zona = "Hati2";    ledVal = 100; freq = 1500; interval = 500;  }
  else                  { zona = "Aman";     ledVal = 30;  freq = 1000; interval = 1000; }

  analogWrite(PIN_LED, ledVal); // kecerahan LED sesuai zona

  // Buzzer: mati jika mute atau zona bebas
  if (isMuted || interval == 0) {
    noTone(PIN_BUZZER);
    buzzerState = false;
  } else {
    // Beep on/off tanpa delay (non-blocking)
    if (sekarang - previousMillis >= interval) {
      previousMillis = sekarang;
      buzzerState = !buzzerState;
      buzzerState ? tone(PIN_BUZZER, freq) : noTone(PIN_BUZZER);
    }
  }

  tampilLCD(jarak, zona);
  Serial.print("Jarak: "); Serial.print(jarak, 1);
  Serial.print(" cm | "); Serial.print(zona);
  if (isMuted) Serial.print(" [MUTE]");
  Serial.println();
}

// ======================
void loop() {
  float jarak = bacaJarak();  // baca sensor
  kontrolOutput(jarak);       // proses output
  delay(100);                 // jeda antar baca
}
