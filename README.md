# iBeacon ESP32

Projeto de transmissor iBeacon usando ESP32 com framework ESP-IDF.

## 📋 Descrição

Este projeto transforma uma ESP32 em um transmissor iBeacon, permitindo que o dispositivo seja detectado por aplicativos móveis compatíveis com a tecnologia iBeacon da Apple. O iBeacon utiliza Bluetooth Low Energy (BLE) para transmitir sinais de proximidade.

**Configuração atual do iBeacon:**
- **UUID:** `FDA50693-A4E2-4FB1-AFCF-C6EB07647825`
- **Major:** 10167
- **Minor:** 61958
- **Measured Power:** -59 dBm (0xC5)

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

## 📱 Verificando o iBeacon

### Opção 1: Aplicativos iOS
- **Locate Beacon** (gratuito)
- **BeaconScanner** (gratuito)

### Opção 2: Aplicativos Android
- **nRF Connect** (Nordic Semiconductor)
- **Beacon Scanner** (gratuito)

## ⚙️ Personalização

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

