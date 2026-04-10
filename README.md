# Laboratorio-2
Este proyecto consiste en un videojuego implementado en un ESP32 utilizando una matriz LED 8x8 bicolor (rojo/verde). El jugador está representado por un LED verde ubicado en la última fila de la matriz y puede desplazarse horizontalmente mediante dos botones.

Desde la parte superior descienden obstáculos representados con LEDs rojos en tres carriles específicos. El objetivo del juego es evitar las colisiones moviendo el jugador a tiempo.

El sistema incluye una lógica de conteo de colisiones:

En la primera colisión se muestra el número 1 en la matriz.
En la segunda colisión se muestra el número 2, seguido de una animación donde toda la matriz se ilumina en verde.
Luego, el juego se reinicia automáticamente.

El control de la matriz se realiza mediante multiplexación por filas, utilizando transistores PNP, y el firmware fue desarrollado en C con ESP-IDF, sin uso de librerías externas.
