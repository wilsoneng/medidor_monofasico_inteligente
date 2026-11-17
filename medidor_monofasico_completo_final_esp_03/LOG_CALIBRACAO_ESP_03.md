# Log Técnico de Diagnóstico e Calibração — ESP 03

**Unidade:** `esp_03` (`LAB_MED_ELET_03`, Canal `6`, IP AP `192.168.4.1`)  
**Pasta:** `/home/wilson/projetos_esp32/medidor_monofasico_inteligente/medidor_monofasico_completo_final_esp_03`  
**Hardware:** ESP32-D0WD rev v1.1 (MAC `14:33:5c:71:5a:e0`, porta `/dev/ttyUSB0`)  
**Início da Sessão:** 2026-11-17  

---

## 1. Controle de Versão e Backup (Git)

* **Tag de restauração criada:** `backup-original-esp03` (apontando para `main` `2603a61`).
* **Branch de trabalho:** `calibracao-esp03`.

---

## 2. Estado Inicial e Comparação Prévia em Bancada (Firmware Legado 10 bits)

* **Constantes originais em `medidor_monofasico_completo_final_esp_03.ino`:**
  * `V_CAL = 244`, `I_CAL = 1.32`, `PHASECAL = -3.5`
* **Leitura prévia comparada ao medidor-calibrador:**

| Grandeza | ESP 03 (legado) | Medidor-Calibrador (ref.) | Razão / Observação |
|---|---:|---:|---|
| Tensão (`V`) | `108 V` | `213 V` | Fator `213 / 108 ≈ 1,972x` |
| Corrente (`I`) | `1,50 A` | `1,28 A` | Fator `1,28 / 1,50 ≈ 0,853x` |
| Potência Ativa (`P`) | `80 W` | `188 W` | A recalibrar via `V_CAL`, `I_CAL` e `PHASECAL` |
| Potência Aparente (`S`) | `164 VA` | `275 VA` | $213 \times 1{,}28 = 272{,}6\text{ VA}$ |
| Potência Reativa (`Q`) | `140 VAr` | `198 VAr` | A recalibrar via `PHASECAL` |
| Fator de Potência (`FP`) | `0,48` | `0,69` | A ajustar via `PHASECAL` após trava em 60 Hz |

---

## 3. Alteração #1 — Migração para `EmonLibESP32` (12 bits + LPF) e Estimativa Inicial

* **Arquivos adicionados:** `EmonLibESP32.h`, `EmonLibESP32.cpp` (idênticos aos validados no `esp_01`).
* **Alterações em `medidor_monofasico_completo_final_esp_03.ino`:**
  * Mantidos `ssid = "LAB_MED_ELET_03"` e `wifi_channel = 6`.
  * Aplicadas melhorias do `esp_01` (ADC 12 bits nativo, pré-calibração de offset DC, filtro LPF $\alpha = 0{,}10$, validação AC, proteção `NaN`, gravação condicional em NVS, limpeza de `botaoFlag` e telemetria Serial).
  * Estimativa inicial (escala 12 bits + compensação `0,9508` do LPF $\times$ razão do calibrador):
    * `V_CAL = 2025.0` ($244 \times 4{,}207 \times \frac{213}{108}$)
    * `I_CAL = 4.74` ($1{,}32 \times 4{,}207 \times \frac{1{,}28}{1{,}50}$)
    * `PHASECAL = -3.5`
* **Compilação / Gravação:** `arduino-cli compile` (`1004840 bytes`, 76% Flash) + `upload` em `/dev/ttyUSB0` — **OK**.

### Leitura Serial inicial (sem rede AC / carga ligada)
```text
[ 0.80s] === MEDIDOR MONOFÁSICO ===
[ 0.80s] SSID: LAB_MED_ELET_03 | Canal: 6
[ 0.80s] IP: 192.168.4.1
[ 9.33s] [MED] V=0.00V | I=0.000A | P=0.0W | S=0.0VA | Q=0.0VAr | FP=0.000 | E=0.40689kWh | calc=507ms (N=5571,X=100,noAC=0) | ppV=103 ppI=22 offV=1872 offI=1803
[11.39s] [MED] V=0.00V | I=0.000A | P=0.0W | S=0.0VA | Q=0.0VAr | FP=0.000 | E=0.40689kWh | calc=566ms (N=6221,X=100,noAC=0) | ppV=109 ppI=27 offV=1872 offI=1803
[13.34s] [MED] V=0.00V | I=0.000A | P=0.0W | S=0.0VA | Q=0.0VAr | FP=0.000 | E=0.40689kWh | calc=518ms (N=5703,X=100,noAC=0) | ppV=104 ppI=26 offV=1873 offI=1802
```
* **Status:** Sem tensão na entrada, `V=0.00V` e `I=0.000A` estáveis.

---

