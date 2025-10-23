#include <IRremote.h>
#include <EEPROM.h>

// Pines
#define RECV_PIN 2
#define SEND_PIN 3

// EEPROM
#define NUM_SLOTS 16
#define SLOT_SIZE 4  // 4 bytes por código NEC
#define EEPROM_MAGIC_ADDR (EEPROM.length() - 1)
#define EEPROM_MAGIC_VALUE 0x42

// Códigos del control "MASTER"
#define MASTER_UP     0xB946FF00
#define MASTER_DOWN   0xEA15FF00
#define MASTER_LEFT   0xBB44FF00
#define MASTER_RIGHT  0xBC43FF00
#define MASTER_OK     0xBF40FF00
#define MASTER_1      0xE916FF00
#define MASTER_2      0xE619FF00
#define MASTER_3      0xF20DFF00
#define MASTER_4      0xF30CFF00
#define MASTER_5      0xE718FF00
#define MASTER_6      0xA15EFF00
#define MASTER_7      0xF708FF00
#define MASTER_8      0xE31CFF00
#define MASTER_9      0xA55AFF00
#define MASTER_0      0xAD52FF00
#define MASTER_STAR   0xBD42FF00  // Activar programación
#define MASTER_HASH   0xB54AFF00  // Borrar EEPROM

bool programmingMode = false;
uint8_t targetButtonIndex = 255;

int getMasterKeyIndex(uint32_t code) {
  switch (code) {
    case MASTER_UP: return 0;
    case MASTER_DOWN: return 1;
    case MASTER_LEFT: return 2;
    case MASTER_RIGHT: return 3;
    case MASTER_OK: return 4;
    case MASTER_1: return 5;
    case MASTER_2: return 6;
    case MASTER_3: return 7;
    case MASTER_4: return 8;
    case MASTER_5: return 9;
    case MASTER_6: return 10;
    case MASTER_7: return 11;
    case MASTER_8: return 12;
    case MASTER_9: return 13;
    case MASTER_0: return 14;
    case MASTER_HASH: return 15;
    default: return -1;
  }
}

void saveNECToEEPROM(int index, uint32_t code) {
  int addr = index * SLOT_SIZE;
  EEPROM.put(addr, code);
  Serial.print(F("✅ Código NEC guardado en slot "));
  Serial.println(index);
}

uint32_t readNECFromEEPROM(int index) {
  int addr = index * SLOT_SIZE;
  uint32_t code;
  EEPROM.get(addr, code);
  return code;
}

void clearAllSlots() {
  for (int i = 0; i < NUM_SLOTS * SLOT_SIZE; i++) {
    EEPROM.update(i, 0xFF);
  }
}


uint32_t reverseNECBytes(uint32_t code) {
    uint32_t res = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t byte = (code >> (8 * i)) & 0xFF;
        // invertir bits de byte
        byte = (byte * 0x0202020202ULL & 0x010884422010ULL) % 1023;
        // poner byte invertido en posición invertida (byteswap)
        res |= (uint32_t)byte << (8 * (3 - i));
    }
    return res;
}


void sendNECFromSlot(int index) {
  
  if (index < 0 || index >= NUM_SLOTS) {
    Serial.println(F("❌ Índice fuera de rango (0-15)."));
    return;
  }

  uint32_t code = readNECFromEEPROM(index);
  uint32_t fixedCode = reverseNECBytes(code);  // <- Inversión de bytes
  if (code != 0xFFFFFFFF && code != 0x00000000) {
    Serial.print(F("📤 Enviando código del slot "));
    Serial.print(index);
    Serial.print(F(": 0x"));
    Serial.println(code, HEX);
    //IrSender.sendNEC(code, 32);
    IrSender.sendNEC(fixedCode, 32);
    IrReceiver.resume();
  } else {
    Serial.println(F("⚠ Slot vacío."));
  }
  
}

