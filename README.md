# iBeacon ESP32

Projeto de transmissor iBeacon usando ESP32 com framework ESP-IDF.

## 📋 Descrição

Este projeto transforma uma ESP32 em um transmissor iBeacon, permitindo que o dispositivo seja detectado por aplicativos móveis compatíveis com a tecnologia iBeacon da Apple. O iBeacon utiliza Bluetooth Low Energy (BLE) para transmitir sinais de proximidade.

**Beacon padrão após o flash:**
- **UUID:** `FDA50693-A4E2-4FB1-AFCF-C6EB07647825`
- **Major:** 10167
- **Minor:** 61958
- **Measured Power:** -59 dBm (0xC5)

> ⚠️ Esses valores são sobrescritos assim que o operador executa o comando `save` via CLI. Use-os apenas como referência inicial ou para gerar QR Codes de fábrica.

## 🔧 Pré-requisitos

### Hardware
- Placa ESP32 (qualquer modelo compatível com ESP-IDF)
- Cabo USB para programação e alimentação
- Computador com macOS, Linux ou Windows

### Software
- ESP-IDF v5.0 ou superior
- Python 3.8 ou superior
- Driver USB para ESP32 (geralmente já incluído no macOS/Linux)

## 📦 Instalação do ESP-IDF

### macOS

1. **Instale as dependências:**
```bash
brew install cmake ninja dfu-util
```

2. **Clone o repositório do ESP-IDF:**
```bash
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
```

3. **Instale as ferramentas do ESP-IDF:**
```bash
./install.sh
```

4. **Configure as variáveis de ambiente:**
```bash
. $HOME/esp/esp-idf/export.sh
```

> **Dica:** Adicione o comando de export ao seu `~/.zshrc` ou `~/.bash_profile` para não precisar executá-lo toda vez:
```bash
echo 'alias get_idf=". $HOME/esp/esp-idf/export.sh"' >> ~/.zshrc
```

## 🚀 Como Executar o Projeto

### 1. Preparar o Ambiente

Antes de começar, ative o ambiente ESP-IDF:

```bash
# Se adicionou o alias
get_idf

# Ou execute diretamente
. $HOME/esp/esp-idf/export.sh
```

### 2. Navegar até o Diretório do Projeto

```bash
cd path-to-project
```

### 3. Conectar a ESP32

Conecte sua placa ESP32 ao computador via USB.

### 4. Configurar o Target

Defina a ESP32 como dispositivo alvo:

```bash
idf.py set-target esp32
```

### 5. Compilar o Projeto

```bash
idf.py build
```

Este comando irá:
- Verificar todas as dependências
- Compilar o código fonte
- Gerar o binário para flash

### 6. Flash na ESP32

Grave o firmware na ESP32:

```bash
idf.py -p <PORT> flash
```

> **Nota:** No macOS, a porta geralmente é `/dev/tty.usbserial-*` ou `/dev/tty.SLAB_USBtoUART`. Use `ls /dev/tty.*` para verificar a porta disponível.

### 7. Monitorar a Saída Serial

Para visualizar os logs do dispositivo:

```bash
idf.py -p <PORT> monitor
```

Para compilar, gravar e monitorar tudo em um único comando:

```bash
idf.py -p <PORT> flash monitor
```

Para sair do monitor, pressione `Ctrl + ]`.

## 🔧 Provisionamento via CLI

Depois de gravar o firmware, o ESP32 expõe um console serial simples para configurar `major` e `minor` sem recompilar o projeto. Basta abrir o monitor com o `idf.py` (ou qualquer terminal serial) e digitar os comandos abaixo:

```
ibeacon> help
Available commands:
  help                Show this message
  show                Display current UUID/Major/Minor
  set_major <value>   Set major (0-65535)
  set_minor <value>   Set minor (0-65535)
  save                Persist values and update advertising
  blink <seconds>     Blink status LED to locate device
  restart             Restart device
```

### Passo a passo sugerido

1. Conecte o ESP32 e abra o monitor (ex.: `idf.py -p /dev/cu.usbserial-0001 monitor`).
2. Execute `show` para conferir os parâmetros atuais.
3. Defina novos valores conforme a planilha/catalog de beacons:
    ```
    set_major 22015
    set_minor 1234
    ```
4. Salve e aplique imediatamente:
    ```
    save
    ```
    O firmware grava na NVS, atualiza o pacote iBeacon em tempo real e imprime os valores efetivos.
5. (Opcional) Acione o LED para identificar fisicamente o dispositivo:
    ```
    blink 5
    ```
6. Finalize com `show` novamente para registrar os números no inventário.

> 💡 O comando `save` é o único que persiste as mudanças. Se você apenas usar `set_major/set_minor` e desligar a placa, os valores serão perdidos.

### Payload padrão para QR de fábrica

Antes de qualquer personalização, todos os ESPs recém-flashados anunciam:

```json
{"uuid":"FDA50693-A4E2-4FB1-AFCF-C6EB07647825","major":10167,"minor":61958}
```

Use esse JSON para gerar um QR Code “genérico” se precisar identificar unidades ainda não provisionadas.

## 📱 Verificando o iBeacon

### Opção 1: Aplicativos iOS
- **Locate Beacon** (gratuito)
- **BeaconScanner** (gratuito)

### Opção 2: Aplicativos Android
- **nRF Connect** (Nordic Semiconductor)
- **Beacon Scanner** (gratuito)

