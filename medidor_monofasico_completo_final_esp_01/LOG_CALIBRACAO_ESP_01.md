# Log Técnico de Diagnóstico e Calibração — ESP 01

**Unidade:** `esp_01` (`LAB_MED_ELET_01`, Canal `1`, IP AP `192.168.4.1`)  
**Pasta:** `/home/wilson/projetos_esp32/medidor_monofasico_inteligente/medidor_monofasico_completo_final_esp_01`  
**Hardware:** ESP32-D0WD rev v1.1 (MAC `14:33:5c:fe:2d:18`, porta `/dev/ttyUSB0`)  
**Início da Sessão:** 2026-11-16  

---

## 1. Segurança e Controle de Versão (Git)

* **Estado inicial do repositório:** branch `main` limpa no commit `e471481` (*Add Documentos_do_Projeto*).
* **Tag de restauração criada:** `backup-original-esp01` (apontando para `e471481`).
* **Branch de trabalho criada:** `calibracao-esp01`.
* **Documentação espelhada no Obsidian:** `ESP32 - Medidor Monofasico Inteligente (Calibracao e Analise).md`.

---

## 2. Diagnóstico Inicial do Código Original (Commit `e471481`)

### Constantes originais no `.ino`
* `V_CAL = 229`
* `I_CAL = 1.11`
* `PHASECAL = -4`

### Falhas identificadas na auditoria estática (`medidor_monofasico_completo_final_esp_01.ino` + `EmonLib`)
1. **[Crítica] Incompatibilidade entre `analogReadResolution(12)` e `EmonLib` (`ADC_BITS = 10`):**
   * O `.ino` configurava o ADC do ESP32 para 12 bits (`0–4095`, centro `~2048`), mas `~/Arduino/libraries/EmonLib/EmonLib.h` definia `ADC_BITS = 10` (`ADC_COUNTS = 1024`) para arquiteturas não-ARM.
   * Consequência: `calcVI(100, 2000)` procurava `startV` entre `460` e `563` (causando timeouts de 2000 ms), iniciava `offsetV`/`offsetI` em `512` e dividia ganhos por `1024` em vez de `4096`.
2. **[Crítica] Corte `i < 0.05` zerava a Tensão (`v = 0`):**
   * `if (v < 80.0 || i < 0.05)` zerava `v` junto com `i, p, s, fp`, impedindo leitura/calibração de tensão a vazio.
3. **[Média] `PHASECAL = -4` fora da faixa nominal (`0.0` a `2.0`):**
   * Amplifica ruído amostra-a-amostra do ADC na potência ativa e distorce `phaseShiftedV`.
4. **[Média] Risco de `NaN` no JSON `/data` e no OLED:**
   * Divisão por zero na `EmonLib` (`numberOfSamples == 0` ou `apparentPower == 0`) passava pelo filtro `< 80.0` e quebrava o JSON.
5. **[Média] Botão (`GPIO 27`) sem limpeza de repique pós-soltura:**
   * A interrupção `FALLING` podia rearmar `botaoFlag` após soltar o botão ou após o `delay(1500)` do reset.
6. **[Menor] Desgaste da Flash (NVS) a cada 10s:**
   * `preferences.putDouble("consumo", consumoKWh)` gravava mesmo quando não havia variação de consumo.
7. **[Menor] Redundância na integração de energia e exibição com `p < 0`:**
   * `(p / 1000.0) * intervaloHoras * 1000.0` simplificável para `p * intervaloHoras`.

---

## 3. Diagnóstico Serial do Firmware Original (Antes das Alterações)

* **Captura em `/dev/ttyUSB0` (`115200` baud, 12s):**
  * Boot em `0,81 s` exibindo `=== MEDIDOR MONOFÁSICO ===`, `SSID: LAB_MED_ELET_01 | Canal: 1`, `IP: 192.168.4.1`.
  * Nenhuma saída contínua no `loop()`, pois o código original não possuía telemetria Serial periódica.

---

## 4. Alteração #1 — Correções Estruturais e Telemetria Serial (2026-11-16)

### Arquivos criados/alterados na pasta do `esp_01`
* **Criados:** `EmonLibESP32.h` e `EmonLibESP32.cpp`
  * Classe `EnergyMonitorESP32` com `ADC_BITS = 12` (`ADC_COUNTS = 4096`) para `ESP32`.
  * Proteção contra `numberOfSamples == 0` e `apparentPower <= 0.0` (evita `NaN`).
  * Exposição de métricas diagnósticas: `lastSamples`, `lastCrossings`, `timedOutStart`, `timedOutLoop`.
