DERIVADOR/GESTOR DE EXCEDENTES PARA INVERSORES SOLARES

Funciona con modulo WIFI v1 y v2 de Solax y Fronius.
Salida de señal PWM a Dimmer PWM de venta en Ebay
Módulo ESP32 o DEV KIT 32 con pantalla OLED

El módulo lee el consumo de red mediante el api de Solax o Fronius, cuando el consumo es menor de un valor prefijado, se incrementa la señal PWM para que el dimmer comienze a dar potencia a la carga, si el consumo de red es mayor de un valor prefijado, la operación se realiza a la inversa. El led irá incrementando el brillo en función de la potencia enviada por el dimmer.

La configuración se realiza mediante un entorno web amigable, disponiendo de los datos en tiempo real.

Puede enlazarse a un servidor mqtt (mosquitto) o mediante Api remota (http) para enviar los datos a un sistema domótico o estadístico y desde el mismo encender los reles.
La gestión de los reles en esta version ya es completa, mediante gpios y mqtt.

Para más información consultar el manual en la carpeta doc

Configuración Wifi, Mqtt y salidas desde entorno web

Novedades en la versión 1.1:

- Test de meter, si el meter se mantiene en 0 durante un tiempo, corta el derivado de excedentes
- Seguridad de acceso en la web, clave por defecto admin:admin
- Cambios en los Topic de mqtt
- Añadido modo local para Solax wifi v2
- Testeado funcionamiento en híbridos solax
- Testeado funcionamiento en Solax
- Iniciado soporte para fronius
- Mejoras gráficas en indicador de relés en web
- Añadido soporte completo para ESP32 (sin pantalla)
- Limitado ancho de campo en web
- Remote API, para enviar la información a otros servisores/servicios
- Web server para configuración (Android 9 da problemas con SmartConfig)(Ver manual)
- Resuelto bug en encedido de relés
- Si perdemos conexión se crea Wifi AP+STA y si no se configura se reinicia, para evitar modo espera
- Resuelto bug y comprobado funcionamiento con Fronius
- Encendido de salidas (Relés y mqtt) retardadas para evitar el encendido de varias antes de que el inversor regule vertido
- Reinicio de mqtt en caso de fallo testeando cada minuto si el server está online
- Sistema mejorado de conexión con Solax V2 y esp01
- Sistema de actualización a través de internet (OTA) para Esp32/DevKit32 y esp01
- Api local para acceder a los datos del inversor sin saturar el mismo






