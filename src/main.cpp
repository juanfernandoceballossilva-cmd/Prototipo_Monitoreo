#include <Arduino.h>
#include <esp32_smartdisplay.h>
#include <ui/ui.h>
#include <ui/screens.h>
#include "esp_now.h"
#include "WiFi.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "SD.h"
#include "FS.h"
#include "SPI.h"
#include <vector>

// ==============================
// CONFIGURACIÓN DE HARDWARE
// ==============================
#define RXD2 17  // Pin RX para comunicación Serial2 con módulo LoRa
#define TXD2 18  // Pin TX para comunicación Serial2 con módulo LoRa  
#define SerialPort Serial2  // Objeto Serial para comunicación con LoRa RYLR998

// ==============================
// CONFIGURACIÓN SD CARD
// ==============================
#define ENABLE_SD_CARD true

// ==============================
// SISTEMA DE SD CARD
// ==============================
bool sdCardInicializada = false;
unsigned long ultimoGuardadoSD = 0;
const unsigned long INTERVALO_GUARDADO_SD = 10000; // 10 segundos

// ==============================
// VARIABLES PARA CONTROL DE BACKLIGHT
// ==============================
bool backlight_estable = true;
unsigned long ultimo_cambio_backlight = 0;
const unsigned long INTERVALO_BACKLIGHT = 1000;

// ==============================
// VARIABLES GLOBALES COMPARTIDAS
// ==============================
lv_chart_series_t *serie_test;
SemaphoreHandle_t xLVGLMutex;

extern "C" {
    void event_handler_cb_screen_b_btn_ph(lv_event_t *e);
    void event_handler_cb_screen_b_btn_temperatura(lv_event_t *e);
    void event_handler_cb_screen_b_btn_humedad(lv_event_t *e);
    void event_handler_cb_screen_b_btn_conductividad(lv_event_t *e);
    void event_handler_cb_screen_b_btn_nitrogeno(lv_event_t *e);
    void event_handler_cb_screen_b_btn_fosforo(lv_event_t *e);
    void event_handler_cb_screen_b_btn_potasio(lv_event_t *e);
}

volatile bool actualizar_grafica = false;
volatile int nuevos_valores[40];
volatile int contador_tarea1 = 0;
volatile int contador_tarea2 = 0;

volatile unsigned long last_sensor_time = 0;
volatile unsigned long last_valvula_time = 0;

// ==============================
// VARIABLES PARA DATOS DE SENSORES
// ==============================
volatile float Humedad = 0.0, Temperatura = 0.0, pH = 0.0, Conductividad = 0.0;
volatile uint16_t Nitrogeno = 0, Fosforo = 0, Potasio = 0, Salinidad = 0, TDS = 0;
String Grafica = "";
// ==============================
// VARIABLES PARA CONTROL DE VÁLVULA
// ==============================
volatile bool valve_switch_state = false;
volatile bool valve_switch_changed = false;
volatile int consultamodo_operacion = 0;
volatile int interrpvalvula = 0;
volatile int estadovalvula = 0;
String fecha = "2025-00-00 00:00:00";
volatile String horaweb="";


// Convertir fecha completa a timestamp (segundos desde 1970 aproximado)
unsigned long tiempoASegundos(String hora) {
    // Formato: "2025-00-00 00:00:00"
    int horas = hora.substring(11, 13).toInt();
    int minutos = hora.substring(14, 16).toInt();
    int segundos = hora.substring(17, 19).toInt();
    
    // Cálculo simple de timestamp (aproximado)
    return horas * 3600 + minutos * 60 + segundos;
}

// ==============================
// MUTEX PARA PROTECCIÓN
// ==============================
SemaphoreHandle_t xControlMutex;
SemaphoreHandle_t xSensorMutex;

// ==============================
// VARIABLES PARA COMUNICACIÓN LoRa
// ==============================
String bufferComandos = "";
bool comandoCompleto = false;
bool datosRecibidos = false;
bool puedeEnviarLora = true;
bool debeEsperarRespuesta = false;
unsigned long tiempoEnvioLora = 0;
const unsigned long TIMEOUT_LORA = 10000;

// ==============================
// ENVÍO CON INTERVALOS DIFERENCIADOS
// ==============================
unsigned long ultimo_envio_completo = 0;
unsigned long ultimo_envio_valvula = 0;
const unsigned long INTERVALO_COMPLETO = 1000;
const unsigned long INTERVALO_VALVULA = 1500;

// ==============================
// VARIABLES PARA MOSTRAR DATOS LoRa
// ==============================
typedef struct {
    char ultimo_envio[128];
    char ultima_respuesta[256];
    int contador_envios;
    int contador_recepciones;
    unsigned long ultimo_envio_time;
    unsigned long ultima_recepcion_time;
} lora_data_t;

volatile lora_data_t lora_data = {"", "", 0, 0, 0, 0};

// ==============================
// ESTRUCTURAS PARA COMUNICACIÓN ESP-NOW
// ==============================
typedef struct struct_message {
  int id;
  float Humedad;
  float Temperatura;
  float pH;
  float Conductividad;
  uint16_t Nitrogeno;
  uint16_t Fosforo;
  uint16_t Potasio;
  uint16_t Salinidad;
  uint16_t TDS;
} struct_message;

typedef struct struct_command {
  int estadovalvula;
} struct_command;

typedef struct struct_response {
  int consultamodo_operacion;
  int interrpvalvula;
} struct_response;

struct_message myData;
struct_command myCommand;
struct_response myResponse;
struct_message boardsStruct[3];

uint8_t receiverMAC[] ={0xD0, 0xEF, 0x76, 0x5C, 0x7D, 0xC0};//= {0x14, 0x08, 0x08, 0xA4, 0x8D, 0x14};//={0xD0, 0xEF, 0x76, 0x59, 0x1A, 0x84};

bool peerRegistered = false;


