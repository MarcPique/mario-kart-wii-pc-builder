## Mario Kart Wii PC Builder 1.1.0

**Idiomas revisados:** catalán, gallego y euskera pasan a estado revisado, sin asteriscos ni aviso beta en el juego o instalador. Revisión de **1.543 fragmentos de juego por idioma** y **82 textos de PC en cada uno de los quince idiomas (1.230 entradas)**. La última pasada corrige acrobacias, invitaciones, multijugador, plurales, eliminaciones y nombres de circuitos.

La generación de publicación exige aceptar todos los fragmentos requeridos; se comprueban fuentes, botones, variables y formatos como `%ls` y `%04d`. Se conservan los nombres propios y el cambio de idioma inmediato, sin reiniciar. Catalán, gallego y euskera tienen origen automático con revisión contextual posterior; esta procedencia sigue documentada. No son traducciones oficiales ni una certificación por hablantes nativos. [Alcance y procedencia](https://github.com/MarcPique/mario-kart-wii-pc-builder/blob/main/docs/LOCALIZATION.md).

**15 idiomas:** español, inglés, francés, alemán, italiano, neerlandés, checo, finés, ruso, turco, español latinoamericano, catalán, gallego, euskera y andalûh (EPA). Solo se utilizan las fuentes existentes. Voces e imágenes con texto conservan su contenido. Los avisos enviados por Retro WFC conservan el idioma del servidor y pueden estar en inglés.

**Formatos visibles en la primera página:** abre Opciones avanzadas → Pantalla y utiliza el selector inferior Formato para recorrer los 16 formatos. Los tres botones superiores son accesos rápidos. La resolución interna sigue disponible en su página propia.

**ZIP y 7z sin descomprimir:** copia tus paquetes en `UserData/Load/Textures/RMCP01` o `RMC`. Se importan en segundo plano al abrir el juego o pulsar Recargar; no hace falta instalar 7-Zip ni reiniciar. Se admiten paquetes con carpetas anidadas y el formato de recursos de Dolphin. Los PNG/DDS sueltos tienen prioridad. Si falla la importación o falta una textura, permanece la original. Los archivos cifrados no se importan. Los resultados completos se guardan en caché por SHA-256.


- **Salir al escritorio** en el menú principal, con confirmación original, Cancelar seleccionado por defecto y B para volver. Se ha verificado el cierre con código 0.
- **16 formatos en vivo:** Automático, 4:3, 5:4, 3:2, 16:10, 16:9, 1.85:1, 2:1, 21:9, 43:18, 64:27, 32:10, 32:9, 48:9, 1:1 y 9:16. El formato fijo conserva proporciones y usa bandas si corresponde; Automático sigue la ventana.
- **Texturas de Dolphin PNG/DDS** con activación y recarga en el menú nativo, sin reiniciar. Copia los archivos descomprimidos a `UserData/Load/Textures/RMCP01` o `RMC`, conservando los nombres `tex1_`. Se han probado PNG, DDS, paquete parcial, archivo dañado y retirada del paquete: lo que falta o falla utiliza el original. ZIP y 7z se importan automáticamente; RAR debe extraerse antes. No se incluye ningún paquete de texturas.


Genera tu juego de Windows a partir de tu propio disco **PAL RMCP01**. Descarga `MKWii-PC-Builder.exe`, selecciona tu ISO, WBFS o RVZ y elige la carpeta. El instalador crea el EXE del escritorio con el icono obtenido localmente de tu disco.

- **Mandos antes que teclado:** al abrir o conectar/desconectar dispositivos, los mandos ocupan automáticamente J1–J4, conservando su orden relativo y rellenando puestos libres. Después puedes reasignarlos en Ajustes. Sin mando, el teclado controla J1. DualSense Edge verificado por Bluetooth en la instalación del usuario.
- **Mandos nativos:** selector DualSense / DualShock / Switch Pro / Xbox, nombre completo en el panel inferior y selección por identidad del dispositivo. Se han probado mandos virtuales, desconexión y sustitución sin reiniciar. Otros modelos y cuatro mandos físicos simultáneos necesitan pruebas.
- **Personajes personalizados en carreras locales:** selecciona la variante con **L/R** —**L1/R1** en DualSense— antes de confirmar. **Mario (Fire) comprobado en Grand Prix**, además del modelo de selección. Usa las variantes incluidas en Retro Rewind 6.13.4.
- **Online integrado:** Retro WFC, login, amigos, selector de salas y refresco automático cada 40 segundos en el mismo proceso. Se ha verificado entrada a salas Retro y CT. CT 200cc no tiene ruta disponible; otros modos y cambios de anfitrión requieren pruebas.
- **Opciones avanzadas:** menús originales en Opciones y pausa, quince idiomas inmediatos, presets y MSAA/anisotropía sin reiniciar, probados en una GPU. Corrección del error mostrado al finalizar la instalación.

**Límites actuales:** cuatro jugadores en multijugador local y **dos jugadores locales por PC en Retro WFC**. El protocolo de selección y los paquetes de carrera reservan dos jugadores por conexión; este release no habilita cuatro jugadores online desde un solo PC. No incluye selector My Stuff, cambio de partida separada en vivo ni actualización del código durante una sesión. La preparación experimental de My Stuff queda desactivada en los arranques normales.

**Archivos públicos:** `MKWii-PC-Builder.exe` y `SHA256SUMS.txt`. No incluyen ROM, juego recompilado, perfiles de prueba, partidas ni claves de consola. Las fuentes del fork, el constructor y sus parches están en el repositorio.

Requiere Windows x64 con AVX2, GPU D3D12, Internet y 20 GiB libres durante la compilación; se recomiendan 16 GiB de RAM. El instalador incluye Python y .NET; descarga las herramientas fijadas por SHA-256. No lleva firma Authenticode.

El juego generado conserva una carpeta con EXE, `GameData` y `UserData`. Para cambiar de PC, copia la carpeta completa. El EXE del escritorio apunta a esa instalación.

Créditos: WiiCompiled / patchzyy, Retro Rewind / Retro WFC, Pulsar, Aurora y las bibliotecas indicadas en el repositorio. Proyecto no oficial de Nintendo, desarrollado con asistencia de Codex (OpenAI).
