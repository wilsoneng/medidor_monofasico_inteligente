#include "EmonLibESP32.h"

void EnergyMonitorESP32::voltage(unsigned int _inPinV, double _VCAL, double _PHASECAL) {
  inPinV = _inPinV;
  VCAL = _VCAL;
  PHASECAL = _PHASECAL;
  offsetsInitialized = false;
}

void EnergyMonitorESP32::current(unsigned int _inPinI, double _ICAL) {
  inPinI = _inPinI;
  ICAL = _ICAL;
  offsetsInitialized = false;
}

void EnergyMonitorESP32::initOffsetsIfNeeded() {
  if (offsetsInitialized) return;

  // Amostra ~20 ms (~1,2 ciclos de 60 Hz) para obter o ponto medio DC real de cada canal
  unsigned long t0 = millis();
  double sumV = 0.0;
  double sumI = 0.0;
  unsigned long n = 0;
  while (millis() - t0 < 20) {
    sumV += analogRead(inPinV);
    sumI += analogRead(inPinI);
    n++;
  }
  if (n > 0) {
    offsetV = sumV / (double)n;
    offsetI = sumI / (double)n;
  } else {
    offsetV = ADC_COUNTS >> 1;
    offsetI = ADC_COUNTS >> 1;
  }
  offsetsInitialized = true;
}

