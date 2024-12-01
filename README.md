# iot-pump-control-esp
## Ambiente de desenvolvimento

Para editar o código fonte utilize a IDE Visual Studio com o plugin platformio

## Tutoriais:

Como instalar e configurar o VSCode e o platformio
https://www.youtube.com/watch?v=A4ZMBIuvHGc

## Conexões com o display TFT
Siga a figura a seguir para conectar o esp-32 ao display tft
![image](https://github.com/user-attachments/assets/60de3386-7009-4dda-99c4-ba72ba580c27)

Caso o display não funcione altere o arquivo de configuração do driver do display em:

.pio\libdeps\esp32doit-devkit-v1\TFT_eSPI\User_Setup.h

para o arquivo salvo nesse repositorio (User_Setup.h)

## Conexões com os botões

Conecte um dos pinos do bloco de contato dos botões no GND.

Conecte o outro pino de cada botão nos pinos do ESP indicados a seguir:

BOTAO 1 - PINO 32

BOTAO 2 - PINO 33

BOTAO 3 (pause) - PINO 5

## Conexões com os relés

As saídas para os relés são as seguintes:

BOMBA1 - PINO 2

BOMBA2 - PINO 25