## ⚙️ Personalização

Agora existem duas formas de alterar os identificadores:

1. **Via CLI (recomendado em campo):** use os comandos `set_major`, `set_minor` e `save`, conforme descrito na seção anterior.
2. **Via código (para builds customizados):** edite as constantes abaixo e reconstrua o firmware.

### Modificar UUID, Major e Minor

Edite o arquivo `main/ibeacon.c` e altere as seguintes definições:

```c
// UUID do iBeacon (16 bytes)
#define IBEACON_UUID    {0xFD, 0xA5, 0x06, 0x93, 0xA4, 0xE2, 0x4F, 0xB1, \
                         0xAF, 0xCF, 0xC6, 0xEB, 0x07, 0x64, 0x78, 0x25}

// Major ID (0-65535)
#define IBEACON_MAJOR   10167

// Minor ID (0-65535)
#define IBEACON_MINOR   61958

// Potência medida em dBm (valor negativo em hex)
#define IBEACON_MEASURED_POWER  0xC5  // -59 dBm
```

Após modificar, recompile e grave novamente:
```bash
idf.py build flash
```

> 🔁 Mesmo após recompilar, você ainda pode sobrescrever os valores com a CLI sem reflashear o dispositivo. Essa flexibilidade facilita reutilizar o hardware em diferentes yards.

### Ajustar Intervalo de Transmissão

No arquivo `main/ibeacon.c`, modifique os parâmetros de advertising:

```c
static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x20,     // Mínimo: 20ms (0x20 * 0.625ms)
    .adv_int_max        = 0x40,     // Máximo: 40ms (0x40 * 0.625ms)
    // ...
};
```

Valores menores = transmissão mais frequente (maior consumo de energia)
Valores maiores = transmissão mais espaçada (menor consumo de energia)

## 🔍 Logs Esperados

Após a execução bem-sucedida, você verá logs semelhantes a:

```
I (XXX) IBEACON: ========================================
I (XXX) IBEACON:    Starting iBeacon Sender ESP32
I (XXX) IBEACON: ========================================
I (XXX) IBEACON: Starting iBeacon...
I (XXX) IBEACON: iBeacon ready, waiting for trasmission to start...
I (XXX) IBEACON: iBeacon setup complete, start advertising...
I (XXX) IBEACON: iBeacon succesfully started!
I (XXX) IBEACON: UUID: FDA50693-A4E2-4FB1-AFCF-C6EB07647825
I (XXX) IBEACON: Major: 10167, Minor: 61958
I (XXX) IBEACON: iBeacon running. The device is now trasmitting.
```

## 🏷️ Gerando e usando QR Codes

Para simplificar o check-in da moto, você pode colar uma etiqueta com QR na carcaça do beacon. O QR deve conter o trio `uuid/major/minor` no formato JSON — o mesmo payload usado pela API ou pelo app móvel.

### Fluxo sugerido

1. **Planilha de inventário:** mantenha um arquivo (por exemplo, `beacon_inventory.csv`) com colunas `BeaconCode`, `Major`, `Minor`, `QRPayload`, etc.
2. **Provisionamento:** durante a configuração via CLI, atualize a planilha com os valores efetivos e gere o JSON:
    ```json
    {"uuid":"FDA50693-A4E2-4FB1-AFCF-C6EB07647825","major":22015,"minor":1234}
    ```
3. **Geração de QR:** use uma das abordagens abaixo:
    - **Google Sheets:** `=IMAGE("https://quickchart.io/qr?text=" & ENCODEURL(K2))`
    - **Script Python (qrcode):** converta cada linha do CSV em um PNG para impressão.
    - **Impressora térmica (ZPL):** envie o payload direto para fabricação de etiquetas.
4. **Instalação em campo:** prenda o beacon na moto, cole a etiqueta com o QR e registre a associação no aplicativo.

Quando o operador lê o QR durante o check-in, o app já sabe exatamente qual iBeacon filtrar, evitando erros de digitação e facilitando auditorias futuras.

## 📚 Referências

- [Documentação ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Especificação iBeacon](https://developer.apple.com/ibeacon/)
- [ESP32 Bluetooth LE (BLE)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html)

## 👥 Equipe de Desenvolvimento

| Nome                        | RM      | Turma    | E-mail                 | GitHub                                         | LinkedIn                                   |
|-----------------------------|---------|----------|------------------------|------------------------------------------------|--------------------------------------------|
| Arthur Vieira Mariano       | RM554742| 2TDSPF   | arthvm@proton.me       | [@arthvm](https://github.com/arthvm)           | [arthvm](https://linkedin.com/in/arthvm/)  |
| Guilherme Henrique Maggiorini| RM554745| 2TDSPF  | guimaggiorini@gmail.com| [@guimaggiorini](https://github.com/guimaggiorini) | [guimaggiorini](https://linkedin.com/in/guimaggiorini/) |
| Ian Rossato Braga           | RM554989| 2TDSPY   | ian007953@gmail.com    | [@iannrb](https://github.com/iannrb)           | [ianrossato](https://linkedin.com/in/ianrossato/)      |

## 📄 Licença

Este projeto foi desenvolvido para fins acadêmicos como parte do challenge da Mottu FIAP.

---

**Nota:** iBeacon é uma marca registrada da Apple Inc.