* **Modificado:** `medidor_monofasico_completo_final_esp_01.ino`
  * Substituído `#include "EmonLib.h"` por `#include "EmonLibESP32.h"` e `EnergyMonitorESP32 emon1;`.
  * Ajustada escala equivalente de 10 bits para 12 bits (`x4`): `V_CAL = 916.0`, `I_CAL = 4.44`, mantido `PHASECAL = -4.0` como ponto de partida.
  * Separado o filtro de ruído: se `v < 80.0` zera tudo; se `i < 0.05` mantém `v` e zera apenas `i, p, s, fp, q`.
  * Adicionada proteção `isfinite(...)` nas leituras vindas da `EmonLibESP32` e da NVS.
  * Adicionada telemetria Serial completa a cada ciclo (`[ESTAB]` / `[MED]`, `V`, `I`, `P`, `S`, `Q`, `FP`, `E`, `calc=...ms`, `N`, `X`, `toS`, `toL`, `ADC_V`, `ADC_I`).
  * Gravação na NVS apenas quando `fabs(consumoKWh - ultimoConsumoSalvo) > 1e-6`.
  * Limpeza de `botaoFlag = false` ao final de `processarBotao()`.

### Compilação e Gravação (#1)
* **Compilação:** `arduino-cli compile --fqbn esp32:esp32:esp32` — **OK** (`1003720 bytes`, 76% da Flash; `50504 bytes`, 15% da RAM).
* **Gravação:** `arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32:UploadSpeed=115200` — **OK**.

---

## 5. Diagnóstico em Execução Após Alteração #1

### Leitura Serial sem rede AC (somente USB)
```text
[ 4.88s] [ESTAB] V=0.00V | I=0.000A | P=0.0W | S=0.0VA | Q=0.0VAr | FP=0.000 | E=0.13964kWh | calc=66ms (N=719,X=100,toS=0,toL=0) | ADC_V=1984 ADC_I=1776
[ 7.02s] [ESTAB] V=0.00V | I=0.000A | P=0.0W | S=0.0VA | Q=0.0VAr | FP=0.000 | E=0.13964kWh | calc=203ms (N=2240,X=100,toS=0,toL=0) | ADC_V=1983 ADC_I=1787
[ 8.85s] [MED]   V=93.01V | I=0.055A | P=1.5W | S=5.1VA | Q=4.9VAr | FP=0.286 | E=0.13964kWh | calc=24ms (N=259,X=100,toS=0,toL=0) | ADC_V=1967 ADC_I=1789
[10.86s] [MED]   V=98.13V | I=0.000A | P=0.0W | S=0.0VA | Q=0.0VAr | FP=0.000 | E=0.13964kWh | calc=27ms (N=291,X=100,toS=0,toL=0) | ADC_V=1967 ADC_I=1766
[12.86s] [MED]   V=95.47V | I=0.000A | P=0.0W | S=0.0VA | Q=0.0VAr | FP=0.000 | E=0.13964kWh | calc=27ms (N=290,X=100,toS=0,toL=0) | ADC_V=1908 ADC_I=1789
```

* **Achado 1 (Timeouts eliminados):** `toS=0, toL=0`.
* **Achado 2 (Leitura fantasma de `~93–98 V` sem rede AC):**
  * Sem sinal 60 Hz na entrada do ZMPT101B, 100 cruzamentos de ruído ocorrem em apenas **`24–27 ms`** (em 60 Hz real seriam `~833 ms`).
  * Além disso, `offsetV` inicia em `2048`, enquanto o repouso real do circuito está em `ADC_V ≈ 1908–1984`, somando erro estático de offset + ruído de ADC (~93–98 V).

### Comparação em Bancada com Medidor-Calibrador (Tensão + Carga Ligadas)
| Grandeza | ESP 01 (medido) | Medidor-Calibrador (ref.) | Ação Planejada |
|---|---:|---:|---|
| Tensão (`V`) | `210 V` | `207 V` | Ajustar `V_CAL`: `916.0 * (207 / 210) ≈ 903.0` |
| Corrente (`I`) | `1.25 A` | `1.25 A` | Manter `I_CAL = 4.44` |
| Potência Aparente (`S`) | `261 VA` | `259 VA` | Corrige automaticamente com `V_CAL = 903.0` (`207 * 1.25 = 258.75 VA`) |
| Potência Ativa (`P`) | `168 W` | `176 W` | Ajustar `PHASECAL` |
| Potência Reativa (`Q`) | `200 VAr` | `190 VAr` | Ajustar `PHASECAL` |
| Fator de Potência (`FP`) | `0.64` | `0.68` | Ajustar `PHASECAL` (corrigir erro de fase de `~3,0°`) |

---

## 6. Alteração #2 — Remoção do Offset Real, Filtro Passa-Baixa Simétrico e Rejeição de Sinal Fantasma

### Alterações no código
* **Atualizados `EmonLibESP32.h` e `EmonLibESP32.cpp`:**
  * Adicionada rotina `initOffsetsIfNeeded()` (~20 ms de amostragem inicial) para medir o ponto médio DC real de cada sensor (em torno de `1825` em `ADC_V` e `1750` em `ADC_I`, em vez de `2048` fixo).
  * Aplicado cálculo pós-janela com remoção exata da média DC (`varV = sumFiltV2/N - meanFV²`, `varI = sumFiltI2/N - meanFI²`, `covP = sumP/N - meanPSV*meanFI`), eliminando erro residual de offset.
  * Aplicado filtro passa-baixa digital **idêntico** nos canais de tensão e corrente (`LPF_ALPHA = 0.10`, $f_c \approx 195\text{ Hz}$ a $\sim 11\text{ kHz}$), atenuando em ~9,2x o ruído de `~1,85 kHz` (`~93 V` fantasmas) sem introduzir defasagem relativa entre $V$ e $I$.
  * Adicionada histerese no detector de cruzamento por zero (`CROSS_HYST = 25`), amplitude mínima pico-a-pico (`MIN_PP_AC_V = 60`) e rejeição de frequência inválida (`crossCount >= 40 && dtLoopMs < 350`), zerando `Vrms` quando não há rede AC de 50/60 Hz.
