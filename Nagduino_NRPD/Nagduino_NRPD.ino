/*
 
 Cliente NRDP para enviar para o Nagios as informações abaixo:
 * Leitura dos sensores de temperatura, umidade e barometro;
 * Sensor de porta para verificar se a porta está aberta ou fechada.
 * Sensor de fumaça.
 O programa envia os dados coletados dos sensores e também envia 
 o status do serviço para o Nagios que pode ser: 
 0 = RECOVERY/OK;
 1 = WARNING;
 2 = CRITICAL;
 3 = UNKNOW.
 
 Limites Umidade:
 30% à 85% = OK
 umi < 30% | umi > 85% = CRITICAL
 
 Limites Temperatura:
 15 à 30 = OK
 temp < 15 = WARNING Temperatura abaixo do normal, verificar. 
 temp > 30 = CRITICAL Temperatura alta verificar.
 
 Criado: 26 Novembro 2015
 Última modificação: 02 Fevereiro 2016
 Autor: Jardel F. F. de Araujo
 
 */

#include <SPI.h>
#include <Ethernet.h>
#include <Nagduino.h>
#include <DHT.h>
#include <SHT1x.h>
#include <Adafruit_BMP085.h>
#include<Wire.h>

// Definições iniciais do programa
#define DHTPIN 8                        // Pino digital do sensor de Temperatura/Umidade DHT11/DHT22
#define DHTTYPE 11                      // Tipo de Sensor de Temperatura/Umidade (DHT22/DHT11)
#define SENSORCH A0                     // Pino analogico do senso de chuva
#define PORTAPIN 5                      // Pino do sensor de porta
#define SENSORFUMACA A1                 // Pino do sensor de fumaça
#define SHTDATA 7                       // Pino do DATA do sensor de temperatura/umidade SHT
#define SHTCLOCK  6                     // Pino do CLOCK do sensor de temperatura/umidade SHT
#define HOST "TIO_ARDUINO_SITE_B"       // HOST do Nagios que vai receber as notificações
#define SRV_PORTA "Porta"               // SERVICE do Nagios para notificação do sensor da porta
#define SRV_ALTITUDE "Altitude"         // SERVICE do Nagios para notificação da altitude
#define SRV_PRESSAO "pressao"           // SERVICE do Nagios para notificação da pressão atmosférica
#define SRV_ALAGAMENTO "Sensor Agua"    // SERVICE do Nagios para notificação do sensor de alagamento
#define SRV_FUMACA "Sensor Fumaca"      // SERVICE do Nagios para notificação do sensor de fumaça
#define SRV_TEMPERATURA "Temperatura"   // SERVICE do Nagios para notificação do sensor de temperatura
#define SRV_UMIDADE "Umidade"           // SERVICE do Nagios para notificação do sensor de umidade

//Definação de IP interface de rede e servidor Nagios
byte mac[6]    = { 0x4E, 0x61, 0x67, 0x69, 0x6F, 0x73 };	// MAC address do Arduino
byte ip[4]     = { 192,168,66,223 };	// IP Arduino
byte nagios[4] = { 192,168,66,33};	// IP Servidor Nagios
char token[]   = "7wntx5qu4x1s";	// Token NRDP

//Definição das variáveis/objetos globais do programa
Nagduino nagduino;                                                // Objeto Nagduino para envio de notificações via NRDP
Adafruit_BMP085 bmp;                                              // Objeto do sensor do barômetro                                              
//DHT dht(DHTPIN, DHTTYPE);                                       // Objeto do sensor de temperatura/umidade DHT
SHT1x sht1x(SHTDATA, SHTCLOCK);                                   // Objeto do sensor de temperatura/umidade SHT
const String vStatus[4] {"OK", "WARNING", "CRITICAL", "UNKNOW"};
const byte nStatus[4] {0, 1, 2, 3};
String temperatura, umidade, pressao, vAltitude, vAgua, vFumaca;
char dados[80];
float valor;
byte ind, tempo;
int Porta = 1;     // Estado da porta (1 = fechada, 0 = aberta)
int VSensor;
int checkPorta=0; // Status da checagem da porta (0 = esta fecha, 1 = esta aberta);

