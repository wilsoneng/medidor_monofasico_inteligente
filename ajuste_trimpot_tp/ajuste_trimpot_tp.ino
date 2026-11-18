#include <Wire.h>
#include <U8g2lib.h>

// ========== PINOS (mesmo hardware do Medidor Monofasico) ==========
#define I2C_SDA 13
#define I2C_SCL 14
#define ZMPT_PIN 34
#define BOTAO_ZERAR 27

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

int recMinPpV = 4095;
int recMaxPpV = 0;

void setup() {
  Serial.begin(115200);
  pinMode(BOTAO_ZERAR, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  u8g2.begin();

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 28, "Ferramenta Trimpot");
  u8g2.drawStr(18, 44, "ZMPT101B (GPIO 34)");
  u8g2.sendBuffer();
  delay(1000);
}

void loop() {
  // Botao fisico (GPIO 27) zera os recordes de minimo e maximo
  if (digitalRead(BOTAO_ZERAR) == LOW) {
    recMinPpV = 4095;
    recMaxPpV = 0;
  }

  // Amostra ~34 ms (~2 ciclos completos de 60 Hz)
  unsigned long t0 = millis();
  int adcMin = 4095;
  int adcMax = 0;
  long soma = 0;
  long n = 0;

  while (millis() - t0 < 34) {
    int val = analogRead(ZMPT_PIN);
    if (val < adcMin) adcMin = val;
    if (val > adcMax) adcMax = val;
    soma += val;
    n++;
  }

  int ppV = (n > 0) ? (adcMax - adcMin) : 0;
  int offV = (n > 0) ? (int)(soma / n) : 0;

  if (ppV < recMinPpV) recMinPpV = ppV;
  if (ppV > recMaxPpV) recMaxPpV = ppV;
  int meioIdeal = (recMaxPpV > recMinPpV) ? ((recMinPpV + recMaxPpV) / 2) : 0;

  // Exibe no Serial
  Serial.printf("ppV=%d | MinADC=%d MaxADC=%d Off=%d | RecMin=%d RecMax=%d Meio=%d\n",
                ppV, adcMin, adcMax, offV, recMinPpV, recMaxPpV, meioIdeal);

  // Exibe no OLED
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Ajuste Trimpot TP");
  u8g2.drawLine(0, 12, 128, 12);

  u8g2.setFont(u8g2_font_ncenB12_tr);
  u8g2.setCursor(0, 30);
  u8g2.print("ppV: ");
  u8g2.print(ppV);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 44);
  u8g2.print("Min:"); u8g2.print(recMinPpV);
  u8g2.setCursor(64, 44);
  u8g2.print("Max:"); u8g2.print(recMaxPpV);

  u8g2.setCursor(0, 56);
  if (adcMin < 100 || adcMax > 3850) {
    u8g2.print("SATURADO! Reduza");
  } else {
    u8g2.print("Meio:"); u8g2.print(meioIdeal);
    u8g2.setCursor(64, 56);
    u8g2.print("Off:"); u8g2.print(offV);
  }

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 64);
  u8g2.print("ADC["); u8g2.print(adcMin); u8g2.print(".."); u8g2.print(adcMax); u8g2.print("]");
  u8g2.sendBuffer();

  delay(150);
}