// ==============================
// SISTEMA DE SD CARD
// ==============================
bool inicializarSD() {
    #if !ENABLE_SD_CARD
    return false;
    #endif
    
    if(!SD.begin()) {
        Serial.println("❌ Falló la inicialización de la SD card");
        sdCardInicializada = false;
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE) {
        Serial.println("❌ No se encontró SD card");
        sdCardInicializada = false;
        return false;
    }
    
    Serial.println("✅ SD Card inicializada - Tipo: " + String(cardType == CARD_MMC ? "MMC" : 
                  cardType == CARD_SD ? "SDSC" : 
                  cardType == CARD_SDHC ? "SDHC" : "Desconocido"));
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    uint64_t usedSpace = SD.usedBytes() / (1024 * 1024);
    uint64_t freeSpace = cardSize - usedSpace;
    
    Serial.println("💾 Tamaño: " + String(cardSize) + "MB, Usado: " + String(usedSpace) + "MB, Libre: " + String(freeSpace) + "MB");
    
    // Crear archivo de datos con headers si no existe
    File file = SD.open("/datos_sensores.csv", FILE_READ);
    if(!file) {
        file = SD.open("/datos_sensores.csv", FILE_WRITE);
        if(file) {
            file.println("Timestamp,Fecha,Hora_Segundos,Temperatura,Humedad,pH,Conductividad,Nitrogeno,Fosforo,Potasio,Salinidad,TDS,EstadoValvula");
            file.close();
            Serial.println("📁 Archivo de datos creado en SD");
        }
    } else {
        file.close();
    }
        
    sdCardInicializada = true;

    return true;
}

void guardarDatosEnSD() {
    #if !ENABLE_SD_CARD
    return;
    #endif
    
    if(!sdCardInicializada) return;
    
    File file = SD.open("/datos_sensores.csv", FILE_APPEND);
    if(!file) {
        Serial.println("❌ Error abriendo archivo de datos en SD");
        return;
    }
    
    // Obtener timestamp actual
    unsigned long timestamp = millis();
    unsigned long hora_segundos = tiempoASegundos(fecha);
    
    // Obtener datos actuales
    float temp = 0.0f, hum = 0.0f, ph_val = 0.0f, cond = 0.0f;
    uint16_t n = 0, p = 0, k = 0, sal = 0, tds_val = 0;
    int estado_valvula = 0;
    
    if (xSemaphoreTake(xSensorMutex, portMAX_DELAY) == pdTRUE) {
        temp = Temperatura;
        hum = Humedad;
        ph_val = pH;
        cond = Conductividad;
        n = Nitrogeno;
        p = Fosforo;
        k = Potasio;
        sal = Salinidad;
        tds_val = TDS;
        xSemaphoreGive(xSensorMutex);
    }
    
    if (xSemaphoreTake(xControlMutex, portMAX_DELAY) == pdTRUE) {
        estado_valvula = estadovalvula;
        xSemaphoreGive(xControlMutex);
    }
    
    // Crear línea de datos
    String dataLine = String(timestamp) + "," +
                     fecha + "," +
                     String(hora_segundos) + "," +
                     String(temp, 2) + "," +
                     String(hum, 2) + "," +
                     String(ph_val, 2) + "," +
                     String(cond, 2) + "," +
                     String(n) + "," +
                     String(p) + "," +
                     String(k) + "," +
                     String(sal) + "," +
                     String(tds_val) + "," +
                     String(estado_valvula);
    
    file.println(dataLine);
    file.close();
    
    Serial.println("💾 Datos guardados en SD - Timestamp: " + String(timestamp));
}

void verificarEspacioSD() {
    #if !ENABLE_SD_CARD
    return;
    #endif
    
    if(!sdCardInicializada) return;
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    uint64_t usedSpace = SD.usedBytes() / (1024 * 1024);
    uint64_t freeSpace = cardSize - usedSpace;
    
    if(freeSpace < 10) { // Menos de 10MB libres
        Serial.println("⚠️  Poco espacio en SD: " + String(freeSpace) + "MB libres");
    }
}

// ==============================
// FUNCIÓN DE DEBUG MEJORADA
// ==============================
void printDebug(String message) {
    Serial.println(message);
}

// ==============================
// DECLARACIÓN ANTICIPADA DE FUNCIONES
// ==============================
void enviarRespuesta();
void leerComandosLoRa();
void procesarBufferCompleto();
void enviarDatosLoRa();
void sendCmd(String cmd);
void actualizarUI();
void procesarMensajeLoRaRecibido(String mensaje);
void setup_valve_switch();

// ==============================
// FUNCIONES PARA COMUNICACIÓN ESP-NOW
// ==============================
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if(len == sizeof(struct_message)){
        memcpy(&myData, incomingData, sizeof(myData));

        if(myData.id >=1 && myData.id <=3){
            boardsStruct[myData.id-1] = myData;

            if (myData.id == 1) {
                if (xSemaphoreTake(xSensorMutex, portMAX_DELAY) == pdTRUE) {
                    Humedad = myData.Humedad;
                    Temperatura = myData.Temperatura;
                    pH = myData.pH;
                    Conductividad = myData.Conductividad;
                    Nitrogeno = myData.Nitrogeno;
                    Fosforo = myData.Fosforo;
                    Potasio = myData.Potasio;
                    Salinidad = myData.Salinidad;
                    TDS = myData.TDS;
                    xSemaphoreGive(xSensorMutex);
                }
                datosRecibidos = true;
                
            }
        }
        last_sensor_time = millis();
    }
    else if(len == sizeof(struct_command)){
        memcpy(&myCommand, incomingData, sizeof(myCommand));
        
        if (xSemaphoreTake(xControlMutex, portMAX_DELAY) == pdTRUE) {
            estadovalvula = myCommand.estadovalvula;
            xSemaphoreGive(xControlMutex);
        }
        Serial.printf("🎯 Comando válvula: %d\n", myCommand.estadovalvula);
        
        // Guardar evento de cambio de válvula
        //guardarLogEvento("Cambio_valvula_ESP_NOW_" + String(myCommand.estadovalvula));
        
        last_valvula_time = millis();
    }
}

