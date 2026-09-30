#include <Arduino.h>
#include <DHT.h>

namespace {
constexpr uint8_t DHT_PIN = 32;
constexpr uint8_t RELAY_PIN = 33;
constexpr float RELAY_TEMPERATURE_C = 33.0f;
constexpr uint32_t SENSOR_INTERVAL_MS = 2000;
constexpr uint8_t DHT_TYPE = DHT22;

// Cada arreglo corresponde a un display y enumera los pines en orden A-G.
// Segmentos en orden A, B, C, D, E, F, G. En cátodo común, HIGH enciende.
constexpr uint8_t TENS_SEGMENTS[] = {23, 22, 21, 19, 18, 5, 17};
constexpr uint8_t UNITS_SEGMENTS[] = {16, 4, 14, 2, 15, 13, 12};

// Para cada número, 1 enciende el segmento correspondiente y 0 lo apaga.
constexpr uint8_t DIGIT_PATTERNS[10][7] = {
    {1, 1, 1, 1, 1, 1, 0},
    {0, 1, 1, 0, 0, 0, 0},
    {1, 1, 0, 1, 1, 0, 1},
    {1, 1, 1, 1, 0, 0, 1},
    {0, 1, 1, 0, 0, 1, 1},
    {1, 0, 1, 1, 0, 1, 1},
    {1, 0, 1, 1, 1, 1, 1},
    {1, 1, 1, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 0, 1, 1}
};

// Encapsula el DHT22 para que el resto del programa no dependa de su librería.
class Dht22Sensor {
 public:
  Dht22Sensor(uint8_t pin) : sensor_(pin, DHT_TYPE) {}

  void begin() {
    sensor_.begin();
  }

  float readTemperatureC() {
    // Si la lectura falla, la librería devuelve NaN (valor no numérico).
    return sensor_.readTemperature();
  }
 
 private:
  DHT sensor_;
};

// Controla un solo display de 7 segmentos usando los pines que recibe.
class SevenSegmentDisplay {
 public:
  SevenSegmentDisplay(const uint8_t* segmentPins) : segmentPins_(segmentPins) {}

  void begin() {
    for (uint8_t segment = 0; segment < 7; ++segment) {
      pinMode(segmentPins_[segment], OUTPUT);
    }
    clear();
  }

  void showDigit(uint8_t digit) {
    for (uint8_t segment = 0; segment < 7; ++segment) {
        // En Ánodo Común: LOW enciende, HIGH apaga
        digitalWrite(segmentPins_[segment], DIGIT_PATTERNS[digit][segment] == 1 ? LOW : HIGH);
    }
}

void clear() {
    for (uint8_t segment = 0; segment < 7; ++segment) {
        digitalWrite(segmentPins_[segment], HIGH); // HIGH apaga en Ánodo Común
    }
}

  private:
  const uint8_t* segmentPins_;
};

// Combina los displays de decenas y unidades para mostrar 00-99 grados.
class TemperatureDisplay {
 public:
  TemperatureDisplay()
      : tensDisplay_(TENS_SEGMENTS), unitsDisplay_(UNITS_SEGMENTS) {}

  void begin() {
    tensDisplay_.begin();
    unitsDisplay_.begin();
  }

  void showTemperature(float temperatureC) {
    // Se muestra la parte entera; se limita el resultado al rango de dos dígitos.
    const int temperature = constrain(static_cast<int>(temperatureC), 0, 99);
    tensDisplay_.showDigit(temperature / 10);
    unitsDisplay_.showDigit(temperature % 10);
  }

  void clear() {
    tensDisplay_.clear();
    unitsDisplay_.clear();
  }

 private:
  SevenSegmentDisplay tensDisplay_;
  SevenSegmentDisplay unitsDisplay_;
};

// Decide cuándo activar el relé comparando la temperatura con el umbral.
class TemperatureRelay {
 public:
  TemperatureRelay(uint8_t pin, float thresholdC)
      : pin_(pin), thresholdC_(thresholdC) {}

  void begin() {
    pinMode(pin_, OUTPUT);
    setActive(false);
  }

  bool update(float temperatureC) {
    // El relé se activa al alcanzar o superar el umbral.
    const bool shouldBeActive = temperatureC >= thresholdC_;
    setActive(shouldBeActive);
    return shouldBeActive;
  }

  void setActive(bool active) {
    digitalWrite(pin_, active ? HIGH : LOW);
  }

 private:
  uint8_t pin_;
  float thresholdC_;
};

// Coordina sensor, displays y relé: es la clase que usa el programa principal.
class TemperatureMonitor {
 public:
  TemperatureMonitor()
      : sensor_(DHT_PIN),
        display_(),
        relay_(RELAY_PIN, RELAY_TEMPERATURE_C),
        lastReadMs_(0) {}

  void begin() {
    Serial.begin(115200);
    sensor_.begin();
    display_.begin();
    relay_.begin();
  }

  void update() {
    const uint32_t now = millis();
    // El DHT22 necesita tiempo entre lecturas; millis evita detener el programa.
    if (now - lastReadMs_ < SENSOR_INTERVAL_MS) {
      return;
    }
    lastReadMs_ = now;

    const float temperatureC = sensor_.readTemperatureC();
    if (isnan(temperatureC)) {
      // Ante un error, se deja el sistema en un estado seguro: relé apagado.
      handleSensorError();
      return;
    }

    display_.showTemperature(temperatureC);
    const bool relayIsActive = relay_.update(temperatureC);
    printStatus(temperatureC, relayIsActive);
  }

 private:
  void handleSensorError() {
    display_.clear();
    relay_.setActive(false);
    Serial.println("Error al leer el DHT22; rele apagado.");
  }

  void printStatus(float temperatureC, bool relayIsActive) {
    Serial.print("Temperatura: ");
    Serial.print(temperatureC, 1);
    Serial.print(" C | Rele: ");
    Serial.println(relayIsActive ? "ACTIVADO" : "APAGADO");
  }

  Dht22Sensor sensor_;
  TemperatureDisplay display_;
  TemperatureRelay relay_;
  uint32_t lastReadMs_;
};

TemperatureMonitor monitor;
}  // namespace

void setup() {
  monitor.begin();
}

void loop() {
  monitor.update();
}

