# Log de Calibração e Alterações — ESP 02

**Projeto:** Medidor Monofásico Inteligente (ESP32)  
**Pasta:** `medidor_monofasico_completo_final_esp_02/`  
**Arquivo principal:** `medidor_monofasico_completo_final_esp_02.ino`  
**Wi-Fi AP:** `LAB_MED_ELET_02` (Canal `3`)

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
- Atualizado `medidor_monofasico_completo_final_esp_02.ino`:
  - Migração para `EnergyMonitorESP32`, medição de frequência 60 Hz com histerese, exibição de tensão mesmo sem carga, gravação condicional em NVS, limpeza do flag de interrupção do botão e telemetria serial.
  - Preservados `ssid = "LAB_MED_ELET_02"` e `wifi_channel = 3`.
- Pré-ajuste proporcional (4x da escala 10 bits original para 12 bits) como ponto de partida antes da calibração em bancada:
  - `V_CAL`: `303` -> `1212.0`
  - `I_CAL`: `1.16` -> `4.64`
  - `PHASECAL`: `-4.0` -> `-4.0`

**Status:** Pronto para ajuste do trimpot (se necessário) e calibração em bancada (`V_CAL`, `I_CAL`, `PHASECAL`).