void enviarRespuesta() {
    if (!peerRegistered) {
        esp_now_peer_info_t peerInfo;
        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, receiverMAC, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("❌ Error registrando peer ESP-NOW");
            return;
        }
        peerRegistered = true;
    }
    
    int modoOperacion = 0;
    int interrValvula = 0;
    if (xSemaphoreTake(xControlMutex, portMAX_DELAY) == pdTRUE) {
        modoOperacion = consultamodo_operacion;
        interrValvula = interrpvalvula;
        xSemaphoreGive(xControlMutex);
    }
    
    myResponse.consultamodo_operacion = modoOperacion;
    myResponse.interrpvalvula = interrValvula;
    
    esp_err_t result = esp_now_send(receiverMAC, (uint8_t *) &myResponse, sizeof(myResponse));
    
    if (result == ESP_OK) {
        Serial.printf("📤 Respuesta ESP-NOW enviada - Válvula: %s, Modo: %d\n", 
                     interrValvula ? "ABIERTA" : "CERRADA", 
                     modoOperacion);
    } else {
        Serial.println("❌ Error enviando respuesta ESP-NOW");
    }
}

// ==============================
// CONTROL DE VÁLVULA DESDE UI
// ==============================
static void valve_switch_event_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * sw = (lv_obj_t *)lv_event_get_target(e);
        bool estado = lv_obj_has_state(sw, LV_STATE_CHECKED);
        
        if (xSemaphoreTake(xControlMutex, portMAX_DELAY) == pdTRUE) {
            estadovalvula = estado ? 1 : 0;
            consultamodo_operacion= estado ? 1 : 0;
            interrpvalvula= estado ? 1 : 0;
            xSemaphoreGive(xControlMutex);
        }
        
        // Guardar evento de cambio desde UI
        //guardarLogEvento("Cambio_valvula_UI_" + String(estado ? "ON" : "OFF"));
        
        enviarRespuesta();
        last_valvula_time = millis();
    }
}