## 4. Captura em Bancada Energizada (com `V_CAL=2025.0`, `I_CAL=4.74`, `PHASECAL=-3.5`)

```text
[ 9.65s] [MED] V=198.13V | I=1.286A | P=133.0W | S=254.8VA | Q=217.4VAr | FP=0.522 | calc=829ms (N=9125,X=100,noAC=0) | ppV=426 ppI=977
[11.65s] [MED] V=198.14V | I=1.286A | P=133.4W | S=254.9VA | Q=217.2VAr | FP=0.524 | calc=828ms (N=9119,X=100,noAC=0) | ppV=424 ppI=975
[13.66s] [MED] V=197.51V | I=1.285A | P=133.7W | S=253.7VA | Q=215.7VAr | FP=0.527 | calc=833ms (N=9176,X=100,noAC=0) | ppV=411 ppI=973
```
* **Referência do medidor-calibrador no mesmo instante:** `213 V`, `1,28 A`, `190 W`, `199 VAr`, `275 VA`, `0,69 fp`.

---

## 5. Alteração #2 — Calibração Fina (`V_CAL`, `I_CAL`, `PHASECAL`) e Atraso em Buffer Circular

* **Atualização em `EmonLibESP32.cpp` (`esp_03`):**
  * Adicionado atraso fracionário exato via buffer circular (`vRing[32]`) para `PHASECAL <= 1.0`, mantendo ganho unitário (`|H| = 1,0`) mesmo com defasagens maiores (~10,6 amostras) sem amplificar ruído ou inflar `realPower`.
* **Atualização em `medidor_monofasico_completo_final_esp_03.ino`:**
  * `V_CAL = 2179.0` ($2025{,}0 \times \frac{213}{197{,}93}$)
  * `I_CAL = 4.72` ($4{,}74 \times \frac{1{,}28}{1{,}286}$)
  * `PHASECAL = -9.6` (atraso de `10,6` amostras $\approx 20{,}9^\circ$ para levar `FP` de `0,524` para `~0,69–0,70`)
* **Compilação / Gravação:** `arduino-cli compile` (`1005140 bytes`, 76% Flash) + `upload` em `/dev/ttyUSB0` — **OK**.

### Leituras em Bancada Energizada após Alteração #2
```text
[13.66s] [MED] V=212.30V | I=1.227A | P=183.5W | S=260.4VA | Q=184.8VAr | FP=0.705 | calc=830ms (N=9003,X=100,noAC=0) | ppV=423 ppI=936
[13.65s] [MED] V=210.51V | I=1.228A | P=181.6W | S=258.5VA | Q=184.0VAr | FP=0.702 | calc=828ms (N=8983,X=100,noAC=0) | ppV=415 ppI=937
```
* **Obs.:** Entre o ensaio anterior e este, o `ppI` da carga caiu de `~975` para `~936` (`-4%` de corrente da própria carga), mantendo `V` em `210,5–212,3 V` e `FP` em `0,702–0,705` (muito próximo dos `213 V` e `0,69 fp` de referência).

---

## 6. Alteração #3 — Compensação para Fonte Externa 5 V + Ajuste Fino de `I_CAL` e `PHASECAL`

* **Observação física em bancada:** Ao alimentar o conjunto com a fonte externa definitiva de 5 V (em vez da USB do notebook), a leitura de tensão subiu ~`+4 a +5 V` devido à diferença de tensão/regulação/ripple entre a porta USB e a fonte 5 V (`214 V` no ESP 03 vs. `210 V` no calibrador; `1,22 A` vs. `1,23 A`).
* **Ajuste final em `medidor_monofasico_completo_final_esp_03.ino`:**
  * `V_CAL = 2138.0` ($2179{,}0 \times \frac{210}{214}$)
  * `I_CAL = 4.76` ($4{,}72 \times \frac{1{,}23}{1{,}22}$)
  * `PHASECAL = -9.0` (teste intermediário: tensão e corrente exatas na fonte 5 V; `FP` leu `0,68` vs. `0,71` no calibrador).

---

## 7. Alteração #4 — Ajuste Fino Final de `PHASECAL` (`0,68` $\rightarrow$ `0,71 fp`)

* **Cálculo:** Com `PHASECAL = -9.0`, `FP = 0,68` ($\theta = 47{,}16^\circ$); com `-9.6`, havia marcado `~0,703` ($\theta = 45{,}33^\circ$). Para atingir `0,710` ($\theta = 44{,}77^\circ$), ajustado `PHASECAL` para **`-9.9`**.
* **Constantes finais gravadas:** `V_CAL = 2138.0`, `I_CAL = 4.76`, `PHASECAL = -9.9`.
* **Validação final:** Aprovado pelo usuário em bancada com a fonte definitiva de 5 V junto ao medidor-calibrador (`esp_03` finalizado e integrado à `main`).