//Rotina inicial do programa
void setup()
{
  Serial.begin(9600);
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  
  if (!bmp.begin()) {
	Serial.println("Não foi possível localizar o sensor BMP085 (barometro), verifique as conexões!");
        nagduino.sendNRDP(HOST, SRV_PRESSAO, 3, "UNKNOW: Falha leitura sensor barometro!");
	//while (1) {}
  }
  pinMode(PORTAPIN, INPUT_PULLUP); // Attach magnetic sensor (or button, switch, etc) to pin 5
  nagduino.sendNRDP(HOST, SRV_PORTA, 0, "Porta OK: Porta fechada!");  // Faz a checagem inicial com a porta fechada.
  tempo=0;
}

//Loop principal do programa
void loop()
{
  sensor_porta();
  if (tempo == 20) {
    temperaruta_sht();
    umidade_sht();
    sensor_fumaca();
    sensor_barometro();
    altitude();
    sensor_alagamento();
    tempo=0; 
  }
  delay(3000);
  tempo++;
}

// Faz a leitura do sensor da porta e envia notificacao com os dados para o Nagios
void sensor_porta() {
  Porta = digitalRead(PORTAPIN); // Lê valor do sensor da porta
  if (Porta != 1) {
    if ((checkPorta!=1) || (tempo == 20)) {
      nagduino.sendNRDP(HOST, SRV_PORTA, 2, "Porta CRITICAL: Porta Aberta!");
      Serial.println("Porta aberta.");
      checkPorta=1;
    }
  } 
  else {
    if ((checkPorta!=0) || (tempo == 20)) {
      nagduino.sendNRDP(HOST, SRV_PORTA, 0, "Porta OK: Porta fechada!");
      Serial.println("Porta fechada.");
      checkPorta=0;
    }
  }
}

// Faz a leitura da pressao atmosférica do sensor barometro e envia notificacao com os dados para o Nagios
void sensor_barometro() {
  valor=bmp.readPressure()/100;
  if ((valor < 1000) && ((valor >= 999))) {
    ind=1;
  }
  else if (valor < 999) {
    ind=2;
  }
  else {
    ind=0;
  }
  pressao="Pressão atmosférica " + vStatus[ind] + ": " + String(valor);
  pressao=pressao + " hPa|'Pressao atmosferia'=" + String(valor) + "hPa;999;1000;950;1020";
  Serial.println(pressao);
  pressao.toCharArray(dados, 80);
  nagduino.sendNRDP(HOST, SRV_PRESSAO, nStatus[ind], dados);
}

// Faz a leitura da altitude do sensor barometro e envia notificacao com os dados para o Nagios
// Obs.: Valor aproximado devido variacao da pressao atmosferica.
void altitude() {
  vAltitude="Altidude: ≅ " + String(bmp.readAltitude());
  vAltitude=vAltitude + " m";
  Serial.println(vAltitude);
  vAltitude.toCharArray(dados, 80);
  nagduino.sendNRDP(HOST, SRV_ALTITUDE, 0, dados);
}

// Faz a leitura do sensor de chuva/alagamento e envia notificacao com os dados para o Nagios
void sensor_alagamento() {
  VSensor = analogRead(SENSORCH);
  if (VSensor >= 1000) {
    vAgua="Sensor alagamento AC OK: Está seco (" + String(VSensor) + ")";
    Serial.println(vAgua);
    vAgua.toCharArray(dados, 80);
    nagduino.sendNRDP(HOST, SRV_ALAGAMENTO, 0, dados);
  }
  else {
    vAgua="Sensor alagamento AC CRITICAL: Detectado água! (" + String(VSensor) + ")";
    Serial.println(vAgua);
    vAgua.toCharArray(dados, 80);
    nagduino.sendNRDP(HOST, SRV_ALAGAMENTO, 2, dados);
  }
}

// Faz a leitura do sensor de fumaça e envia notificacao com os dados para o Nagios
void sensor_fumaca() {
  vFumaca="Sensor fumaça OK!";
  Serial.println(vFumaca);
  vFumaca.toCharArray(dados, 80);
  nagduino.sendNRDP(HOST, SRV_FUMACA, 0, dados);
}