void setup_valve_switch() {
    if (objects.switch_value != NULL) {
        lv_obj_add_event_cb(objects.switch_value, valve_switch_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
        
        if (xSemaphoreTake(xControlMutex, portMAX_DELAY) == pdTRUE) {
            if (interrpvalvula == 1) {
                lv_obj_add_state(objects.switch_value, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(objects.switch_value, LV_STATE_CHECKED);
            }
            xSemaphoreGive(xControlMutex);
        }
        
        Serial.println("✅ Switch_value configurado para control ESP-NOW");
    } else {
        Serial.println("❌ Switch_value no encontrado");
    }
}

// ==============================
// FUNCIONES PARA COMUNICACIÓN LoRa
// ==============================
void limpiarBufferLoRa() {
    while (SerialPort.available()) {
        SerialPort.read();
    }
}

void sendCmd(String cmd) {
    Serial.println("📤 ENVIANDO: " + cmd);
    
    limpiarBufferLoRa();
    
    SerialPort.println(cmd);
    SerialPort.flush();
    
    delay(200);
    
    String response = "";
    unsigned long startTime = millis();
    
    while (millis() - startTime < 2000) {
        if (SerialPort.available()) {
            char c = SerialPort.read();
            response += c;
            
            if (response.endsWith("\r\n") || response.indexOf("+OK") != -1 || response.indexOf("+RCV=") != -1) {
                break;
            }
        }
        delay(10);
    }
    
    if (response.length() > 0) {
        Serial.println("✅ RESPUESTA: " + response);
        
        if (response.indexOf("+RCV=") != -1) {
            bufferComandos = response;
            comandoCompleto = true;
        }
    } else {
        Serial.println("❌ SIN RESPUESTA");
    }
    
    delay(100);
}

void leerComandosLoRa() {
    static String bufferTemporal = "";
    
    while (SerialPort.available()) {
        char c = SerialPort.read();
        bufferTemporal += c;

        if (bufferTemporal.endsWith("\r\n")) {
            bufferComandos = bufferTemporal;
            bufferTemporal = "";
            comandoCompleto = true;
            Serial.println("📨 Mensaje LoRa completo recibido");
            
        }
        
        delay(1);
    }
    
    if (bufferTemporal.length() > 200) {
        Serial.println("⚠️ Buffer LoRa demasiado grande, limpiando...");
        bufferTemporal = "";
    }
}

bool esNumeroValido(String str) {
    if (str.length() == 0) return false;
    
    for (int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (!(c >= '0' && c <= '9') && c != '-' && c != '+' && c != '.') {
            return false;
        }
    }
    return true;
}

void parsearComandos(String datos) {
    String partes[5];
    int numPartes = 0;
    
    int ultimaPos = 0;
    for (int i = 0; i <= datos.length() && numPartes < 4; i++) {
        if (i == datos.length() || datos.charAt(i) == ',') {
            if (i > ultimaPos) {
                partes[numPartes] = datos.substring(ultimaPos, i);
                partes[numPartes].trim();
                numPartes++;
            }
            ultimaPos = i + 1;
        }
    }
    
    Serial.println("Número de partes encontradas: " + String(numPartes));
    
    for (int i = 0; i < numPartes && i < 3; i++) {
        Serial.println("Parte " + String(i) + ": [" + partes[i] + "]");
        
        if (partes[i].length() > 0) {
            switch(i) {
                case 0: // MODO_OPERACION
                    if (esNumeroValido(partes[i])) {
                        if (xSemaphoreTake(xControlMutex, portMAX_DELAY) == pdTRUE) {
                            consultamodo_operacion = partes[i].toInt();
                            xSemaphoreGive(xControlMutex);
                        }
                        Serial.println("✓ Modo Operación: " + String(partes[i].toInt()));
                    }
                    break;
                case 1: // INTERR_VALVULA
                    if (esNumeroValido(partes[i])) {
                        if (xSemaphoreTake(xControlMutex, portMAX_DELAY) == pdTRUE) {
                            interrpvalvula = partes[i].toInt();
                            xSemaphoreGive(xControlMutex);
                        }
                        Serial.println("✓ Interrupción Válvula: " + String(partes[i].toInt()));
                        
                     
                    }
                    break;
                case 2: // FECHA
                    fecha = partes[i];
                    Serial.println("✓ Fecha: " + fecha);
                    break;
            }
        }
    }
    
    Serial.println("=== COMANDOS PROCESADOS ===");
}

void procesarBufferCompleto() {
    if (!comandoCompleto || bufferComandos.length() == 0) return;
    
    Serial.println("🔍 Procesando mensaje LoRa: [" + bufferComandos + "]");
    
    int posicion = 0;
    bool mensajeProcesado = false;
    
    while (posicion < bufferComandos.length()) {
        int inicioRCV = bufferComandos.indexOf("+RCV=", posicion);
        if (inicioRCV == -1) break;
        
        int finRCV = bufferComandos.indexOf("\r\n", inicioRCV);
        if (finRCV == -1) {
            finRCV = bufferComandos.length();
        }
        
        String mensajeRCV = bufferComandos.substring(inicioRCV, finRCV);
        Serial.println("🎯 Mensaje RCV encontrado: [" + mensajeRCV + "]");
        
        if (mensajeRCV.length() > 5) {
            procesarMensajeLoRaRecibido(mensajeRCV);
            mensajeProcesado = true;
        }
        
        posicion = finRCV + 2;
    }
    
    if (!mensajeProcesado) {
        if (bufferComandos.indexOf("+OK") != -1) {
            Serial.println("✅ Comando AT ejecutado correctamente");
            puedeEnviarLora = true;
            debeEsperarRespuesta = false;
        }
    }
    
    strncpy((char*)lora_data.ultima_respuesta, bufferComandos.c_str(), 
            sizeof(lora_data.ultima_respuesta) - 1);
    lora_data.ultima_respuesta[sizeof(lora_data.ultima_respuesta) - 1] = '\0';
    lora_data.contador_recepciones++;
    lora_data.ultima_recepcion_time = millis();
    
    bufferComandos = "";
    comandoCompleto = false;
    
    Serial.println("🔄 Buffer limpiado");
}

void procesarMensajeLoRaRecibido(String mensaje) {
    Serial.println("🔊 PROCESANDO MENSAJE LoRa: " + mensaje);
    
    if (!mensaje.startsWith("+RCV=")) {
        Serial.println("❌ Formato de mensaje incorrecto");
        return;
    }
    
    int primeraComa = mensaje.indexOf(',');
    if (primeraComa == -1) {
        Serial.println("❌ Formato inválido - no se encontró primera coma");
        return;
    }
    
    int segundaComa = mensaje.indexOf(',', primeraComa + 1);
    if (segundaComa == -1) {
        Serial.println("❌ Formato inválido - no se encontró segunda coma");
        return;
    }
    
    String longitudStr = mensaje.substring(primeraComa + 1, segundaComa);
    int longitudEsperada = longitudStr.toInt();
    
    Serial.println("Longitud esperada de datos: " + String(longitudEsperada));
    
    int inicioDatos = segundaComa + 1;
    int finDatos = inicioDatos + longitudEsperada;
    
    if (finDatos > mensaje.length()) {
        Serial.println("❌ Mensaje incompleto");
        return;
    }
    
    String datosLimpios = mensaje.substring(inicioDatos, finDatos);
    datosLimpios.trim();
    
    Serial.println("Datos extraídos: [" + datosLimpios + "]");
    
    int contarComas = 0;
    for (int i = 0; i < datosLimpios.length(); i++) {
        if (datosLimpios.charAt(i) == ',') {
            contarComas++;
        }
    }
    Serial.println("Comas encontradas en datos: " + String(contarComas));
    
    parsearComandos(datosLimpios);
    
    puedeEnviarLora = true;
    debeEsperarRespuesta = false;
    Serial.println("✅ Mensaje LoRa procesado completamente");
}

void verificarEstadoLoRa() {
    static unsigned long ultimaVerificacion = 0;
    
    if (millis() - ultimaVerificacion >= 20000) {
        ultimaVerificacion = millis();
        
        Serial.println("\n🔍 DIAGNÓSTICO LoRa:");
        Serial.println("  📊 Buffer comandos: " + String(bufferComandos.length()) + " chars");
        Serial.println("  🚩 Comando completo: " + String(comandoCompleto ? "SI" : "NO"));
        Serial.println("  📤 Puede enviar: " + String(puedeEnviarLora ? "SI" : "NO"));
        Serial.println("  ⏳ Espera respuesta: " + String(debeEsperarRespuesta ? "SI" : "NO"));
        Serial.println("  📟 Contador recepciones: " + String(lora_data.contador_recepciones));
        Serial.println("  🔄 Contador envíos: " + String(lora_data.contador_envios));
    }
}

void enviarDatosLoRa() {
    unsigned long ahora = millis();
    bool enviar_completo = false;
    bool enviar_valvula = false;

    // Verificar si es tiempo de enviar datos completos
    if (ahora - ultimo_envio_completo >= INTERVALO_COMPLETO) {
        enviar_completo = true;
        ultimo_envio_completo = ahora;
    }
    
    // Verificar si es tiempo de enviar estado de válvula
    if (ahora - ultimo_envio_valvula >= INTERVALO_VALVULA) {
        enviar_valvula = true;
        ultimo_envio_valvula = ahora;
    }
    
    if (!enviar_completo && !enviar_valvula) {
        return;
    }

    float humedad = 0.0, temperatura = 0.0, ph = 0.0, conductividad = 0.0;
    uint16_t nitrogeno = 0, fosforo = 0, potasio = 0, salinidad = 0, tds = 0;
    int estadoValvula = 0;
    
    if (xSemaphoreTake(xSensorMutex, portMAX_DELAY) == pdTRUE) {
        humedad = Humedad;
        temperatura = Temperatura;
        ph = pH;
        conductividad = Conductividad;
        nitrogeno = Nitrogeno;
        fosforo = Fosforo;
        potasio = Potasio;
        salinidad = Salinidad;
        tds = TDS;
        xSemaphoreGive(xSensorMutex);
    }
    
    if (xSemaphoreTake(xControlMutex, portMAX_DELAY) == pdTRUE) {
        estadoValvula = estadovalvula;
        xSemaphoreGive(xControlMutex);
    }

    String data = "";
    String tipo_envio = "";

    if (enviar_completo) {
        // Enviar solo datos completos (sin estado de válvula)
        data = "S," + String(humedad, 1) + "," + String(temperatura, 1) + "," +
               String(ph, 1) + "," + String(conductividad, 1) + "," +
               String(nitrogeno) + "," + String(fosforo) + "," +
               String(potasio) + "," + String(salinidad) + "," +
               String(tds);
        tipo_envio = "SOLO_SENSORES";
        printDebug("📊 Enviando LoRa (SOLO_SENSORES): " + data);
        
    } else if (enviar_valvula) {
        // ENVÍO MEJORADO: Estado + Fuente de control
        data = "V," + String(estadoValvula) + "," + "fuenteControl";
        tipo_envio = "VÁLVULA_CON_FUENTE";
        printDebug("🎯 Enviando LoRa (VÁLVULA_CON_FUENTE): " + data + 
                  " - Fuente: " + "fuenteControl");
    }
    
    // Actualizar estadísticas de envío
    strncpy((char*)lora_data.ultimo_envio, data.c_str(), sizeof(lora_data.ultimo_envio) - 1);
    lora_data.ultimo_envio[sizeof(lora_data.ultimo_envio) - 1] = '\0';
    lora_data.contador_envios++;
    lora_data.ultimo_envio_time = millis();

    // Enviar comando LoRa
    String comando = "AT+SEND=2," + String(data.length()) + "," + data;
    
    printDebug("📤 Enviando LoRa: " + data);
    sendCmd(comando);
    
    
    puedeEnviarLora = false;
    debeEsperarRespuesta = true;
    tiempoEnvioLora = ahora;

    printDebug("⏰ Próximo envío - Sensores: " + 
               String((INTERVALO_COMPLETO - (ahora - ultimo_envio_completo)) / 1000) + 
               "s, Válvula: " + 
               String((INTERVALO_VALVULA - (ahora - ultimo_envio_valvula)) / 1000) + "s");
}

// ==============================
// FUNCIÓN PARA ACTUALIZAR INTERFAZ
// ==============================
void actualizarUI() {
    
    if(xSemaphoreTake(xLVGLMutex, portMAX_DELAY) == pdTRUE) {

        // SINCRONIZAR SWITCH
        if (objects.switch_value != NULL) {
            int estado_actual = 0;
            if (xSemaphoreTake(xControlMutex, portMAX_DELAY) == pdTRUE) {
                estado_actual = interrpvalvula;
                estadovalvula = estado_actual;
                xSemaphoreGive(xControlMutex);
            }
    
            if (millis() - last_valvula_time > 1000) {
                bool switch_debe_estar = (estado_actual == 1);
                bool switch_esta_actualmente = lv_obj_has_state(objects.switch_value, LV_STATE_CHECKED);
        
                if (switch_debe_estar != switch_esta_actualmente) {
                    if (switch_debe_estar) {
                        lv_obj_add_state(objects.switch_value, LV_STATE_CHECKED);
                    } else {
                        lv_obj_clear_state(objects.switch_value, LV_STATE_CHECKED);
                    }
                }
            }
        }

        
        // INDICADOR ESP-NOW VÁLVULA
        if (objects.indicador_1 != NULL) {
            bool espNowValvulaActivo = peerRegistered && (millis() - last_valvula_time < 10000);
            
            if (espNowValvulaActivo) {
                lv_led_set_color(objects.indicador_1, lv_color_hex(0xFF00FF00));
                lv_led_on(objects.indicador_1);
            } else {
                lv_led_set_color(objects.indicador_1, lv_color_hex(0xFFFF0000));
                lv_led_on(objects.indicador_1);
            }
        }
        
        
        // INDICADOR ESP-NOW SENSOR
        if (objects.indicador_2 != NULL) {
            bool espNowSensorActivo = peerRegistered && (millis() - last_sensor_time < 10000);
            
            if (espNowSensorActivo) {
                lv_led_set_color(objects.indicador_2, lv_color_hex(0xFF00FF00));
                lv_led_on(objects.indicador_2);
            } else {
                lv_led_set_color(objects.indicador_2, lv_color_hex(0xFFFF0000));
                lv_led_on(objects.indicador_2);
            }
        }
        
        
        // ESTADO DE VÁLVULA
        if (objects.indicador_5_2 != NULL) {
            if (estadovalvula) {
            //lv_led_set_color(objects.indicador_5, lv_color_hex(0xFF00FF00));
               // lv_led_on(objects.indicador_5);
                lv_led_set_color(objects.indicador_5_2, lv_color_hex(0xFF00FF00));
                lv_led_on(objects.indicador_5_2);
            } else {
                //lv_led_set_color(objects.indicador_5, lv_color_hex(0xFFFF0000));
                //lv_led_on(objects.indicador_5);
                lv_led_set_color(objects.indicador_5_2, lv_color_hex(0xFFFF0000));
                lv_led_on(objects.indicador_5_2);
            }   
        }
        
        // DATOS LoRa
        if (objects.lora_envios_value != NULL) {
            char lora_info[1024];
            snprintf(lora_info, sizeof(lora_info),
            LV_SYMBOL_WIFI" ESTADO LoRa:\n"
            LV_SYMBOL_UPLOAD" Envios: %d  \n"
            LV_SYMBOL_DRIVE" Ultimo: %lu seg\n"
            LV_SYMBOL_UPLOAD" Enviado: %.30s\n"
            LV_SYMBOL_DOWNLOAD" Recibido: %.50s\n"
            LV_SYMBOL_SETTINGS" Estado: %s",
            lora_data.contador_envios,
            (millis() - max(lora_data.ultimo_envio_time, lora_data.ultima_recepcion_time)) / 1000,
            lora_data.ultimo_envio,
            lora_data.ultima_respuesta,
            (puedeEnviarLora && !debeEsperarRespuesta) ? "LISTO" : (debeEsperarRespuesta ? "ESPERANDO" : "ERROR")
            );

            lv_textarea_set_text(objects.lora_envios_value, lora_info);
        }
        
        // DATOS DE SENSORES NPK
        if (objects.temperatura_value != NULL) {
            char temp_str[16];
            sprintf(temp_str, "%.1f°C", Temperatura);
            lv_label_set_text(objects.temperatura_value, temp_str);
            lv_label_set_text(objects.temperatura_value_2, temp_str);
            if (Temperatura > 30.0) {
                lv_obj_set_style_text_color(objects.temperatura_value, lv_color_hex(0xFFFF0000), 0);
            } else if (Temperatura < 15.0) {
                lv_obj_set_style_text_color(objects.temperatura_value, lv_color_hex(0xFF0000FF), 0);
            } else {
                lv_obj_set_style_text_color(objects.temperatura_value, lv_color_hex(0x00000000), 0);
            }
        }
        
        if (objects.humedad_value != NULL) {
            char hum_str[16];
            sprintf(hum_str, "%.1f%%", Humedad);
            lv_label_set_text(objects.humedad_value, hum_str);
            lv_label_set_text(objects.humedad_value_2, hum_str);

            if (Humedad < 30.0) {
                lv_obj_set_style_text_color(objects.humedad_value, lv_color_hex(0xFFFF0000), 0);
            } else if (Humedad > 70.0) {
                lv_obj_set_style_text_color(objects.humedad_value, lv_color_hex(0xFFFFFF00), 0);
            } else {
                lv_obj_set_style_text_color(objects.humedad_value, lv_color_hex(0xFF00FF00), 0);
            }
        }
        
        if (objects.ph_value != NULL) {
            char ph_str[16];
            sprintf(ph_str, "%.1f", pH);
            lv_label_set_text(objects.ph_value, ph_str);
            lv_label_set_text(objects.ph_value_2, ph_str);
            if (pH < 6.0 || pH > 7.5) {
                lv_obj_set_style_text_color(objects.ph_value, lv_color_hex(0xFFFF0000), 0);
            } else {
                lv_obj_set_style_text_color(objects.ph_value, lv_color_hex(0xFF00FF00), 0);
            }
        }


        // Luego puedes usarlos normalmente
        if(objects.btn_ph != NULL) {
        }
        

        if (objects.nitrogeno_value != NULL) {
            char n_str[16];
            sprintf(n_str, "%u mg/kg", Nitrogeno);
            lv_label_set_text(objects.nitrogeno_value, n_str);
            lv_label_set_text(objects.nitrogeno_value_2, n_str);
        }
        
        if (objects.fosforo_value != NULL) {
            char p_str[16];
            sprintf(p_str, "%u mg/kg", Fosforo);
            lv_label_set_text(objects.fosforo_value, p_str);
                lv_label_set_text(objects.fosforo_value_2, p_str);
        }
        
        if (objects.potasio_value != NULL) {
            char k_str[16];
            sprintf(k_str, "%u mg/kg", Potasio);
            lv_label_set_text(objects.potasio_value, k_str);
            lv_label_set_text(objects.potasio_value_2, k_str);
        }
        
        
        // CONDUCTIVIDAD Y SALINIDAD
        if (objects.conductividad_value != NULL) {
            char cond_str[16];
            sprintf(cond_str, "%.1f uS/cm", Conductividad);
            lv_label_set_text(objects.conductividad_value, cond_str);
            lv_label_set_text(objects.conductividad_value_2, cond_str);
        }
        
        if (objects.salinidad_value != NULL) {
            char sal_str[16];
            sprintf(sal_str, "%u mg/L", Salinidad);
            lv_label_set_text(objects.salinidad_value, sal_str);
                lv_label_set_text(objects.salinidad_value_2, sal_str);
        }
        
        if (objects.tds_value != NULL) {
            char tds_str[16];
            sprintf(tds_str, "%u mg/L", TDS);
            lv_label_set_text(objects.tds_value, tds_str);
            lv_label_set_text(objects.tds_value_2, tds_str);
        }

        
        // INFORMACIÓN DE SISTEMA
        if (objects.system_info != NULL) {
            int hora=tiempoASegundos(fecha);
            char sys_info[128];
            sprintf(sys_info, "Fecha: %s \nHora: %u seg\nSD: %s", 
                    fecha.c_str(),
                    hora,
                    sdCardInicializada ? "OK" : "NO");
            lv_label_set_text(objects.system_info, sys_info);
        }
        
        xSemaphoreGive(xLVGLMutex);
    }
}

// ==============================
// TAREA PARA GUARDADO EN SD
// ==============================
void tareaGuardadoSD(void *parameter) {
    Serial.println("💾 Tarea guardado SD iniciada");
    
    while(1) {
        unsigned long ahora = millis();
        
        // Guardar datos cada intervalo configurado
        if (ahora - ultimoGuardadoSD >= INTERVALO_GUARDADO_SD) {
            ultimoGuardadoSD = ahora;
            
            if(sdCardInicializada) {
                guardarDatosEnSD();
                verificarEspacioSD();
                Serial.println("💾 Guardado automático en SD completado");
            }
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// ==============================
// INTERFAZ DE USUARIO
// ==============================
void create_ui() { 
    Serial.println("🎨 Inicializando interfaz de usuario...");
    setup_valve_switch();
    if(xSemaphoreTake(xLVGLMutex, portMAX_DELAY) == pdTRUE) {
        create_screen_main();
        lv_disp_load_scr(objects.main);
        if(objects.grafica_a != NULL) {
            serie_test = lv_chart_add_series(objects.grafica_a, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
            lv_chart_set_point_count(objects.grafica_a, 15);

            

            lv_chart_set_next_value(objects.grafica_a,serie_test,Temperatura);
        }
        xSemaphoreGive(xLVGLMutex);
    }
    
    Serial.println("✅ Interfaz de usuario inicializada");
}

// ==============================
// TAREAS FreeRTOS
// ==============================
void tarea_actualizacion_datos(void *parameter) {
    Serial.println("🚀 Tarea de datos iniciada");
    
    unsigned long ultima_lectura = 0;
    
    while(1) {
        unsigned long ahora = millis();
        
        if (ahora - ultima_lectura >= 500) {
            ultima_lectura = ahora;
            actualizar_grafica = true;
            contador_tarea1++;
        }
        
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void tarea_comunicaciones(void *parameter) {
    Serial.println("📡 Tarea comunicaciones iniciada");
    
    unsigned long ultimo_envio_espnow = 0;
    unsigned long ultimo_envio_lora = 0;
    unsigned long ultimo_diagnostico = 0;

    while(1) {
        unsigned long ahora = millis();
        
        // LEER DATOS LoRa
        if (SerialPort.available()) {
            leerComandosLoRa();
        }
        
        // PROCESAR COMANDOS
        if (comandoCompleto) {
            procesarBufferCompleto();
        }
        
        // DIAGNÓSTICO
        if (ahora - ultimo_diagnostico >= 10000) {
            ultimo_diagnostico = ahora;
            verificarEstadoLoRa();
        }
        
        // ENVIOS ESP-NOW
        if (ahora - ultimo_envio_espnow >= 10000) {
            ultimo_envio_espnow = ahora;
            enviarRespuesta();
        }
        
        // ENVIOS LoRa
        if (ahora - ultimo_envio_lora >= 10000 && puedeEnviarLora && !debeEsperarRespuesta) {
            ultimo_envio_lora = ahora;
            enviarDatosLoRa();
        }
        
        // TIMEOUT LoRa
        if (debeEsperarRespuesta && (ahora - tiempoEnvioLora > TIMEOUT_LORA)) {
            Serial.println("⏰ TIMEOUT LoRa - Reintentando...");
            puedeEnviarLora = true;
            debeEsperarRespuesta = false;
        }
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void tarea_lvgl_handler(void *parameter) {
    Serial.println("🎨 Tarea LVGL iniciada");
    
    unsigned long ultima_actualizacion = 0;
    unsigned long lv_last_tick = millis();
    unsigned long ultimo_backlight_check = 0;
    
    while(1) {
        unsigned long ahora = millis();
        
        if(xSemaphoreTake(xLVGLMutex, portMAX_DELAY) == pdTRUE) {
            
            lv_tick_inc(ahora - lv_last_tick);
            lv_last_tick = ahora;
            
            lv_timer_handler();
            ui_tick();

            if(objects.titulo_grafica != NULL) {
                // Obtener el texto del label
                const char* texto = lv_label_get_text(objects.titulo_grafica);
        
                // Procesar el texto
                if(strlen(texto) > 0) {
                    //Serial.printf("📝 Título de gráfica: %s\n", texto);
            
                    // Guardar en una variable global
                    Grafica = String(texto);
            
                } else {
                    Serial.println("⚠️ El label está vacío");
                }
            }

            if (actualizar_grafica && objects.grafica_a != NULL && serie_test != NULL) {
                if(Grafica=="Ph"){
                    lv_scale_set_range(objects.escala, 0, 10);
                    lv_scale_set_total_tick_count(objects.escala, 21);   
                    lv_scale_set_major_tick_every(objects.escala, 2);
                    lv_chart_set_range(objects.grafica_a, LV_CHART_AXIS_PRIMARY_Y, 0, 10);
                    lv_chart_set_next_value(objects.grafica_a,serie_test,pH);
                }else if (Grafica=="Humedad"){
                    lv_scale_set_range(objects.escala, 0, 100);
                    lv_scale_set_total_tick_count(objects.escala, 51);   
                    lv_scale_set_major_tick_every(objects.escala, 5);
                    lv_chart_set_range(objects.grafica_a, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
                    lv_chart_set_next_value(objects.grafica_a,serie_test,Humedad);
                }else if (Grafica=="Temperatura"){
                    lv_scale_set_range(objects.escala, 0, 40);
                    lv_scale_set_total_tick_count(objects.escala, 41);   
                    lv_scale_set_major_tick_every(objects.escala, 4);
                    lv_chart_set_range(objects.grafica_a, LV_CHART_AXIS_PRIMARY_Y, 0, 40);
                    lv_chart_set_next_value(objects.grafica_a,serie_test,Temperatura);
                }else if (Grafica=="Conductividad"){
                    lv_scale_set_range(objects.escala, 0, 3600);
                    lv_scale_set_total_tick_count(objects.escala, 16);   
                    lv_scale_set_major_tick_every(objects.escala, 1);     
                    lv_chart_set_range(objects.grafica_a, LV_CHART_AXIS_PRIMARY_Y, 0, 3600);                    
                    lv_chart_set_next_value(objects.grafica_a,serie_test,Conductividad);
                }else if (Grafica=="Salinidad"){
                    lv_scale_set_range(objects.escala, 0, 1500);
                    lv_scale_set_total_tick_count(objects.escala, 16);   
                    lv_scale_set_major_tick_every(objects.escala, 1);
                    lv_chart_set_range(objects.grafica_a, LV_CHART_AXIS_PRIMARY_Y, 0, 1500);
                    lv_chart_set_next_value(objects.grafica_a,serie_test,Salinidad);    
                }else if (Grafica=="TDS") {
                    lv_scale_set_range(objects.escala, 0, 1500);
                    lv_scale_set_total_tick_count(objects.escala, 16);   
                    lv_scale_set_major_tick_every(objects.escala, 1);                    
                    lv_chart_set_range(objects.grafica_a, LV_CHART_AXIS_PRIMARY_Y, 0, 1500);
                    lv_chart_set_next_value(objects.grafica_a,serie_test,TDS);
                }else if (Grafica=="Nitrogeno") {
                    lv_scale_set_range(objects.escala, 0, 1300);
                    lv_scale_set_total_tick_count(objects.escala, 16);   
                    lv_scale_set_major_tick_every(objects.escala, 1);
                    lv_chart_set_range(objects.grafica_a, LV_CHART_AXIS_PRIMARY_Y, 0, 1300);
                    lv_chart_set_next_value(objects.grafica_a,serie_test,Nitrogeno);
                }else if (Grafica=="Fosforo") {
                    lv_scale_set_range(objects.escala, 0, 1300);
                    lv_scale_set_total_tick_count(objects.escala, 16);   
                    lv_scale_set_major_tick_every(objects.escala, 1);
                    lv_chart_set_range(objects.grafica_a, LV_CHART_AXIS_PRIMARY_Y, 0, 1300);
                    lv_chart_set_next_value(objects.grafica_a,serie_test,Fosforo);
                }else if (Grafica=="Potasio") {
                    lv_scale_set_range(objects.escala, 0, 1300);
                    lv_scale_set_total_tick_count(objects.escala, 16);   
                    lv_scale_set_major_tick_every(objects.escala, 1);
                    lv_chart_set_range(objects.grafica_a, LV_CHART_AXIS_PRIMARY_Y, 0, 1300);
                    lv_chart_set_next_value(objects.grafica_a,serie_test,Potasio);
                }

                //lv_chart_refresh(objects.grafica_a);
                actualizar_grafica = false;
                contador_tarea2++;
            }
            
            xSemaphoreGive(xLVGLMutex);

            if (ahora - ultima_actualizacion >= 1000) {
                ultima_actualizacion = ahora;
                actualizarUI();
            }

            if (ahora - ultimo_backlight_check >= 5000) {
                ultimo_backlight_check = ahora;
            }
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ==============================
// SETUP PRINCIPAL
// ==============================
void setup() {
#ifdef ARDUINO_USB_CDC_ON_BOOT
    delay(1000);
#endif

    Serial.begin(115200);
    Serial.setDebugOutput(true);
    
    Serial.println("\n🚀 INICIANDO SISTEMA CON SD CARD");

    smartdisplay_init();    
    
    xLVGLMutex = xSemaphoreCreateMutex();
    xControlMutex = xSemaphoreCreateMutex();
    xSensorMutex = xSemaphoreCreateMutex();

    if (xLVGLMutex == NULL) {
        Serial.println("❌ Error crítico: No se pudo crear mutex LVGL");
        while(1) delay(1000);
    }

    // INICIALIZAR SD CARD
    #if ENABLE_SD_CARD
    Serial.println("💾 Inicializando SD card...");
    if(inicializarSD()) {
        Serial.println("✅ SD card lista para usar");
    } else {
        Serial.println("⚠️  SD card no disponible, continuando sin almacenamiento");
    }
    #endif
    delay(100);
    ui_init();
    create_ui();

    xTaskCreatePinnedToCore(
        tarea_lvgl_handler,
        "Tarea_LVGL",
        12288,
        NULL,
        3,
        NULL,
        1
    );

    SerialPort.begin(115200, SERIAL_8N1, RXD2, TXD2);
    delay(1000);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ Error inicializando ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);

    Serial.println("🔧 Configurando módulo LoRa...");
    sendCmd("AT");
    delay(500);
    sendCmd("AT+PARAMETER=7,7,1,7");
    delay(500);
    sendCmd("AT+ADDRESS=1");
    delay(500);
    sendCmd("AT+NETWORKID=21");
    delay(500);
    sendCmd("AT+BAND=915000000");
    delay(500);
    
    Serial.println("📊 Verificando configuración LoRa:");
    sendCmd("AT+ADDRESS?");
    delay(300);
    sendCmd("AT+NETWORKID?");
    delay(300);

    Serial.println("✅ Configuración LoRa completada");

    Serial.println("🎯 Creando tareas...");
    
    xTaskCreatePinnedToCore(
        tarea_actualizacion_datos,
        "Tarea_Datos",
        12288,
        NULL,
        1,
        NULL,
        1
    );
    
    xTaskCreatePinnedToCore(
        tarea_comunicaciones,
        "Tarea_Comms",
        12288,
        NULL,
        2,
        NULL,
        1
    );
    
    #if ENABLE_SD_CARD
    xTaskCreatePinnedToCore(
        tareaGuardadoSD,
        "Tarea_SD",
        12288,
        NULL,
        1,
        NULL,
        1
    );
    #endif
    


    Serial.println("✅ Sistema listo con SD Card");
}

// ==============================
// LOOP PRINCIPAL
// ==============================
void loop() {
    
    static unsigned long ultimo_monitoreo = 0;
    if (millis() - ultimo_monitoreo >= 30000) {
        ultimo_monitoreo = millis();
        Serial.printf("📊 Monitor - Heap: %d, Tareas: %d/%d, SD: %s\n", 
                     ESP.getFreeHeap(), contador_tarea1, contador_tarea2,
                     sdCardInicializada ? "OK" : "NO");
    }
    
    delay(1000);
}