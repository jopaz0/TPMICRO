//https://www.tinkercad.com/things/3fWcA0IAWxG-copy-of-puerta-gatuna-20/editel
//ESTADOS: 6 estados posibles y variables de uso
//en cualquier caso, se abre al presionar el boton de abrir puerta o al detectar movimiento dentro del tunel
//0=cerrado normal, -1=solo se abre desde fuera, -2=no se abre excepto x movimiento adentro del tunel
#define BLOQUEADO -2
#define SOLOINGRESO -1
#define NORMAL 0
#define ABRIENDO 1
#define ABIERTO 2
#define CERRANDO 3
int estado_actual, estado_anterior, tipo_cierre;

//TIEMPOS: definiciones de tiempo de apertura
#define CORTO 1500
#define LARGO 3000
int tiempo_apertura;

//===============================================================================================================
//INPUTS
//*in=input, rfid=sensor de radiofrecuencia, finc=final de carrera, int=interno, ext=externo, 
//sup=superior, inf=inferior, mov=sensor de movimiento, btn=boton, conf=configurar
//---------------------------------------------------------------------------------------------------------------
//UTILIDADES para configuracion de entradas
#define CONF_INPUT(sensor)  pinMode(sensor, INPUT)
#define LEER_SENSOR(sensor)  digitalRead(sensor)
//---------------------------------------------------------------------------------------------------------------
//SENSORES
// los 4 finales de carrera podria leerlos como 2, lo que me dejaria manejar todas las entradas con un byte
#define IN_RFID_INT     0
#define IN_RFID_EXT     13
#define IN_FINC_INT_SUP 2
#define IN_FINC_INT_INF 3
#define IN_FINC_EXT_SUP 4
#define IN_FINC_EXT_INF 5
#define IN_MOV_INT      6
const int pins_sensores[7] = {IN_RFID_INT, IN_RFID_EXT, IN_FINC_INT_INF, IN_FINC_EXT_INF, IN_MOV_INT, IN_FINC_INT_SUP, IN_FINC_EXT_SUP};
const int numero_sensores = 7;
byte estado_sensores;

byte DETECTAR_CAMBIO_SENSORES() {
  bool estado_sensor;
  byte cambios_detectados = 0b00000000;
  byte nuevos_estados = 0;
  for (int i = 0; i < numero_sensores; i++) {
    estado_sensor = LEER_SENSOR(pins_sensores[i]);
    nuevos_estados |= (estado_sensor << i);
  }
  if (nuevos_estados == estado_sensores) return 0;
  cambios_detectados = (nuevos_estados ^ estado_sensores);
  estado_sensores = nuevos_estados;
  return cambios_detectados;
}

//---------------------------------------------------------------------------------------------------------------
//BOTONES
#define IN_BTN_TIPO_CIERRE   7
#define IN_BTN_APERTURA 8
#define IN_BTN_TIEMPOS  9
const int pins_botones[3] = {IN_BTN_TIPO_CIERRE, IN_BTN_APERTURA, IN_BTN_TIEMPOS};
const int numero_botones = 3;
byte estado_botones;

byte DETECTAR_CAMBIO_BOTONES() {
  bool estado_sensor;
  byte botones_soltados;
  byte nuevos_estados = 0;
  for (int i = 0; i < numero_botones; i++) {
    estado_sensor = LEER_SENSOR(pins_botones[i]);
    nuevos_estados |= (estado_sensor << i);
  }
  if (nuevos_estados == estado_botones) return 0;
  botones_soltados = ((nuevos_estados ^ estado_botones) & ~nuevos_estados);
  estado_botones = nuevos_estados;
  return botones_soltados;
}

//===============================================================================================================
//OUTPUTS - se van a realizar por medio de un chip 74HC59. En el byte datos, el digito menos significativo
//IMPORTANTE, EL CHIP SE ROMPE SI LA CORRIENTE TOTAL SUPERA 50mA
//corresponde al pin 0, y va ascendiendo.
//out=output, est=estado, camb=cambiar, act=actualizar
#define CONF_OUTPUT(salida)  pinMode(salida, OUTPUT)
//---------------------------------------------------------------------------------------------------------------
//Estos pines corresponden al chip 74HC59, NO al Arduino
#define OUT_LED_CIERRE_NORMAL 0 //un led verde
#define OUT_LED_CIERRE_SOLOENTRADA 1 //un led verde 
#define OUT_LED_CIERRE_BLOQUEADO 2 //un led rojo
#define OUT_LED_TIEMPO_CORTO 3 //un led ?
#define OUT_LED_TIEMPO_LARGO 4 //un led ?
#define OUT_MOTOR_FORWARD 5
#define OUT_MOTOR_BACKWARD 6
#define OUT_MOTOR_BRIDGE 7