void EnergyMonitorESP32::calcVI(unsigned int crossings, unsigned int timeout) {
  const int SupplyVoltage = 3300;  // Referencia 3.3V do ESP32
  const int CROSS_HYST = 25;       // Histerese (em contagens 12 bits) para rejeitar ruido no cruzamento por zero
  const int MIN_PP_AC_V = 60;      // Excursionamento pico-a-pico minimo (~43 Vpp) para confirmar rede AC presente
  const double LPF_ALPHA = 0.10;   // Filtro passa-baixa identico em V e I (~195 Hz @ ~11 kHz) para atenuar ruido HF sem defasar V vs I

  initOffsetsIfNeeded();

  unsigned int crossCount = 0;
  unsigned int numberOfSamples = 0;
  timedOutStart = false;
  timedOutLoop = false;
  noAcSignal = false;

  const double baseV = offsetV;
  const double baseI = offsetI;

  // 1) Aguarda a tensao passar proximo do offset DC real (janela curta de ate 25 ms)
  unsigned long start = millis();
  int startV = (int)baseV;
  while (true) {
    startV = analogRead(inPinV);
    if (abs(startV - (int)baseV) <= 150) {
      break;
    }
    if ((millis() - start) >= 25) {
      timedOutStart = true;
      break;
    }
  }

  // 2) Janela principal de amostragem
  double sumRawDV = 0.0, sumRawDI = 0.0;
  double sumFiltV = 0.0, sumFiltV2 = 0.0;
  double sumFiltI = 0.0, sumFiltI2 = 0.0;
  double sumPSV = 0.0, sumP = 0.0;
  double filtV = 0.0, filtI = 0.0, lastFiltV = 0.0;
  double minFV = 4095.0, maxFV = -4095.0;
  double minFI = 4095.0, maxFI = -4095.0;
  bool checkVCross = false;
  bool lastVCross = false;

  start = millis();
  while ((crossCount < crossings) && ((millis() - start) < timeout)) {
    numberOfSamples++;

    int sampleV = analogRead(inPinV);
    int sampleI = analogRead(inPinI);

    double rawDV = (double)sampleV - baseV;
    double rawDI = (double)sampleI - baseI;
    sumRawDV += rawDV;
    sumRawDI += rawDI;

    if (numberOfSamples == 1) {
      filtV = rawDV;
      filtI = rawDI;
      lastFiltV = filtV;
    } else {
      lastFiltV = filtV;
      filtV = filtV + LPF_ALPHA * (rawDV - filtV);
      filtI = filtI + LPF_ALPHA * (rawDI - filtI);
    }

    if (filtV < minFV) minFV = filtV;
    if (filtV > maxFV) maxFV = filtV;
    if (filtI < minFI) minFI = filtI;
    if (filtI > maxFI) maxFI = filtI;

    sumFiltV += filtV;
    sumFiltV2 += filtV * filtV;
    sumFiltI += filtI;
    sumFiltI2 += filtI * filtI;

    double phaseShiftedV = lastFiltV + PHASECAL * (filtV - lastFiltV);
    sumPSV += phaseShiftedV;
    sumP += phaseShiftedV * filtI;

    // Detector de cruzamento por zero sobre o sinal filtrado com histerese
    if (numberOfSamples == 1) {
      checkVCross = (filtV > 0.0);
      lastVCross = checkVCross;
    } else {
      lastVCross = checkVCross;
      if (filtV > (double)CROSS_HYST) checkVCross = true;
      else if (filtV < -(double)CROSS_HYST) checkVCross = false;
      if (lastVCross != checkVCross) crossCount++;
    }
  }
  unsigned long dtLoopMs = millis() - start;

  if (crossCount < crossings) {
    timedOutLoop = true;
  }

  lastSamples = numberOfSamples;
  lastCrossings = crossCount;
  lastPpV = (numberOfSamples > 0) ? (int)(maxFV - minFV) : 0;
  lastPpI = (numberOfSamples > 0) ? (int)(maxFI - minFI) : 0;

  // 3) Calculo pos-janela com remocao exata da media DC da janela (zero offset residual)
  if (numberOfSamples < 2) {
    Vrms = 0.0;
    Irms = 0.0;
    realPower = 0.0;
    apparentPower = 0.0;
    powerFactor = 0.0;
    noAcSignal = true;
  } else {
    double N = (double)numberOfSamples;
    double meanRawDV = sumRawDV / N;
    double meanRawDI = sumRawDI / N;
    double meanFV = sumFiltV / N;
    double meanFI = sumFiltI / N;
    double meanPSV = sumPSV / N;

    // Atualiza offsets reais para o proximo ciclo
    offsetV = baseV + meanRawDV;
    offsetI = baseI + meanRawDI;
    lastOffsetV = offsetV;
    lastOffsetI = offsetI;

    // Rejeicao de sinal fantasma sem rede AC:
    // - poucos cruzamentos na janela; ou
    // - 100 cruzamentos em < 400 ms (frequencia impossivel para rede 50/60 Hz); ou
    // - amplitude pico-a-pico filtrada menor que o limiar minimo
    bool freqInvalida = (crossCount >= 40 && dtLoopMs < 350);
    if ((crossings >= 20 && crossCount < 10) || freqInvalida || (lastPpV < MIN_PP_AC_V)) {
      noAcSignal = true;
      Vrms = 0.0;
      Irms = 0.0;
      realPower = 0.0;
      apparentPower = 0.0;
      powerFactor = 0.0;
      return;
    }

    double varV = (sumFiltV2 / N) - (meanFV * meanFV);
    double varI = (sumFiltI2 / N) - (meanFI * meanFI);
    if (varV < 0.0) varV = 0.0;
    if (varI < 0.0) varI = 0.0;

    double covP = (sumP / N) - (meanPSV * meanFI);

    double V_RATIO = VCAL * ((SupplyVoltage / 1000.0) / ADC_COUNTS);
    Vrms = V_RATIO * sqrt(varV);

    double I_RATIO = ICAL * ((SupplyVoltage / 1000.0) / ADC_COUNTS);
    Irms = I_RATIO * sqrt(varI);

    realPower = V_RATIO * I_RATIO * covP;
    apparentPower = Vrms * Irms;
    powerFactor = (apparentPower > 0.0) ? (realPower / apparentPower) : 0.0;
  }
}

double EnergyMonitorESP32::calcIrms(unsigned int Number_of_Samples) {
  const int SupplyVoltage = 3300;
  if (Number_of_Samples == 0) return 0.0;

  initOffsetsIfNeeded();

  double baseI = offsetI;
  double sumDI = 0.0, sumDI2 = 0.0;
  for (unsigned int n = 0; n < Number_of_Samples; n++) {
    double di = (double)analogRead(inPinI) - baseI;
    sumDI += di;
    sumDI2 += di * di;
  }

  double N = (double)Number_of_Samples;
  double meanDI = sumDI / N;
  offsetI = baseI + meanDI;
  double varI = (sumDI2 / N) - (meanDI * meanDI);
  if (varI < 0.0) varI = 0.0;

  double I_RATIO = ICAL * ((SupplyVoltage / 1000.0) / ADC_COUNTS);
  Irms = I_RATIO * sqrt(varI);
  return Irms;
}
