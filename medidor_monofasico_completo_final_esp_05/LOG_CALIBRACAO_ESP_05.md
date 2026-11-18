# Log de Calibração e Alterações — ESP 05

**Projeto:** Medidor Monofásico Inteligente (ESP32)  
**Pasta:** `medidor_monofasico_completo_final_esp_05/`  
**Arquivo principal:** `medidor_monofasico_completo_final_esp_05.ino`  
**Wi-Fi AP:** `LAB_MED_ELET_05` (Canal `11`)

---

## Alteração #1 — Migração para `EmonLibESP32` (12 bits) e correções estruturais

**Data:** 2026-11-17  
**Motivo:** Atualizar o firmware legado (10 bits `EmonLib`) com todas as correções validadas nos ESPs 03 e 01, deixando a unidade pronta para calibração em bancada.

### O que foi aplicado
- Adicionados os arquivos locais `EmonLibESP32.h` e `EmonLibESP32.cpp`:
  - Resolução nativa de 12 bits no ADC do ESP32 (`0–4095`).
  - Pré-calibração de offset DC por 20 ms na inicialização.
  - Filtro passa-baixas simétrico (`LPF_ALPHA = 0.10`) em tensão e corrente.
  - Buffer circular `vRing[32]` para atraso fracionário estável mesmo com `PHASECAL <= 1.0`.
  - Proteção contra `NaN` e divisão por zero.
- Atualizado `medidor_monofasico_completo_final_esp_05.ino`:
  - Migração para `EnergyMonitorESP32`, medição de frequência 60 Hz com histerese, exibição de tensão mesmo sem carga, gravação condicional em NVS, limpeza do flag de interrupção do botão e telemetria serial.
  - Preservados `ssid = "LAB_MED_ELET_05"` e `wifi_channel = 11`.
- Pré-ajuste proporcional (4x da escala 10 bits original para 12 bits) como ponto de partida antes da calibração em bancada:
  - `V_CAL`: `236` -> `944.0`
  - `I_CAL`: `1.11` -> `4.44`
  - `PHASECAL`: `-5.4` -> `-5.4`

**Status:** Pronto para ajuste do trimpot (se necessário) e calibração em bancada (`V_CAL`, `I_CAL`, `PHASECAL`).

---

## Alteração #2 — Calibração final em bancada aprovada

**Data:** 2026-11-18  
**Trimpot ZMPT101B:** Ajustado no ponto médio (`~1515`, faixa `645–2386`).  
**Resultado:** Leituras aprovadas em bancada (`V_CAL`, `I_CAL` e `PHASECAL` finais salvos em `medidor_monofasico_completo_final_esp_05.ino`).