#define MOTOR_AVANZAR     ACTUALIZAR_BYTE(OUT_MOTOR_BRIDGE, 1); ACTUALIZAR_BYTE(OUT_MOTOR_FORWARD, 1); ACTUALIZAR_BYTE(OUT_MOTOR_BACKWARD, 0);
#define MOTOR_RETROCEDER  ACTUALIZAR_BYTE(OUT_MOTOR_BRIDGE, 1); ACTUALIZAR_BYTE(OUT_MOTOR_FORWARD, 0); ACTUALIZAR_BYTE(OUT_MOTOR_BACKWARD, 1);
#define MOTOR_PARAR       ACTUALIZAR_BYTE(OUT_MOTOR_BRIDGE, 0); ACTUALIZAR_BYTE(OUT_MOTOR_FORWARD, 0); ACTUALIZAR_BYTE(OUT_MOTOR_BACKWARD, 0);

//---------------------------------------------------------------------------------------------------------------
//Pines del Arduino reservados para el chip 74HC59
#define OUT_CHIP_DATO 10       // Pin conectado a DS de 74HC595
#define OUT_CHIP_REGISTRO 11   // Pin conectado a STCP de 74HC595
#define OUT_CHIP_RELOJ 12      // Pin conectado a SHCP de 74HC595
#define CAMB_EST_REGISTRO(x) digitalWrite(OUT_CHIP_REGISTRO, x)
const int pins_outputs_chip[3] = {OUT_CHIP_DATO, OUT_CHIP_REGISTRO, OUT_CHIP_RELOJ};
byte salidas;

//---------------------------------------------------------------------------------------------------------------
//FUNCION para pasar los valores del byte salidas al chip
void ACTUALIZAR_BYTE(int posicion, bool valor) {
  if (valor) {
    salidas |= (1 << posicion);
  } else {
    salidas &= ~(1 << posicion);
  }
  ACTUALIZAR_CHIP();
}
void ACTUALIZAR_CHIP(){
  shiftOut(OUT_CHIP_DATO, OUT_CHIP_RELOJ, MSBFIRST, salidas);
  CAMB_EST_REGISTRO(HIGH);
  CAMB_EST_REGISTRO(LOW);
}

//===============================================================================================================
//MANEJO DE EVENTOS
byte cambios_sensores, cambios_botones;
int ENCONTRAR_INDICE_VERDADERO(byte cambios) {
  for (int i = 0; i < 8; i++) {
    if ((cambios & (1 << i)) != 0) {
      return i;
    }
  }
}
void CAMBIAR_ESTADO(int nuevo){
  estado_anterior = estado_actual;
  estado_actual = nuevo;
}

//---------------------------------------------------------------------------------------------------------------

void MANEJAR_EVENTOS_BOTONES(byte cambios){
  if (!cambios) return;
  int indice = ENCONTRAR_INDICE_VERDADERO(cambios);
  if (indice >= numero_botones) return; //ESTO ES UN ERROR, DEBERIA ARROJAR UN MENSAJE SI PASA
  int boton = pins_botones[indice];
  switch (boton){
    case IN_BTN_TIPO_CIERRE:
      CICLAR_TIPO_CIERRE();
      break;
    case IN_BTN_APERTURA:
      CAMBIAR_ESTADO(ABRIENDO);
      break;
    case IN_BTN_TIEMPOS:
      CICLAR_TIEMPO_APERTURA();
      break;
    default: //OTRO LUGAR DONDE NUNCA DEBERIA CAER
      break;
  }
}

