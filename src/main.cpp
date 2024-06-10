#include <Arduino.h>
#include <U8g2lib.h>

#define OLED_SDA 23 //jaune
#define OLED_SCL 19 //orange

// Tableau pour stocker les données mesurées
double dataArray_tension[10]; // Vous pouvez ajuster la taille selon vos besoins
int arrayLength_tension = 0;

double dataArray_courant[10]; // Vous pouvez ajuster la taille selon vos besoins
int arrayLength_courant = 0;



unsigned long previousSendTime = 0;
unsigned long previousAverageTime = 0;

const unsigned long sendInterval = 200; // Interval d'envoi des données en ms
const unsigned long averageInterval = 100; // Interval pour le calcul de la moyenne glissante en ms


const uint32_t SERIAL_SPEED = 115200;
// const uint32_t SERIAL_SPEED = 9600;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(/* R2: rotation 180°*/ U8G2_R2,/* reset=*/ U8X8_PIN_NONE,  /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA);

double Vmes=0,Vval=0, Imes=0, Ival=0;
int ds=0, Secondes=0;

char InData[]  = {"$VXX.XXX IXX.XXX SXXXX*"};
char InDataLen = 23;


volatile struct {
    unsigned Display: 1;
    unsigned Update: 1;
    unsigned unused: 30;
} Flags;


// Pour configurer une interruption Timer
hw_timer_t *timer = NULL;
const double Tsamp_us = 10000;  // 10 ms = 10000 us

// Compteur logiciel
const int DispNcount = 50;
volatile int Dispcount = 0; // 50 *10ms = 0.5s

const int entreeAnalogique2 = 33;  // Courant (cable jaune)
const int entreeAnalogique1 = 32;  // Tension (cable orange)

const float R=0.0066;
const float gain_bat = 1;
const float gain_courant = 1;


void IRAM_ATTR onTimer() { // ici on est toutes les Tsamp = 10 ms
  Flags.Update = 1;
  ds++;
  Secondes=ds/100;
  if (++Dispcount >= DispNcount) {
    Dispcount = 0;
    Flags.Display = 1;
  }
}

// Fonction pour calculer la moyenne glissante des données mesurées
double calculateMovingAverage(double* dataArray, int arrayLength) {
    double sum = 0;
    for (int i = 0; i < arrayLength; i++) {
        sum += dataArray[i];
    }
    return sum / arrayLength;
}


void mesure(){
  
  Vmes = (12.0/15625.0)*analogRead(entreeAnalogique1) + 0.1734;
  Vval = Vmes*gain_bat;
  Imes = (12.0/15625.0)*analogRead(entreeAnalogique2) + 0.1734;
  Ival = (Imes/R)*gain_courant;

  // Serial.println("Vmes = "+ String(Vmes)+" V");
  // Serial.println("Imes = "+ String(Imes)+" V");
  // Serial.println("Ival = "+ String(Ival)+" A");
  // Serial.println("Timer = "+ String(Secondes));
  
  if (Vmes>99.99) Vmes=99.99;
  if (Vmes<-9.99) Vmes=-9.99;

  if (Ival>99.99) Ival=99.99;
  if (Ival<-9.99) Ival=-9.99;
  if (Secondes>9999) Secondes=9999;

  dataArray_tension[arrayLength_tension++] = Vmes; // Stocker la tension
    dataArray_courant[arrayLength_courant++] = Ival; // Stocker le courant

    // Assurez-vous de vérifier les limites du tableau pour éviter les débordements
    if (arrayLength_tension >= 10) {
        arrayLength_tension = 0; // Réinitialiser l'index lorsque le tableau est plein, ou ajustez selon votre besoin
    }
    if (arrayLength_courant >= 10) {
        arrayLength_courant = 0; // Réinitialiser l'index lorsque le tableau est plein, ou ajustez selon votre besoin
    }

  // sprintf(InData, "$%6.3f %6.3f %4d*", Vmes, Ival, Secondes);
  // Serial.println(InData);

  }

  void refreshDisplay() {

  if (Vmes>99.99) Vmes=99.99;
  if (Vmes<-9.99) Vmes=-9.99;

  if (Ival>99.99) Ival=99.99;
  if (Ival<-9.99) Ival=-9.99;
  if (Secondes>9999) Secondes=9999;



  // Affichage sur OLED
  u8g2.clearBuffer();
  
  u8g2.setCursor(0, 16);
  u8g2.print("Vbat =");
  u8g2.print(Vmes, 2);
  u8g2.print(" V");

  u8g2.setCursor(0, 32);
  u8g2.print("Imes =");
  u8g2.print(Imes, 2);
  u8g2.print(" V");

  u8g2.setCursor(0, 48);
  u8g2.print("Ival =");
  u8g2.print(Ival, 2);
  u8g2.print(" A");
  u8g2.sendBuffer();
}


void setup() {
  
  Serial.begin(SERIAL_SPEED);
  
  analogReadResolution(12);
  u8g2.begin();
  u8g2.setFont(u8g2_font_helvB12_tr);
  u8g2.setFontMode(0);
  u8g2.clearBuffer();

  u8g2.setCursor(0, 16);
  u8g2.print("Vbat =");
  u8g2.print(Vmes, 2);
  u8g2.print(" V");

  u8g2.setCursor(0, 32);
  u8g2.print("Imes =");
  u8g2.print(Imes, 2);
  u8g2.print(" V");

  u8g2.setCursor(0, 48);
  u8g2.print("Ival =");
  u8g2.print(Ival, 2);
  u8g2.print(" A");
  u8g2.sendBuffer();

  Dispcount = 0;
  Flags.Update = 0;
  Flags.Display = 0;

  // config timer0 et installation de l'interruption
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, Tsamp_us, true);
  timerAlarmEnable(timer);
}

void loop() {
    unsigned long currentMillis = millis();


    // Gérer l'envoi des données toutes les 200 ms
    if (currentMillis - previousSendTime >= sendInterval) {
        // Sauvegarder l'heure actuelle pour le prochain envoi
        previousSendTime = currentMillis;

        // Appeler la fonction de mesure
        mesure();

    }

    // Gérer le calcul de la moyenne glissante toutes les 100 ms
    if (currentMillis - previousAverageTime >= averageInterval) {
        // Sauvegarder l'heure actuelle pour le prochain calcul
        previousAverageTime = currentMillis;

        // Calculer la moyenne glissante des données mesurées
        double movingAverage_tension = calculateMovingAverage(dataArray_tension, arrayLength_tension);
        double movingAverage_courant = calculateMovingAverage(dataArray_courant, arrayLength_courant);

        // Envoyer la moyenne glissante par exemple via Serial
        sprintf(InData, "$%6.3f %6.3f %4d*", movingAverage_tension, movingAverage_courant, Secondes);
        refreshDisplay();
        Serial.println(InData);
    }

}
