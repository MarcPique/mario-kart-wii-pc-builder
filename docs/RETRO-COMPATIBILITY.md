# Compatibilidad de Retro Rewind en 1.2.0

La build usa el pack oficial **6.13.4** y su código traducido a Windows. La
consulta del [manifiesto oficial](https://update.rwfc.net/RetroRewind/RetroRewindVersion.txt) del 4 de noviembre de 2026 sigue indicando
6.13.4 como versión más reciente. Esto verifica la versión disponible; no
constituye una prueba de instalación de una futura actualización.

## CT 200cc

El [servidor web oficial reconoce `vs_22` como CT 200cc](https://github.com/Retro-Rewind-Team/rwfc-web/blob/main/Backend/Models/DTOs/Room/RoomDtoExtensions.cs).
Una etiqueta del servidor no demuestra que un cliente pueda jugar ese modo.

En el [selector oficial inspeccionado](https://github.com/Jacherr/Retro-Rewind/blob/794f569fdfc43084971bf60f6d7cac04e0b2863e/PulsarEngine/Settings/UI/ExpWFCMainPage.cpp),
los botones asignan las regiones 10–15, 20 y 21. El [cálculo de contextos](https://github.com/Jacherr/Retro-Rewind/blob/794f569fdfc43084971bf60f6d7cac04e0b2863e/PulsarEngine/PulsarSystem.cpp)
activa 200cc para la región 12 y circuitos personalizados para la 20; no hay un
caso para la región 22. La [lógica de regiones de amigos](https://github.com/Jacherr/Retro-Rewind/blob/794f569fdfc43084971bf60f6d7cac04e0b2863e/PulsarEngine/Network/Region.cpp)
tampoco incluye la 22. Esta revisión de fuentes no establece por sí sola la
equivalencia de cada función con el binario del pack.

La integración nativa conserva la ausencia de una ruta verificada para
`vs_22`. No la redirige a CT normal ni a Retro 200cc. Habilitarla requiere
verificar conjuntamente selección de circuitos, física, paquetes de red y una
partida con un cliente compatible; esa prueba no se ha completado. Los modos
**Retro y CT normal** sí cuentan con pruebas de entrada a salas. La existencia
de la etiqueta no se presenta como compatibilidad implementada.

## Actualizaciones del launcher

El [launcher oficial](https://github.com/Retro-Rewind-Team/rr-launcher/tree/d7bde0b7dd933e4c5cfc7b4c237c49b97e135baf/source/update)
aplica archivos ZIP antes de iniciar el juego Wii. Su manifiesto enumera
versiones y URLs. El runtime de Windows comprueba ese manifiesto por HTTPS al
arrancar, cada seis horas y al pulsar Revisar.

El builder descarga un pack fijado por tamaño y SHA-256 y recompila su código
localmente. Una actualización del código Wii requiere generar de nuevo la
traducción nativa; sustituir `Code.pul` en una aplicación ya traducida no
actualiza las funciones que está ejecutando. **No hay descarga, preparación ni
aplicación automática de versiones futuras desde el menú del juego.** La
actualización de la instalación se realiza con el builder y conserva
`UserData`, dejando el contenido anterior en `GameData.previous`.

My Stuff, cambio confirmado de partida, consulta automática de versiones y
salida están integrados. La instalación de un canal Wii no se aplica a Windows;
su punto de entrada es el EXE con icono del escritorio. No se anuncia que todas
las funciones del launcher original estén implementadas.

## Mandos y hardware

La distribución predeterminada es Nintendo: círculo acepta y acelera en
DualSense/DualShock; X vuelve y frena. Se conservan las asignaciones explícitas
del usuario. Online permite uno o dos jugadores locales en un solo cliente;
cuatro jugadores siguen disponibles sin conexión.

Las pruebas con cuatro mandos virtuales comprueban asignación y registro, pero
no sustituyen pruebas de radio, firmware ni vibración de cuatro mandos físicos.
El DualSense Edge por Bluetooth fue comprobado por el usuario. Hay pruebas de
carreras en RTX 4090 y una prueba adicional de arranque y navegación de menús
en AMD Radeon(TM) Graphics integrada con D3D12. Esta última utilizó una build
privada que solicitaba la GPU de bajo consumo; no cambia la preferencia de
la build pública ni certifica el rendimiento de carreras en esa GPU.