void CICLAR_TIPO_CIERRE(){
  //(esto asume que las luces son los 3 primeros, es mas dificil de mantener)
  //ciclo el tipo de cierre
  tipo_cierre = (tipo_cierre == NORMAL)? SOLOINGRESO : (tipo_cierre == SOLOINGRESO)? BLOQUEADO : NORMAL;
  //borro los 3 primeros bytes de la salida del chip
  salidas = salidas & 0b11111000;
  //cambio la posicion correspondiente a 1
  salidas |= (1 << abs(tipo_cierre));
  //mando el byte nuevo al chip
  ACTUALIZAR_CHIP();
  if (estado_actual <= NORMAL) CAMBIAR_ESTADO(tipo_cierre);
  
}
void CICLAR_TIEMPO_APERTURA(){
  //(esto asume que las luces son los 3 primeros, es mas dificil de mantener)
  //ciclo los tiempos de apertura
  tiempo_apertura = (tiempo_apertura == CORTO)? LARGO : CORTO;
  //borro los 2 ultimos bytes de la salida del chip
  salidas = salidas & 0b11100111;
  //cambio la posicion correspondiente a 1
  int indice = (tiempo_apertura == CORTO)? 3 : 4;
  salidas |= (1 << indice);
  //mando el byte nuevo al chip
  ACTUALIZAR_CHIP();
}

//---------------------------------------------------------------------------------------------------------------

void MANEJAR_EVENTOS_SENSORES(byte cambios){
  if (!cambios) return;
  int indice = ENCONTRAR_INDICE_VERDADERO(cambios);
  if (indice >= numero_sensores) return; //ESTO ES UN ERROR, DEBERIA ARROJAR UN MENSAJE SI PASA
  int sensor = pins_sensores[indice];
  switch (sensor){
    case IN_RFID_INT:
      MENSAJE_X_SERIAL("Señal detectada en el sensor interior.",0);
      if (estado_actual > SOLOINGRESO) CAMBIAR_ESTADO(ABRIENDO);
      else MENSAJE_X_SERIAL("Puerta en modo BLOQUEADA o SOLO INGRESO.",0);
      break;
    case IN_RFID_EXT:
      MENSAJE_X_SERIAL("Señal detectada en el sensor exterior.",0);
      if (estado_actual > BLOQUEADO) CAMBIAR_ESTADO(ABRIENDO);
      else MENSAJE_X_SERIAL("Puerta BLOQUEADA.",0);
      break;
    case IN_FINC_INT_SUP:
      MENSAJE_X_SERIAL("Señal detectada en el final de carrera interior superior.",0);
      if (estado_actual == ABRIENDO) CAMBIAR_ESTADO(ABIERTO);
      break;
    case IN_FINC_INT_INF:
      MENSAJE_X_SERIAL("Señal detectada en el final de carrera interior inferior.",0);
      if (estado_actual == CERRANDO) CAMBIAR_ESTADO(tipo_cierre);
      break;
    case IN_FINC_EXT_SUP:
      MENSAJE_X_SERIAL("Señal detectada en el final de carrera exterior superior.",0);
      if (estado_actual == ABRIENDO) CAMBIAR_ESTADO(ABIERTO);
      break;
    case IN_FINC_EXT_INF:
      MENSAJE_X_SERIAL("Señal detectada en el final de carrera esterior inferior.",0);
      if (estado_actual == CERRANDO) CAMBIAR_ESTADO(tipo_cierre);
      break;
    case IN_MOV_INT:
      if (estado_actual > NORMAL) return;
      MENSAJE_X_SERIAL("Movimiento detectado en el interior del tunel, con la puerta cerrada. Apertura activada.",0);
      CAMBIAR_ESTADO(ABRIENDO);
      break;
    default: //OTRO LUGAR DONDE NUNCA DEBERIA CAER
      break;
  }
}

//===============================================================================================================
//Cambiar esto a false anula todas las salidas informativas de IMPRIMIR_X_SERIAL por puerto serie

#define TESTEANDO true

void MENSAJE_X_SERIAL(String mensaje, int espera){
  if (!TESTEANDO) return;
  Serial.println("-------------------------------");
  Serial.println(mensaje);
  Serial.println("-------------------------------");
  if (espera) delay(espera);
}

