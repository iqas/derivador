DERIVADOR DE EXCEDENTES PARA SOLAX X1

Funciona con modulo WIFI v1 solamente.
Salida de señal PWM a Dimmer PWM de venta en Ebay
Módulo ESP32

El módulo lee el consumo de red mediante el api de Solax, cuando el consumo es menor de 100w, se incrementa la señal PWM para que el dimmer comienze a dar potencia a la carga, si el consumo de red es mayor de 150w, la operación se realiza a la inversa. El led azul irá incrementando el brillo en función de la potencia enviada por el dimmer.

Puede enlazarse a un servidor mqtt (mosquitto) para enviar los datos a un sistema domótico y desde el mismo encender los reles.
La gestión de los reles en esta version solo se puede hacer mediante mqtt, asi mismo se puede desconectar la señal pwm desde el procolo mqtt.


