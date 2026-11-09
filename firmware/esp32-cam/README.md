# Firmware ESP32-CAM

Firmware para ESP32-CAM AI Thinker com OV2640, compatível com o leitor de
MJPEG e o buzzer do APK Android.

## Endpoints

- Access Point: `HELIOS-CAM`, IP `192.168.4.1`
- Stream: `http://192.168.4.1:81/stream`
- Status: `http://192.168.4.1/status`
- Buzzer: `POST http://192.168.4.1/buzzer?padrao=0|1|2`

## O que editar

Edite somente `include/config.h` para alterar:

- nome da rede: `HELIOS_WIFI_SSID`;
- senha WPA2: `HELIOS_WIFI_PASSWORD`;
- canal Wi-Fi;
- GPIO do buzzer;
- resolução e qualidade JPEG.

O padrão é uma rede aberta `HELIOS-CAM`, pois o Android está configurado com
`senha = ""`. Não coloque credenciais da sua rede residencial no repositório.

## Arduino IDE

1. Instale o suporte `esp32 by Espressif Systems` no Boards Manager.
2. Abra como sketch a pasta:
   `firmware/esp32-cam/arduino/helios_cam`
3. Selecione `AI Thinker ESP32-CAM`.
4. Selecione a porta serial do conversor USB-serial.
5. Ligue IO0 ao GND somente durante o upload.
6. Faça o upload.
7. Remova IO0 do GND e pressione RESET.
8. Abra o Serial Monitor em `115200`.

O sketch Arduino já está pronto e inclui o código compartilhado em
`firmware/esp32-cam/src/main.cpp`. Não edite o arquivo `.ino` para mudar a
configuração. O código aceita tanto o core ESP32 2.x quanto o 3.x; no PlatformIO
usamos 2.x fixo.

## PlatformIO

Abra `firmware/esp32-cam` no VS Code com PlatformIO e execute `Upload`. A
versão de Arduino-ESP32 está fixada em `platformio.ini` para manter compatibilidade
com `ledcSetup` e `ledcAttachPin`.

## Teste

Conecte um computador ou celular à rede `HELIOS-CAM` e execute:

```powershell
curl.exe http://192.168.4.1/
curl.exe http://192.168.4.1/status
curl.exe -X POST "http://192.168.4.1/buzzer?padrao=1"
curl.exe -X POST "http://192.168.4.1/buzzer?padrao=0"
```

Abra `http://192.168.4.1:81/stream` para verificar as imagens. Depois selecione
`ESP32-CAM pelo Wi-Fi` nos Ajustes do APK e inicie uma viagem.

O buzzer deve ser ligado ao GPIO 13 por transistor ou módulo apropriado. Não
ligue um buzzer de potência diretamente ao GPIO. O GPIO 4 fica reservado ao
flash da câmera.