void IMPRIMIR_X_SERIAL(String mensaje, int espera){
  if (!TESTEANDO) return;
  Serial.println("-------------------------------");
  Serial.println(mensaje);
  Serial.println("-------------------------------");
  Serial.print("Tipo de cierre: ");
  String aux = (tipo_cierre == NORMAL) ? "NORMAL" : ((tipo_cierre == SOLOINGRESO) ? "SOLOINGRESO" : "BLOQUEADA");
  Serial.println(aux);

  Serial.print("ESTADO ANTERIOR: ");
  Serial.print(estado_actual);
  Serial.print(" - ");
  String aux_estados[6] = {"BLOQUEADO","SOLOINGRESO","NORMAL","ABRIENDO","ABIERTO","CERRANDO"};
  aux = aux_estados[estado_anterior+2];
  Serial.println(aux);

  Serial.print("ESTADO ACTUAL: ");
  Serial.print(estado_actual);
  Serial.print(" - ");
  aux = aux_estados[estado_actual+2];
  Serial.println(aux);
  
  Serial.print("Tiempo de apertura: ");
  Serial.println(tiempo_apertura);
  Serial.print("Estado sensores: ");
  Serial.println(estado_sensores, BIN);
  Serial.print("Estado botones: ");
  Serial.println(estado_botones, BIN);
  Serial.print("Byte salida: ");
  Serial.println(salidas, BIN);
  if (espera) delay(espera);
}

//===============================================================================================================
void ACTUAR_SEGUN_ESTADO_ACTUAL(){
  unsigned static long ultima_apertura;
  switch (estado_actual){
    case ABRIENDO:
      ABRIR_PUERTAS();
      break;
    case ABIERTO:
      PARAR_MOTORES();
      if (estado_actual != estado_anterior) {
        ultima_apertura = millis();
      }
      if((millis() - ultima_apertura) > tiempo_apertura) {
        CAMBIAR_ESTADO(CERRANDO);
        IMPRIMIR_X_SERIAL("Cerrando la puerta automaticamente...",1);
      }
      break;
    case CERRANDO:
      CERRAR_PUERTAS();
      break;
    default:
      PARAR_MOTORES();
      break;
  }
  estado_anterior = estado_actual;
}
void PARAR_MOTORES(){
  MOTOR_PARAR;
}
void ABRIR_PUERTAS(){
  if ((salidas & 0b11100000) == 0b11000000) return; //esto tengo que abstraerlo
  MOTOR_AVANZAR;                      
}
void CERRAR_PUERTAS(){
  if ((salidas & 0b11100000) == 0b10100000) return; //esto tengo que abstraerlo
  MOTOR_RETROCEDER;
}
//===============================================================================================================
void setup() {
  Serial.begin(9600);
  //inicia en estado CERRANDO. Si ya esta cerrada, el final de carrera va a hacer cambiar el estado a tipo_cierre
  //ver si estos los puedo definir en una pasada de lo que sea que use para cambiar el estado
  estado_actual = CERRANDO;
  estado_anterior = CERRANDO;
  tipo_cierre = NORMAL;
  tiempo_apertura = CORTO;
  estado_sensores = 0b0;
  estado_botones = 0b0;
  salidas = 0b00001001;
  ACTUALIZAR_CHIP();

  //defino entradas y salidas usando las listas
  for(int sensor = 0; sensor < numero_sensores; sensor++) 
    CONF_INPUT(pins_sensores[sensor]);
  for(int boton = 0; boton < numero_botones; boton++) 
    CONF_INPUT(pins_botones[boton]);
  for(int output = 0; output < 3; output++) 
    CONF_OUTPUT(pins_outputs_chip[output]);

  IMPRIMIR_X_SERIAL("FIn SETUP",1);
}
void loop(){
  //hacer unos parpadeos asi bien kukis
  //LED_TEST();

  cambios_botones = DETECTAR_CAMBIO_BOTONES();
  MANEJAR_EVENTOS_BOTONES(cambios_botones);

  cambios_sensores = DETECTAR_CAMBIO_SENSORES();
  MANEJAR_EVENTOS_SENSORES(cambios_sensores);

  ACTUAR_SEGUN_ESTADO_ACTUAL();


  if (cambios_botones | cambios_sensores) IMPRIMIR_X_SERIAL("FIn LOOP, con cambios!!!",1);
  
}
