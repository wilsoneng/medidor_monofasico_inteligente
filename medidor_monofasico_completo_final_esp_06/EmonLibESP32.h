#ifndef EmonLibESP32_h
#define EmonLibESP32_h

#include "Arduino.h"

// ESP32 utiliza ADC de 12 bits (0-4095) quando analogReadResolution(12) esta ativo.
#if defined(ESP32) || defined(__arm__)
#define ADC_BITS 12
#else
#define ADC_BITS 10
#endif

#define ADC_COUNTS (1 << ADC_BITS)

class EnergyMonitorESP32 {
  public:
    void voltage(unsigned int _inPinV, double _VCAL, double _PHASECAL);
    void current(unsigned int _inPinI, double _ICAL);

    void calcVI(unsigned int crossings, unsigned int timeout);
    double calcIrms(unsigned int NUMBER_OF_SAMPLES);

    double realPower,
      apparentPower,
      powerFactor,
      Vrms,
      Irms;

    // Diagnostico de execucao da ultima janela calcVI
    unsigned int lastSamples = 0;
    unsigned int lastCrossings = 0;
    int lastPpV = 0;
    int lastPpI = 0;
    double lastOffsetV = ADC_COUNTS >> 1;
    double lastOffsetI = ADC_COUNTS >> 1;
    bool timedOutStart = false;
    bool timedOutLoop = false;
    bool noAcSignal = false;

  private:
    void initOffsetsIfNeeded();

    unsigned int inPinV = 34;
    unsigned int inPinI = 32;
    double VCAL = 1.0;
    double ICAL = 1.0;
    double PHASECAL = 1.0;

    bool offsetsInitialized = false;
    double offsetV = ADC_COUNTS >> 1;
    double offsetI = ADC_COUNTS >> 1;
};

#endif
