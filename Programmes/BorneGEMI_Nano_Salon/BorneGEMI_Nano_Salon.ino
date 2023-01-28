/*
 * Pour mémoire :
 *
 * UID: xxxxxxxxxxxxxxxxxxxxxxx                                   => Spécifique à chaque Nano 33 BLE Sense. Obtenu avec le programme donnant cette information indispensable (PicoVoice https://console.picovoice.ai)
 *
 * Key: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx  => Clé de licence d'utilisation gratuite (Obtenu sur le site PicoVoice https://console.picovoice.ai)
 *
 *
 *
 */

#include <Picovoice_FR.h>
#include "params.h"

#define MEMORY_BUFFER_SIZE (70 * 1024)
const char Version[] = "0.1.1";

unsigned long millisEmet;
int Delais = 4000;                                                                        // Délais d'attente maximum du "Ready" émis par l'ESP32

static const char* ACCESS_KEY = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // Chaîne Access_Key obtenue à partir de la console Picovoice (https://picovoice.ai/console/)

static pv_picovoice_t *handle = NULL;

static int8_t memory_buffer[MEMORY_BUFFER_SIZE] __attribute__((aligned(16)));

static const float PORCUPINE_SENSITIVITY = 0.75f;
static const float RHINO_SENSITIVITY = 0.5f;

void Attend(int tmp) {
  unsigned long horloge = millis() + tmp;
  while (millis() < horloge) {
    yield();
  }
}

static void wake_word_callback(void) {
  Serial.println("Activation: Hé GEMI");                                                  // Appel reconnu (fonction appelée seulement si l'appel est reconnu)
  Serial1.println("open");                                                                // Commande ouverte, l'ESP32 attend la commande => pour moi GEMI (Gestion Elmectronique de Maison Individuelle)
  digitalWrite(LED_BUILTIN, HIGH);
}

static void inference_callback(pv_inference_t *inference) {
  String Cde = "";

  String r = "";
  millisEmet = millis() + Delais;
  while (r != "ready") {                                                                  // On attend le "Ready" avant de continuer, sinon commande avortée.
    if(millis() > millisEmet) break;
    while (Serial1.available()) {
      char s = Serial1.read();
      r += s;
      r.replace("\r", "");
      r.replace("\n", "");
      r.trim();
      r.toLowerCase();
      millisEmet = millis() + Delais;
    }
  }
  if (r == "") Serial1.println(F("erreur"));                                              // Expression "Ready" non reconnue
  Serial.print(F("R = "));
  Serial.println(r);

  Serial.println("{");
  Serial.print("    Compris : ");
  bool Compris = inference->is_understood;
  Serial.println(Compris == true ? "true" : "false");                                     // Pour le debug
  if (Compris == true) {
    Serial.print("    concerne : ");                                                      // Structure d'un fichier JASON
    Serial.println(inference->intent);
//    Cde = "conserne:" + String(inference->intent);
    if (inference->num_slots > 0) {
      Serial.println("    faire : {");
      for (int32_t i = 0; i < inference->num_slots; i++) {
        Serial.print("        ");
        Serial.print(inference->slots[i]);
        Serial.print(" : ");
        Serial.println(inference->values[i]);
        if (Cde != "") Cde += ",";
        Cde += String(inference->slots[i]) + ":" + String(inference->values[i]);          // Transformation pour mes besoins en une chaine de caractère (pas de JASON à décoder avec l'ESP32
//        Serial1.print(Cde);
      }
//      Serial1.println();
      Serial.println("    }");
      Serial.print(F("Cde = "));
      Serial.println(Cde);
      Serial1.print(Cde);
    }
  } else {
    Serial1.println(F("erreur"));                                                         // Commande incomprise, le mot "erreur" est envoyé à l'ESP32
  }
  Serial.println("}\n");
  digitalWrite(LED_BUILTIN, LOW);
  pv_inference_delete(inference);
}

void setup() {
  Serial.begin(115200);                                                                   // Pour le debug
  //while (!Serial);

  pinMode(LED_BUILTIN, OUTPUT);                                                           // set LED pin as output
  digitalWrite(LED_BUILTIN, LOW);                                                         // switch off LED pin

  Serial1.begin(115200);                                                                  // initialize UART pour dialogue avec ESP32
  //while (!Serial1);                                                                     // Nécessaire uniquement en debug, sinon le NANO attendra indéfiniement

  pv_status_t status = pv_audio_rec_init();
  if (status != PV_STATUS_SUCCESS) {
    Serial.print("Echec de l'initialisation audio avec ");
    Serial.println(pv_status_to_string(status));
    Serial1.println("erreur");                                                            // On signale une erreur à l'ESP32, car l'initialisation du NANO a échoué (ne doit jamais arriver)
    while (1);
  }

  status = pv_picovoice_init (
    ACCESS_KEY,
    MEMORY_BUFFER_SIZE,
    memory_buffer,
    sizeof(KEYWORD_ARRAY),
    KEYWORD_ARRAY,
    PORCUPINE_SENSITIVITY,
    wake_word_callback,
    sizeof(CONTEXT_ARRAY),
    CONTEXT_ARRAY,
    RHINO_SENSITIVITY,
    1.0,
    true,
    inference_callback,
    &handle
  );
  if (status != PV_STATUS_SUCCESS) {
    Serial.print("Echec de l'initialisation de Picovoice avec ");
    Serial.println(pv_status_to_string(status));
    Serial1.println("erreur");                                                            // On signale une erreur à l'ESP32, car l'initialisation de la librairie a échouée (ne doit jamais arriver)
    while (1);
  }

  const char *rhino_context = NULL;
  status = pv_picovoice_context_info(handle, &rhino_context);
  if (status != PV_STATUS_SUCCESS) {
    Serial.print("Echec de la récupération des informations contextuelles avec ");
    Serial.println(pv_status_to_string(status));
    Serial1.println("erreur");                                                            // On signale une erreur à l'ESP32, car l'initialisation de "RHINO" de la librairie a échouée (ne doit jamais arriver)
    while (1);
  }
//  Serial.println("Activation: Salut ordinateur");
  Serial.println("Activation: Hé, GEMI");
  Serial.println(rhino_context);
  Serial.println("\nVer. " + String(Version));
}

void loop() {
  const int16_t *buffer = pv_audio_rec_get_new_buffer();
  if (buffer) {
    const pv_status_t status = pv_picovoice_process(handle, buffer);
    if (status != PV_STATUS_SUCCESS) {
      Serial.print("Le processus Picovoice a échoué avec ");
      Serial.println(pv_status_to_string(status));
      Serial1.println("erreur");
//      while(1);
    }
  } else if (Serial.available() > 0) {
    String r;
    while(Serial1.available()) {
      char s = Serial1.read();
      r += s;
    }
    r.replace("\r", "");
    r.replace("\n", "");
    r.toLowerCase();
    Serial.print(F("Recu: "));
    Serial.println(r);
    if (r.indexOf("version") > -1) {
      Serial1.println("Nano 33 BLE Ver. " + String(Version));                               // L'ESP32 peut obtenir le numéro de version du programme
    }
  }
}








/* END */
