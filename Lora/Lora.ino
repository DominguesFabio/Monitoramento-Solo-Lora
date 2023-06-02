#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <SSD1306.h>
#include <Adafruit_AHT10.h>
#include <adafruit_sensor.h>
#include <Adafruit_MPU6050.h>

//Deixe esta linha descomentada para compilar o Master
//Comente ou remova para compilar o Slave
//#define MASTER

#define SCK 5   // GPIO5  SCK
#define MISO 19 // GPIO19 MISO
#define MOSI 27 // GPIO27 MOSI
#define SS 18   // GPIO18 CS
#define RST 14  // GPIO14 RESET
#define DI00 26 // GPIO26 IRQ(Interrupt Request)

#define BAND 915E6 //Frequência do radio - exemplo : 433E6, 868E6, 915E6

//Constante para informar ao Slave que queremos os dados
const String GETDATA = "getdata";
//Constante que o Slave retorna junto com os dados para o Master
const String SETDATA = "setdata=";

//Estrutura com os dados do sensor
typedef struct {
  double temperature;
  double humidity;
  double x;
  double y;
  double z;
}Data;

//Variável para controlar o display
SSD1306 display(0x3c, 4, 15);

void setupDisplay(){
  //O estado do GPIO16 é utilizado para controlar o display OLED
  pinMode(16, OUTPUT);
  //Reseta as configurações do display OLED
  digitalWrite(16, LOW);
  //Para o OLED permanecer ligado, o GPIO16 deve permanecer HIGH
  //Deve estar em HIGH antes de chamar o display.init() e fazer as demais configurações,
  //não inverta a ordem
  digitalWrite(16, HIGH);

  //Configurações do display
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

//Configurações iniciais do LoRa
void setupLoRa(){ 
  //Inicializa a comunicação
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DI00);

  //Inicializa o LoRa
  if (!LoRa.begin(BAND)){
    //Se não conseguiu inicializar, mostra uma mensagem no display
    display.clear();
    display.drawString(0, 0, "Erro ao inicializar o LoRa!");
    display.display();
    while (1);
  }

  //Ativa o crc
  LoRa.enableCrc();
  //Ativa o recebimento de pacotes
  LoRa.receive();
}

void send(){
  //Inicializa o pacote
  LoRa.beginPacket();
  //Envia o que está contido em "GETDATA"
  LoRa.print(GETDATA);
  //Finaliza e envia o pacote
  LoRa.endPacket();
}

//Compila apenas se MASTER estiver definido no arquivo principal
#ifdef MASTER

//Intervalo entre os envios
#define INTERVAL 1500

//Tempo do último envio
long lastSendTime = 0;

void setup(){
  Serial.begin(115200);
  //Chama a configuração inicial do display
  setupDisplay();
  //Chama a configuração inicial do LoRa
  setupLoRa();

  display.clear();
  display.drawString(0, 0, "Master");
  display.display();
}

void loop(){
  //Se passou o tempo definido em INTERVAL desde o último envio
  if (millis() - lastSendTime > INTERVAL){
    //Marcamos o tempo que ocorreu o último envio
    lastSendTime = millis();
    //Envia o pacote para informar ao Slave que queremos receber os dados
    send();
  }

  //Verificamos se há pacotes para recebermos
  receive();
}

void receive(){
  //Tentamos ler o pacote
  int packetSize = LoRa.parsePacket();
  
  //Verificamos se o pacote tem o tamanho mínimo de caracteres que esperamos
  if (packetSize > SETDATA.length()){
    String received = "";
    //Armazena os dados do pacote em uma string
    for(int i=0; i<SETDATA.length(); i++){
      received += (char) LoRa.read();
    }

    //Se o cabeçalho é o que esperamos
    if(received.equals(SETDATA)){
      //Fazemos a leitura dos dados
      Data data;
      LoRa.readBytes((uint8_t*)&data, sizeof(data));
      //Mostramos os dados no display
      showData(data);
    }
  }
}

void showData(Data data){
  //Tempo que demorou para o Master criar o pacote, enviar o pacote,
  //o Slave receber, fazer a leitura, criar um novo pacote, enviá-lo
  //e o Master receber e ler
  String waiting = String(millis() - lastSendTime);
  //Mostra no display os dados e o tempo que a operação demorou
  display.clear();
  display.drawString(0, 0, String(data.temperature) + " C");
  display.drawString(0, 8, String(data.humidity) + "%");
  display.drawString(0, 16, "X - " + String(data.x) );
  display.drawString(0, 24, "Y - " + String(data.y) );
  display.drawString(0, 36, "Z - " + String(data.z) );
  display.drawString(0, 48, waiting + " ms");
  display.display();
}

#endif

//Compila apenas se MASTER não estiver definido no arquivo principal
#ifndef MASTER


//Responsável pela leitura da temperatura e umidade
Adafruit_AHT10 aht;
Adafruit_MPU6050 mpu;

void setup(){
  Serial.begin(115200);
  //Chama a configuração inicial do display
  setupDisplay();
  //Chama a configuração inicial do LoRa
  setupLoRa();

  if (!mpu.begin()) {
    Serial.println("Sensor init failed");
    while (1)
      yield();
  }
  
  if (! aht.begin())
  {
    display.clear();
    display.drawString(0, 0, "Sensor não encontrado");
    display.display();
    while(1);
  }

  display.clear();
  display.drawString(0, 0, "Slave esperando...");
  display.display();
}

void loop(){
  //Tenta ler o pacote
  int packetSize = LoRa.parsePacket();

  //Verifica se o pacote possui a quantidade de caracteres que esperamos
  if (packetSize == GETDATA.length())
  {
    String received = "";

    //Armazena os dados do pacote em uma string
    while(LoRa.available())
    {
      received += (char) LoRa.read();
    }

    if(received.equals(GETDATA))
    {
      //Faz a leitura dos dados
      Data data = readData();
      Serial.println("Criando pacote para envio");
      //Cria o pacote para envio
      LoRa.beginPacket();
      LoRa.print(SETDATA);
      LoRa.write((uint8_t*)&data, sizeof(data));
      //Finaliza e envia o pacote
      LoRa.endPacket();
      showSentData(data);
    }
  }
}

//Função onde se faz a leitura dos dados
Data readData()
{
  Data data;
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  
  data.temperature = temp.temperature;
  data.humidity = humidity.relative_humidity;

  sensors_event_t a, g;
  mpu.getEvent(&a, &g, &temp);

  data.x = a.acceleration.x;
  data.z = a.acceleration.z;
  data.y = a.acceleration.y;
  return data;
}

void showSentData(Data data)
{
  //Mostra no display
  display.clear();
  display.drawString(0, 0, "Enviou:");
  display.drawString(0, 10,  String(data.temperature) + " C");
  display.drawString(0, 20, String(data.humidity) + "%");
  display.drawString(0, 30, "X - " + String(data.x) );
  display.drawString(0, 40, "Y - " + String(data.y) );
  display.drawString(0, 50, "Z - " + String(data.z) );
  display.display();
}

#endif