* **Atualizado `medidor_monofasico_completo_final_esp_01.ino`:**
  * Reduzido timeout da janela `calcVI(100, 1100)` de `2000 ms` para `1100 ms`.
  * Ajustados `V_CAL = 950.0` e `I_CAL = 4.67` (compensando o ganho `0,9508` do LPF em 60 Hz) e `PHASECAL = -5.2` para teste inicial.
  * Telemetria Serial ampliada com `noAC`, `ppV`, `ppI`, `offV` e `offI`.

### Resultado em bancada energizada após Alteração #2
```text
[ 9.66s] [MED] V=193.79V | I=1.251A | P=177.0W | S=242.5VA | Q=165.8VAr | FP=0.730 | calc=830ms (N=8967,X=100,noAC=0) | ppV=786 ppI=967 offV=1827 offI=1752
[11.66s] [MED] V=192.14V | I=1.248A | P=174.9W | S=239.9VA | Q=164.2VAr | FP=0.729 | calc=830ms (N=8967,X=100,noAC=0) | ppV=766 ppI=969 offV=1822 offI=1749
[13.67s] [MED] V=192.70V | I=1.249A | P=174.8W | S=240.7VA | Q=165.5VAr | FP=0.726 | calc=835ms (N=9026,X=100,noAC=0) | ppV=775 ppI=961 offV=1825 offI=1751
```
* **Diagnóstico:** `calc = 830–835 ms` exatos para 100 semiciclos em 60 Hz (`noAC = 0`). Com o ruído de `~93 V` removido pelo LPF, a tensão real sem ruído apareceu em `~192,88 V` (provando que os `210 V` anteriores incluíam o ruído quadrático $\sqrt{188^2 + 93^2}$), enquanto `I = 1,249 A` permaneceu exato.

---

## 7. Alteração #3 — Ajuste Fino de `V_CAL` e `PHASECAL`

### Alterações no código (`medidor_monofasico_completo_final_esp_01.ino`)
* **`V_CAL`:** ajustado de `950.0` para **`1019.5`** ($950{,}0 \times \frac{207{,}0}{192{,}88}$).
* **`I_CAL`:** mantido **`4.67`**.
* **`PHASECAL`:** ajustado de `-5.2` (`FP = 0.728`) para **`-3.3`** (interpolação para atingir `FP = 0.68`).
* **Compilação / Gravação:** `arduino-cli compile` (`1004844 bytes`, 76% Flash) + `upload` em `/dev/ttyUSB0` — **OK**.

### Resultado em bancada energizada após Alteração #3
```text
[ 9.69s] [MED] V=209.17V | I=1.252A | P=176.5W | S=261.9VA | Q=193.6VAr | FP=0.674 | E=0.14997kWh | calc=832ms (N=8954,X=100,noAC=0) | ppV=787 ppI=983 offV=1830 offI=1755
[11.69s] [MED] V=206.88V | I=1.254A | P=174.9W | S=259.5VA | Q=191.7VAr | FP=0.674 | E=0.15007kWh | calc=829ms (N=8956,X=100,noAC=0) | ppV=782 ppI=982 offV=1827 offI=1751
[13.70s] [MED] V=207.52V | I=1.252A | P=175.3W | S=259.9VA | Q=191.9VAr | FP=0.674 | E=0.15016kWh | calc=834ms (N=9021,X=100,noAC=0) | ppV=781 ppI=959 offV=1824 offI=1750
```

| Grandeza | Medidor-Calibrador (ref.) | ESP 01 (antes) | ESP 01 (atual) |
|---|---:|---:|---:|
| Tensão (`V`) | `207 V` | `210 V` | `206.88 – 209.17 V` |
| Corrente (`I`) | `1.25 A` | `1.25 A` | `1.252 – 1.254 A` |
| Potência Ativa (`P`) | `176 W` | `168 W` | `174.9 – 176.5 W` |
| Potência Aparente (`S`) | `259 VA` | `261 VA` | `259.5 – 261.9 VA` |
| Potência Reativa (`Q`) | `190 VAr` | `200 VAr` | `191.7 – 193.6 VAr` |
| Fator de Potência (`FP`) | `0.68` | `0.64` | `0.674` |

### Observações para próximo ajuste (se desejar)
* Para levar `FP` de `0.674` exatamente para `0.680` (e `Q` de `~192 VAr` para `190 VAr`), basta ajustar `PHASECAL` de `-3.3` para **`-3.5`**.
* Validar também o comportamento com a tomada AC **desligada** para confirmar `V = 0.00 V` (`noAC = 1`).

