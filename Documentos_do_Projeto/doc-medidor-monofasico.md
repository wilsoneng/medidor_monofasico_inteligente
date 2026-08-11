# Documentação - Medidor Monofásico com ESP32

## 📋 Índice
1. [Visão Geral do Projeto](#visão-geral-do-projeto)
2. [Componentes Utilizados](#componentes-utilizados)
3. [Esquema de Ligações](#esquema-de-ligações)
4. [Funcionamento do Sistema](#funcionamento-do-sistema)
5. [Interface com o Usuário](#interface-com-o-usuário)
6. [Configuração Wi-Fi](#configuração-wi-fi)
7. [Calibração e Ajustes](#calibração-e-ajustes)
8. [Guia de Uso](#guia-de-uso)
9. [Troubleshooting](#troubleshooting)

---

## 🎯 Visão Geral do Projeto

### O que é?
Este projeto implementa um **medidor de energia elétrica monofásico inteligente** usando ESP32, capaz de medir e monitorar em tempo real o consumo de energia elétrica em instalações residenciais ou comerciais.

### Funcionalidades Principais
- ✅ Medição de tensão, corrente e potência em tempo real
- ✅ Cálculo automático de consumo de energia (kWh)
- ✅ Display OLED para visualização local
- ✅ Interface web acessível via smartphone/computador
- ✅ Armazenamento persistente do consumo
- ✅ Indicador LED pulsante (simula medidor convencional)
- ✅ Sistema de reset de consumo por botão

### Aplicações Práticas
- Monitoramento de consumo residencial
- Controle de gastos de energia
- Identificação de equipamentos com alto consumo
- Projetos de eficiência energética
- Estudos e pesquisas em engenharia elétrica

---

## 🔧 Componentes Utilizados

### Hardware Principal

| Componente | Função | Quantidade |
|------------|--------|------------|
| **ESP32 DevKit** | Microcontrolador principal | 1 |
| **Sensor ZMPT101B** | Medição de tensão AC (127V/220V) | 1 |
| **Sensor SCT-013** | Medição de corrente AC | 1 |
| **Display OLED 128x64** | Visualização local dos dados | 1 |
| **Botão Push Button** | Navegação entre telas e reset | 1 |
| **LED Builtin** | Indicador de pulso de energia | Interno |

### Bibliotecas Necessárias

```cpp
#include "EmonLib.h"              // Medição de energia
#include <Preferences.h>          // Armazenamento na flash
#include <Wire.h>                 // Comunicação I2C
#include <Adafruit_GFX.h>         // Gráficos para display
#include <Adafruit_SSD1306.h>     // Driver do display OLED
#include <WiFi.h>                 // Conectividade Wi-Fi
#include <WebServer.h>            // Servidor web
```

---

## 🔌 Esquema de Ligações

### Pinout do ESP32

```
ESP32 Pin      →  Componente
─────────────────────────────────
GPIO 13 (SDA)  →  Display OLED (SDA)
GPIO 14 (SCL)  →  Display OLED (SCL)
GPIO 27        →  Botão Externo (com pull-up interno)
GPIO 32        →  Sensor SCT-013 (corrente)
GPIO 34        →  Sensor ZMPT101B (tensão)
GPIO 2         →  LED Builtin (interno)
GND            →  GND comum de todos os componentes
3.3V           →  VCC do Display OLED
```

### Diagrama Simplificado

```
     [REDE ELÉTRICA]
           |
    [ZMPT101B]─────→ GPIO 34 (Tensão)
           |
    [SCT-013]──────→ GPIO 32 (Corrente)
           |
      [ESP32]
        |  |
        |  └─→ GPIO 13/14 → [Display OLED]
        |
        └─────→ GPIO 27 → [Botão]
```

---

## ⚙️ Funcionamento do Sistema

### 1. Medição Elétrica

#### Como funciona?
O sistema utiliza dois sensores para realizar medições não-invasivas:

**Sensor ZMPT101B (Tensão)**
- Mede a tensão RMS da rede elétrica
- Faixa: 0-250V AC
- Saída analógica para o ESP32

**Sensor SCT-013 (Corrente)**
- Transformador de corrente tipo "alicate"
- Mede corrente sem cortar fios
- Faixa típica: 0-100A

#### Grandezas Calculadas

| Grandeza | Símbolo | Unidade | Descrição |
|----------|---------|---------|-----------|
| Tensão RMS | V | Volts (V) | Tensão eficaz da rede |
| Corrente RMS | I | Amperes (A) | Corrente eficaz consumida |
| Potência Ativa | P | Watts (W) | Potência real consumida |
| Potência Reativa | Q | VAR | Potência que não realiza trabalho |
| Potência Aparente | S | VA | Potência total fornecida |
| Fator de Potência | FP | - | Eficiência do uso da energia (0-1) |
| Energia | E | kWh | Consumo acumulado |

#### Fórmulas Utilizadas

```
Potência Ativa (P) = V × I × FP
Potência Aparente (S) = V × I
Potência Reativa (Q) = √(S² - P²)
Energia (kWh) = (P / 1000) × Tempo (horas)
```

### 2. Cálculo de Consumo

O sistema calcula o consumo de energia de forma contínua:

1. **Medição a cada 2 segundos**
   - Lê tensão e corrente
   - Calcula potências instantâneas

2. **Integração temporal**
   - Calcula energia consumida no intervalo
   - Fórmula: `Energia (Wh) = Potência (W) × Tempo (h)`

3. **Acumulação**
   - Soma energia ao total acumulado
   - Converte para kWh (divide por 1000)

**Exemplo prático:**
```
Se um equipamento consome 1000W (1kW) durante 1 hora:
Energia = 1kW × 1h = 1 kWh
```

### 3. Sistema de Pulsos LED

O medidor simula o comportamento de medidores convencionais através de pulsos luminosos:

- **1600 pulsos = 1 kWh** (padrão dos medidores residenciais)
- **1 pulso = 0,625 Wh**

**Como funciona:**
```cpp
// A cada 0.625 Wh consumidos, o LED pisca
energiaAcumuladaParaPulso += energiaWh;
if (energiaAcumuladaParaPulso >= 0.625) {
    digitalWrite(LED_BUILTIN, HIGH);  // Pisca LED
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
}
```

### 4. Armazenamento de Dados

**Memória Flash (não-volátil)**
- O consumo total é salvo automaticamente a cada 10 segundos
- Os dados permanecem salvos mesmo após desligar o ESP32
- Utiliza a biblioteca `Preferences` para acesso à flash

```cpp
// Salvar consumo
preferences.putDouble("consumo", consumoKWh);

// Recuperar consumo ao ligar
consumoKWh = preferences.getDouble("consumo", 0.0);
```

---

## 👤 Interface com o Usuário

### 1. Display OLED (128x64 pixels)

O display possui **5 telas diferentes** que podem ser navegadas pressionando o botão:

#### 🖥️ Tela 0 - Tensão e Corrente
```
┌─────────────────────────┐
│ Medidor Monofasico      │
├─────────────────────────┤
│ 220.5 V                 │
│                         │
│ 5.32 A                  │
│                         │
│ 0.12345 kWh             │
└─────────────────────────┘
```

#### 🖥️ Tela 1 - Potências W e VA
```
┌─────────────────────────┐
│ Medidor Monofasico      │
├─────────────────────────┤
│ 1172.5 W                │
│                         │
│ 1172.9 VA               │
│                         │
│ 0.12345 kWh             │
└─────────────────────────┘
```

#### 🖥️ Tela 2 - Potência Reativa e FP
```
┌─────────────────────────┐
│ Medidor Monofasico      │
├─────────────────────────┤
│ 15.2 VAr                │
│                         │
│ 0.99 fp                 │
│                         │
│ 0.12345 kWh             │
└─────────────────────────┘
```

#### 🖥️ Tela 3 - Consumo Total
```
┌─────────────────────────┐
│ Medidor Monofasico      │
├─────────────────────────┤
│ Consumo Total:          │
│                         │
│  0.1234 kWh             │
│                         │
└─────────────────────────┘
```

#### 🖥️ Tela 4 - Informações Wi-Fi
```
┌─────────────────────────┐
│ Medidor Monofasico      │
├─────────────────────────┤
│ Rede: LAB_MED_ELET      │
│ Senha: 12345678         │
│ IP: 192.168.4.1         │
│                         │
└─────────────────────────┘
```

### 2. Interface Web (Acesso via Smartphone/PC)

**Como acessar:**
1. Conecte-se à rede Wi-Fi do medidor (nome: `LAB_MED_ELET`)
2. Abra o navegador
3. Digite o IP: `192.168.4.1`

**Recursos da Interface:**
- 📊 Visualização em tempo real de todas as grandezas
- 🔄 Atualização automática a cada 1,2 segundos
- 📱 Design responsivo (adapta-se ao celular)
- 🎨 Interface limpa e moderna

**Dados exibidos:**
- Tensão RMS (V)
- Corrente RMS (A)
- Potência Ativa (W)
- Potência Reativa (VAR)
- Potência Aparente (VA)
- Fator de Potência
- Energia acumulada (kWh)

### 3. Controle por Botão

**Funções do botão:**

#### Pressão Curta (< 4 segundos)
- Navega entre as 5 telas do display
- Sequência: Tela 0 → 1 → 2 → 3 → 4 → 0 (cíclico)

#### Pressão Longa (10 segundos)
- **RESET do consumo acumulado**
- Contador regressivo aparece no display (10...9...8...0)
- Consumo volta para 0.0000 kWh
- Dado é salvo na memória flash

**Importante:** O botão possui debounce implementado por software para evitar leituras errôneas.

---

## 📡 Configuração Wi-Fi

### Modo de Operação: Access Point (AP)

O ESP32 opera como **ponto de acesso Wi-Fi**, criando sua própria rede sem necessidade de roteador.

### Configurações Padrão

```cpp
SSID:     LAB_MED_ELET
Senha:    12345678
Canal:    1 (configurável)
IP:       192.168.4.1
```

### Usando Múltiplos ESP32

Se você possui vários medidores no mesmo local, é essencial configurar **canais Wi-Fi diferentes** para evitar interferência.

#### Recomendação para 6 ESP32:

| Medidor | SSID | Canal Recomendado |
|---------|------|-------------------|
| ESP32 #1 | LAB_MED_ELET_1 | Canal 1 |
| ESP32 #2 | LAB_MED_ELET_2 | Canal 6 |
| ESP32 #3 | LAB_MED_ELET_3 | Canal 11 |
| ESP32 #4 | LAB_MED_ELET_4 | Canal 3 |
| ESP32 #5 | LAB_MED_ELET_5 | Canal 8 |
| ESP32 #6 | LAB_MED_ELET_6 | Canal 13 |

**Como alterar no código:**

```cpp
// No início do código, altere:
const char* ssid = "LAB_MED_ELET_1";     // Nome único
const char* password = "12345678";
const int wifi_channel = 1;              // Canal único (1-13)
```

#### Por que canais 1, 6 e 11?
Estes são os únicos canais que **não se sobrepõem** no espectro 2.4GHz, garantindo comunicação sem interferência.

### Capacidade de Conexões

- **Máximo:** 4 dispositivos conectados simultaneamente
- **Recomendado:** 2-3 conexões para melhor estabilidade
- Cada conexão consome memória RAM do ESP32

---

## 🎚️ Calibração e Ajustes

### Parâmetros de Calibração

O código possui três constantes principais para ajuste fino das medições:

```cpp
#define V_CAL 194.1      // Calibração de tensão
#define I_CAL 1.12       // Calibração de corrente
#define PHASECAL -2.7    // Calibração de fase
```

### Como Calibrar

#### 1. Calibração de Tensão (V_CAL)

**Objetivo:** Ajustar a leitura de tensão para corresponder à realidade.

**Procedimento:**
1. Meça a tensão real da rede com um multímetro de qualidade
2. Compare com o valor exibido no medidor
3. Calcule o fator de correção:
   ```
   V_CAL_novo = V_CAL_atual × (Tensão_Real / Tensão_Medida)
   ```

**Exemplo:**
```
Tensão real: 220V
Tensão medida: 215V
V_CAL atual: 194.1

V_CAL_novo = 194.1 × (220 / 215) = 198.5
```

#### 2. Calibração de Corrente (I_CAL)

**Objetivo:** Ajustar a leitura de corrente.

**Procedimento:**
1. Use uma carga conhecida (ex: lâmpada de 100W)
2. Calcule a corrente teórica: `I = P / V`
3. Compare com a leitura do medidor
4. Ajuste o I_CAL proporcionalmente

**Exemplo:**
```
Carga: 1000W (1kW)
Tensão: 220V
Corrente teórica: 1000/220 = 4.54A
Corrente medida: 4.80A

I_CAL_novo = 1.12 × (4.54 / 4.80) = 1.06
```

#### 3. Calibração de Fase (PHASECAL)

**Objetivo:** Corrigir o defasamento entre tensão e corrente.

**Quando ajustar:**
- Se o fator de potência (FP) estiver incorreto
- Em cargas puramente resistivas, FP deve ser próximo de 1.0

**Ajuste fino:**
- Valores negativos atrasam a corrente
- Valores positivos adiantam a corrente
- Ajustar em pequenos incrementos (±0.5)

### Otimização de Medição

**Parâmetros da função calcVI:**

```cpp
emon1.calcVI(10, 1500);
//           ↑   ↑
//           |   └─ Timeout (ms)
//           └───── Número de amostras
```

- **Menos amostras:** Resposta mais rápida, menos precisão
- **Mais amostras:** Maior precisão, resposta mais lenta
- **Padrão recomendado:** 10-20 amostras

---

## 📖 Guia de Uso

### Primeira Inicialização

1. **Conecte os sensores** conforme o esquema de ligações
2. **Carregue o código** no ESP32 via Arduino IDE
3. **Aguarde 2 segundos** - Tela inicial será exibida
4. **Observe o Serial Monitor** para ver as informações de conexão

**Serial Monitor mostrará:**
```
=== MEDIDOR MONOFÁSICO ===
SSID: LAB_MED_ELET
SENHA: 12345678
IP: 192.168.4.1
=========================

--- MONITORAMENTO INICIADO ---
```

### Uso Diário

#### Visualização Local (Display OLED)
1. Pressione o **botão** para navegar entre as telas
2. Observe os valores em tempo real
3. O consumo total aparece em todas as telas (exceto tela 3 e 4)

#### Acesso Remoto (Via Celular)

**Passo a passo:**
1. 📱 Abra as configurações Wi-Fi do seu celular
2. 🔍 Procure pela rede `LAB_MED_ELET`
3. 🔐 Conecte usando a senha: `12345678`
4. 🌐 Abra o navegador (Chrome, Safari, etc.)
5. 🔗 Digite na barra de endereço: `192.168.4.1`
6. ✅ A interface web será carregada automaticamente

**Dica:** Salve a página como favorito para acesso rápido!

### Resetar o Consumo

**Quando fazer:**
- Início de um novo mês
- Após pagar a conta de luz
- Para medir consumo específico de equipamentos

**Como fazer:**
1. Pressione e **segure o botão por 10 segundos**
2. Uma **contagem regressiva** aparecerá no display
3. Continue segurando até aparecer **"ZERADO!"**
4. Solte o botão
5. O consumo voltará para 0.0000 kWh

**Atenção:** Esta ação não pode ser desfeita!

### Monitoramento em Tempo Real

**O que observar:**

| Situação | O que acontece | O que significa |
|----------|----------------|-----------------|
| V ≈ 0 e I ≈ 0 | Sem leitura | Verifique conexões dos sensores |
| FP próximo de 1 | Bom fator de potência | Uso eficiente da energia |
| FP < 0.8 | Baixo fator de potência | Muita potência reativa |
| LED piscando | Pulso de energia | A cada 0.625 Wh consumidos |
| P > 0 mas I ≈ 0 | Erro de medição | Recalibrar corrente |

### Interpretando os Dados

#### Exemplo Prático: Chuveiro Elétrico

```
Display mostra:
Tensão: 220V
Corrente: 22.7A
Potência Ativa: 5000W (5kW)
Fator de Potência: 0.99

Interpretação:
- Chuveiro está consumindo 5000W
- Em 1 hora de uso: 5 kWh
- Se kWh custa R$ 0,80: R$ 4,00 por hora
- Banho de 15 minutos: 1,25 kWh = R$ 1,00
```

---

## 🔧 Troubleshooting

### Problemas Comuns e Soluções

#### 1. Display OLED não liga

**Possíveis causas:**
- ❌ Endereço I2C incorreto
- ❌ Conexões SDA/SCL invertidas
- ❌ Display defeituoso

**Soluções:**
1. Verifique as conexões:
   - SDA → GPIO 13
   - SCL → GPIO 14
   - VCC → 3.3V
   - GND → GND

2. Teste o endereço I2C:
```cpp
// Use um scanner I2C para verificar
// Endereço padrão: 0x3C
display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
```

3. Tente outro display para descartar defeito

#### 2. Não consigo conectar no Wi-Fi

**Sintomas:**
- Rede não aparece na lista
- Senha recusada
- Não carrega a página web

**Soluções:**

**Problema: Rede não aparece**
```cpp
// Verifique se o ESP iniciou corretamente
Serial.println(WiFi.softAPIP());  // Deve mostrar 192.168.4.1
```

**Problema: Senha incorreta**
- Confirme a senha no código: `12345678`
- Mínimo 8 caracteres obrigatório

**Problema: Página não carrega**
- Certifique-se de digitar: `http://192.168.4.1`
- Não use `https://` (sem S)
- Tente abrir em modo anônimo do navegador

#### 3. Medições incorretas

**Tensão lendo 0V ou valores absurdos:**
```cpp
// Verifique:
1. Sensor ZMPT101B conectado corretamente no GPIO 34
2. Sensor alimentado (VCC e GND)
3. Calibração V_CAL adequada
```

**Corrente lendo 0A com carga ligada:**
```cpp
// Verifique:
1. SCT-013 abraçando apenas 1 fio (fase OU neutro, não ambos)
2. Conector P2 bem encaixado
3. Polaridade correta
4. Calibração I_CAL
```

**Fator de Potência incorreto:**
```cpp
// Ajuste o PHASECAL:
#define PHASECAL -2.7  // Tente valores entre -5 e 5
```

#### 4. Consumo não está salvando

**Sintomas:**
- Ao reiniciar, consumo volta para zero
- Não salva após desligar

**Solução:**
```cpp
// Verifique se o Preferences está inicializando:
preferences.begin("medidor", false);

// No Serial Monitor deve aparecer a cada 10s:
"💾 [Flash] Consumo salvo automaticamente."
```

**Se persistir:**
- Flash do ESP32 pode estar corrompida
- Tente apagar completamente e regravar:
  - Arduino IDE → Ferramentas → Erase Flash → All Flash Contents

#### 5. LED não pisca

**Sintomas:**
- Consumo aumenta mas LED não indica

**Causa:**
- LED builtin pode estar desabilitado ou queimado

**Solução:**
```cpp
// Teste com LED externo:
#define LED_BUILTIN 2  // Mude para outro GPIO se necessário
pinMode(LED_BUILTIN, OUTPUT);
```

#### 6. Interface web está lenta

**Sintomas:**
- Página demora para atualizar
- Conexão instável

**Soluções:**
1. Reduza o número de dispositivos conectados (máx 2-3)
2. Aproxime-se do ESP32
3. Evite interferências (micro-ondas, Bluetooth)
4. Se múltiplos ESP32, use canais diferentes

#### 7. ESP32 reiniciando sozinho

**Sintomas:**
- Watchdog timer reset
- "Guru Meditation Error"

**Causas comuns:**
- Loop bloqueante muito longo
- Falta de `yield()` ou `delay()`

**Já implementado no código:**
```cpp
server.handleClient();
yield();  // Libera o watchdog
```

**Se persistir:**
- Aumente o intervalo de medição (de 2s para 5s)
- Reduza amostras em `calcVI()`

### Mensagens de Erro Serial

| Mensagem | Significado | Solução |
|----------|-------------|---------|
| `Erro: Display nao encontrado` | OLED não responde | Verificar conexões I2C |
| `Watchdog timer` | Loop travado | Adicionar `yield()` |
| `Brownout detector` | Tensão baixa | Fonte de alimentação fraca |

---

## 📊 Especificações Técnicas

### Limites e Precisão

| Parâmetro | Faixa | Precisão |
|-----------|-------|----------|
| Tensão | 0-250V AC | ±1% (após calibração) |
| Corrente | 0-100A | ±2% (após calibração) |
| Potência | 0-22kW | ±2-3% |
| Frequência | 50/60Hz | Automático |
| Consumo | 0-999.9999 kWh | ±2-3% |

### Desempenho

- **Taxa de amostragem:** A cada 2 segundos
- **Atualização web:** 1,2 segundos
- **Salvamento flash:** 10 segundos
- **Tempo de resposta botão:** ~50ms (debounce)

### Consumo do Medidor

- **Operação normal:** ~150-200mA @ 5V
- **Potência consumida:** ~1W
- **Custo mensal:** Desprezível (< R$ 0,50/mês)

---

## 🚀 Melhorias Futuras (Ideias)

### Hardware
- [ ] Adicionar módulo SD para log de dados
- [ ] Bateria backup para manter hora/data
- [ ] Display colorido maior
- [ ] Caixa 3D impressa profissional

### Software
- [ ] Gráficos históricos na interface web
- [ ] Integração com Home Assistant / MQTT
- [ ] Notificações via Telegram/WhatsApp
- [ ] Exportação de dados CSV
- [ ] App mobile dedicado
- [ ] Medição trifásica

---

## 📞 Suporte e Contato

### Recursos Adicionais
- 📚 Biblioteca EmonLib: [OpenEnergyMonitor](https://github.com/openenergymonitor/EmonLib)
- 🔧 ESP32 Docs: [Espressif](https://docs.espressif.com/)
- 💬 Comunidade Arduino: [Forum Arduino](https://forum.arduino.cc/)

### Informações do Projeto
- **Versão:** 1.0
- **Data:** 2025
- **Licença:** Open Source
- **Linguagem:** C++ (Arduino Framework)

---

## 📄 Licença

Este projeto é de código aberto e pode ser usado, modificado e distribuído livremente para fins educacionais e comerciais.

**⚠️ Aviso de Segurança:**
Este projeto trabalha com rede elétrica de alta tensão (127V/220V). Sempre tome as devidas precauções:
- ⚡ Desligue a energia antes de fazer conexões
- 👷 Use equipamentos de proteção adequados
- 🔒 Instale em caixa isolada e protegida
- 👨‍🔧 Em caso de dúvida, consulte um eletricista profissional

---

**Desenvolvido com ❤️ para a comunidade maker**