void handleSerialCommand() {
  static String input = "";

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      input.trim();
      if (input.length() > 0) {
        Serial.print(F("📥 Comando recibido: "));
        Serial.println(input);

        if (input.startsWith("B")) {
          int eqIndex = input.indexOf('=');
          if (eqIndex > 1) {
            // Ej: B5=0xE916FF00
            int slot = input.substring(1, eqIndex).toInt();
            String codeStr = input.substring(eqIndex + 1);
            uint32_t code = strtoul(codeStr.c_str(), NULL, 0);
            if (slot >= 0 && slot < NUM_SLOTS) {
              saveNECToEEPROM(slot, code);
            } else {
              Serial.println(F("❌ Índice inválido. Debe ser 0 a 15."));
            }
          } else {
            // Ej: B5
            int index = input.substring(1).toInt();
            
            sendNECFromSlot(index);
          }
        } else if (input.equalsIgnoreCase("CLEAR")) {
          clearAllSlots();
          EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
          Serial.println(F("✅ EEPROM limpiada."));
        } else if (input.equalsIgnoreCase("LIST")) {
          Serial.println(F("📋 Códigos almacenados:"));
          for (int i = 0; i < NUM_SLOTS; i++) {
            uint32_t code = readNECFromEEPROM(i);
            if (code != 0xFFFFFFFF && code != 0x00000000) {
              Serial.print(F("  • Slot "));
              Serial.print(i);
              Serial.print(F(": 0x"));
              Serial.println(code, HEX);
            }
          }
        } else if (input.equalsIgnoreCase("HELP")) {
          Serial.println(F("📖 Comandos disponibles:"));
          Serial.println(F("  Bn           -> Enviar código almacenado en slot n (0-15)"));
          Serial.println(F("  Bn=0xXXXXXX  -> Guardar código NEC en slot n"));
          Serial.println(F("  LIST         -> Mostrar códigos almacenados"));
          Serial.println(F("  CLEAR        -> Borrar todos los códigos"));
          Serial.println(F("  HELP         -> Mostrar este menú"));
        } else {
          Serial.println(F("❓ Comando no reconocido. Escribe HELP para ver opciones."));
        }
      }
      input = "";
    } else {
      input += c;
    }
  }
}

void setup() {
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  Serial.begin(115200);
  IrReceiver.begin(RECV_PIN, DISABLE_LED_FEEDBACK);
  IrSender.begin(SEND_PIN);

  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    Serial.println(F("🔄 EEPROM sin inicializar. Borrando..."));
    clearAllSlots();
    EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
  }

  Serial.println(F("✅ Clonador IR (NEC) listo"));
  Serial.println(F("Presiona '*' para entrar en modo programación"));
  Serial.println(F("Presiona '#' para borrar todos los códigos"));
  Serial.println(F("Escribe HELP por comandos seriales"));
}

void loop() {
  handleSerialCommand();

  if (IrReceiver.decode()) {
    uint32_t receivedCode = IrReceiver.decodedIRData.decodedRawData;
    decode_type_t protocol = IrReceiver.decodedIRData.protocol;

    if (receivedCode == 0 || protocol != NEC) {
      IrReceiver.resume();
      return;
    }

    Serial.print(F("🔹 NEC recibido: 0x"));
    Serial.println(receivedCode, HEX);

    Serial.print(F("🔢 Longitud en bits: "));
    Serial.println(IrReceiver.decodedIRData.numberOfBits);

    Serial.print(F("📡 Protocolo: "));
    Serial.println(IrReceiver.getProtocolString());

    if (programmingMode) {
      if (targetButtonIndex == 255) {
        targetButtonIndex = getMasterKeyIndex(receivedCode);
        if (targetButtonIndex == -1) {
          Serial.println(F("❌ Tecla MASTER inválida."));
          programmingMode = false;
        } else {
          Serial.print(F("🛠️ Slot seleccionado: "));
          Serial.println(targetButtonIndex);
          Serial.println(F("👉 Presiona tecla del control a clonar (NEC)..."));
        }
      } else {
        saveNECToEEPROM(targetButtonIndex, receivedCode);
        programmingMode = false;
        targetButtonIndex = 255;
        Serial.println(F("✅ Código NEC guardado."));
      }
    } else {
      if (receivedCode == MASTER_STAR) {
        programmingMode = true;
        targetButtonIndex = 255;
        Serial.println(F("🔧 Modo programación activado."));
      } 
      /*
      else if (receivedCode == MASTER_HASH) {
        Serial.println(F("⚠ Borrando EEPROM..."));
        clearAllSlots();
        EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
        Serial.println(F("✅ EEPROM limpia."));
      } 
      */
      else {
        int index = getMasterKeyIndex(receivedCode);
        if (index >= 0) {
          sendNECFromSlot(index);
        } else {
          Serial.println(F("🔸 Tecla no reconocida como MASTER."));
        }
      }
    }

    IrReceiver.resume();
  }
}
