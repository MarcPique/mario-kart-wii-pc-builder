## Mario Kart Wii PC Builder 1.0.2

Genera tu juego de Windows a partir de tu propio disco **PAL RMCP01**. Descarga `MKWii-PC-Builder.exe`, selecciona tu ISO, WBFS o RVZ y elige la carpeta. El instalador crea el EXE del escritorio con el icono obtenido localmente de tu disco.

- **Mandos antes que teclado:** al abrir o conectar/desconectar dispositivos, los mandos ocupan automáticamente J1–J4, conservando su orden relativo y rellenando puestos libres. Después puedes reasignarlos en Ajustes. Sin mando, el teclado controla J1. DualSense Edge verificado por Bluetooth en la instalación del usuario.
- **Mandos nativos:** selector DualSense / DualShock / Switch Pro / Xbox, nombre completo en el panel inferior y selección por identidad del dispositivo. Se han probado mandos virtuales, desconexión y sustitución sin reiniciar. Otros modelos y cuatro mandos físicos simultáneos necesitan pruebas.
- **Personajes personalizados en carreras locales:** selecciona la variante con **L/R** —**L1/R1** en DualSense— antes de confirmar. **Mario (Fire) comprobado en Grand Prix**, además del modelo de selección. Usa las variantes incluidas en Retro Rewind 6.13.4.
- **Online integrado:** Retro WFC, login, amigos, selector de salas y refresco automático cada 40 segundos en el mismo proceso. Se ha verificado entrada a salas Retro y CT. CT 200cc no tiene ruta disponible; otros modos y cambios de anfitrión requieren pruebas.
- **Opciones avanzadas:** menús originales en Opciones y pausa, cinco idiomas inmediatos, presets y MSAA/anisotropía sin reiniciar, probados en una GPU. Corrección del error mostrado al finalizar la instalación.

**Límites actuales:** cuatro jugadores en multijugador local y **dos jugadores locales por PC en Retro WFC**. El protocolo de selección y los paquetes de carrera reservan dos jugadores por conexión; este release no habilita cuatro jugadores online desde un solo PC. No incluye selector My Stuff, cambio de partida separada en vivo ni actualización del código durante una sesión. La preparación experimental de My Stuff queda desactivada en los arranques normales.

**Archivos públicos:** `MKWii-PC-Builder.exe` y `SHA256SUMS.txt`. No incluyen ROM, juego recompilado, perfiles de prueba, partidas ni claves de consola. Las fuentes del fork, el constructor y sus parches están en el repositorio.

Requiere Windows x64 con AVX2, GPU D3D12, Internet y 20 GiB libres durante la compilación; se recomiendan 16 GiB de RAM. El instalador incluye Python y .NET; descarga las herramientas fijadas por SHA-256. No lleva firma Authenticode.

El juego generado conserva una carpeta con EXE, `GameData` y `UserData`. Para cambiar de PC, copia la carpeta completa. El EXE del escritorio apunta a esa instalación.

Créditos: WiiCompiled / patchzyy, Retro Rewind / Retro WFC, Pulsar, Aurora y las bibliotecas indicadas en el repositorio. Proyecto no oficial de Nintendo, desarrollado con asistencia de Codex (OpenAI).