// Funcao para fazer a leitura do sensor de umidade (Sensor SHT10) e envia notificacao com os dados para o Nagios
void umidade_sht() {
  // Se nao conseguir fazer a leitura do sensor retorna UNKNOW
  if (sht1x.readHumidity() < 0.0) {
  nagduino.sendNRDP(HOST, SRV_UMIDADE, 3, "Umidade UNKNOW: Falha leitura sensor umidade!");
  }
  else {
    if ((sht1x.readHumidity() >= 30) && (sht1x.readHumidity() <= 85)) {
     ind=0;
    }
    else {
     ind=2;
    } 
    umidade="Umidade " + vStatus[ind] + ": " + String((float)sht1x.readHumidity(),2);
    umidade=umidade + "%|Umidade=" + String((float)sht1x.readHumidity(),2) + "%;85;85;15;85";
    Serial.println(umidade);
    umidade.toCharArray(dados, 80);
    nagduino.sendNRDP(HOST, SRV_UMIDADE, nStatus[ind], dados);
  }
}

//
// Funcao para fazer a leitura do sensor de umidade (Sensor DHT11) e envia notificacao com os dados para o Nagios
//
/*
void umidade_dht() {
  // Se nao conseguir fazer a leitura do sensor retorna UNKNOW
  if (isnan(dht.readHumidity())) {
  nagduino.sendNRDP(HOST, SRV_UMIDADE, 3, "Umidade UNKNOW: Falha leitura sensor umidade!");
  }
  else {
    if ((dht.readHumidity() >= 30) && (dht.readHumidity() <= 85)) {
     ind=0;
    }
    else {
     ind=1;
    } 
    umidade="Umidade " + vStatus[ind] + ": " + String((float)dht.readHumidity(), 2);
    umidade=umidade + "%";
    Serial.println(umidade);
    umidade.toCharArray(dados, 80);
    nagduino.sendNRDP(HOST, SRV_UMIDADE, nStatus[ind], dados);
  }
}
*/

//
// Funcao para fazer a leitura do sensor de umidade (Sensor SHT10) e envia notificacao com os dados para o Nagios
//
void temperaruta_sht() {
  // Se nao conseguir fazer a leitura do sensor retorna UNKNOW
  if (sht1x.readTemperatureC() < -40.0) {
    nagduino.sendNRDP(HOST, SRV_TEMPERATURA, 3, "Temperatura UNKNOW: Falha leitura sensor temperatura!");
  }
  else {
    // Se a temperatura for maior de 15 e menor que 31 retorna OK
    if ((sht1x.readTemperatureC() > 15.0) && (sht1x.readTemperatureC() < 31.0)) {
      ind=0;
    }
    // Se a temperatura for menor que 15 retorna WARNING
    else if (sht1x.readTemperatureC() < 15.0) {
      ind=1;
    }
    // Caso contrario as condicoes anteriores retorna CRITICAL
    else {
     ind=2; 
    }
    temperatura="Temperatura " + vStatus[ind] + ": " + String((float)sht1x.readTemperatureC());
    temperatura=temperatura + " °C|Temperatura=" + String((float)sht1x.readTemperatureC()) + "Celsius;31;31;15;35";
    Serial.println(temperatura);
    temperatura.toCharArray(dados, 80);
    nagduino.sendNRDP(HOST, SRV_TEMPERATURA, nStatus[ind], dados);
  }
}

//
// Funcao para fazer a leitura do sensor de temperatura (Sensor DHT11) e envia notificacao com os dados para o Nagios
//
/*
void temperatura_dht() {
  if (isnan(dht.readTemperature())) {
    nagduino.sendNRDP(HOST, SRV_TEMPERATURA, 3, "Temperatura UNKNOW: Falha leitura sensor temperatura!");
  }
  else {
    // Se a temperatura for maior de 15 e menor que 31 retorna OK
    if (((float)bmp.readTemperature() > 15.0) && ((float)bmp.readTemperature() < 31.0)) {
      ind=0;
    }
    // Se a temperatura for menor que 15 retorna WARNING
    else if ((float)bmp.readTemperature() < 15.0) {
      ind=1;
    }
    // Caso contrario as condicoes anteriores retorna CRITICAL
    else {
     ind=2; 
    }
    temperatura="Temperatura " + vStatus[ind] + ": " + String(bmp.readTemperature());
    temperatura=temperatura + " *C|Temperatura=" + String(bmp.readTemperature()) + "C;31;31;;";
    Serial.println(temperatura);
    temperatura.toCharArray(dados, 80);
    nagduino.sendNRDP(HOST, SRV_TEMPERATURA, nStatus[ind], dados);
  }
}
*/
