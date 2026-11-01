# Punto de continuación · 29-10-2026

Proyecto en pausa por solicitud del usuario. No continuar automáticamente. Leer este documento y la entrada superior de ETAPAS.md al retomarlo. Mantener comunicaciones breves y evitar búsquedas/lecturas repetidas; el usuario pidió reducir tokens. No usar subagentes sin autorización para este tramo.

## Petición pendiente

Soportar interfaces Core Audio como SSL 2+, hasta 18 canales, asignación de entradas y diagnóstico de Waves que funciona en Cubase bajo Rosetta pero presenta problemas en Silente. El usuario decidió continuar sin conectar la SSL. No modificar licencias ni instalaciones de terceros.

## Implementado y compilado, todavía sin cierre de etapa

- src/audio/DeviceSettings.h/.cpp: configuración JUCE/Core Audio hasta 18 entradas y salidas, XML persistente en ~/Library/Silente/audio-device.xml, inventario y resumen. La configuración se guarda explícitamente con el botón Guardar configuración. Arranque nuevo con entrada desactivada; una preferencia previa conserva los canales elegidos.
- src/ui/audio/AudioSettingsPanel.h/.cpp: selector de dispositivo, canales, frecuencia/búfer y MIDI, actualización de dispositivos y guardado. Ajustes bloqueados durante grabación. La mezcla estéreo usa los dos primeros canales de salida activados; no existen 18 mezclas de salida independientes.
- Track incorpora inputLeft/inputRight (índices físicos 0–17; iguales = mono). Inspector ofrece 18 entradas mono y nueve pares estéreo. Asignación incluida en historial y archivo de proyecto, con valores por defecto para proyectos antiguos.
- AudioEngine convierte índices físicos a canales activos del callback Core Audio, usa la entrada de cada pista para monitorizar/grabar y deja de forzar las entradas 1–2 al grabar. Archivo WAV mono para entrada mono, estéreo para par. Entrada desactivada produce silencio y la grabación comprueba los canales asignados.
- Recursos español/inglés: 350 entradas por idioma; comprobación estática aprobada. Todavía falta repetir la integración completa de idiomas tras este tramo.
- scripts/build_intel.sh genera build-intel/ y releases/macOS/Silente-Intel.app (x86_64, Rosetta disponible en este Mac). El catálogo Intel se separa en Plugins-Intel.xml; arm64 conserva Plugins.xml.
- Main.cpp incorpora --audio-inventory, --check-devices y --probe-plugin (fixture XML, JSON de resultado). La prueba de plugin no abre el editor ni verifica licencia.

## Evidencia observada

Compilaciones Release arm64 e Intel completadas. tests/DeviceTests.cpp, ejecutado mediante --check-devices, produjo 22 PASS: entradas mono 1–18, estéreo 17–18, canal físico 18 como único activo, silencio para entrada desactivada y guardado/recarga del ruteo. Son pruebas con interfaz simulada, no grabación física ni prueba de 18 entradas simultáneas de hardware.

build/device-check/devices.txt contiene resultados; inventory.json registra dispositivos locales (MacBook/iPhone, sin SSL), audio-settings.png muestra los ajustes sin dispositivo abierto. La captura se revisó visualmente. El inventario no abrió ni grabó entradas.

Waves: los WaveShell AU/VST3 V14 instalados son binarios universales arm64+x86_64 de 64 bits. Silente previo era solo arm64. Rosetta ejecuta Intel de 64 bits, no plugins de 32 bits.

CLA-76 (s) AudioUnit 14.12.90 se creó correctamente en arm64 y x86_64, con dos entradas y dos salidas y error vacío. Informes build/waves-native-au.json y waves-intel-au.json. Ambos dicen licenseVerified=false: crear instancia no prueba autorización, editor ni audio. Esto no identifica todavía el fallo que reportó el usuario.

El análisis completo de WaveShell1-VST3 14.12.vst3 no terminó en unos 60 segundos; se terminó únicamente ese proceso de prueba. build/waves-scan.log incluye avisos de clases Objective-C duplicadas entre WaveShell y Bass Slapper. No confundir esos avisos con una causa demostrada ni modificar los binarios de Waves. No se generó waves-native-vst3.xml completo.

Binario universal (31-10): `./scripts/build_universal.sh` → `releases/macOS/Silente-Universal.app`; la casilla «Abrir con Rosetta» de Finder elige la mitad x86_64. CLA-76 AU creado en ambas mitades. Sustituye en la práctica a Silente-Intel.app.

## Pendientes al reanudar

1. Revisar el cambio de audio y completar pruebas de persistencia de dispositivo, grabación mono/estéreo y comportamiento al desconectar/reconectar. No prometer compatibilidad universal: depende del controlador y dispositivo expuesto por Core Audio. Hardware SSL pendiente de conexión posterior.
2. Comprobar 18 pistas/entradas simultáneas en simulación y, cuando sea posible, con hardware. Distinguir límite de E/S (18) del límite existente de buses de plugins (16) y de la mezcla principal estéreo.
3. Diagnosticar un Waves concreto que el usuario use: mismo nombre/versión/formato en Cubase y Silente; abrir editor y procesar señal en arm64/Intel. Investigar el bloqueo del escaneo VST3 por shell; la creación AU ya funciona en ambas arquitecturas. No suponer que Rosetta resuelve la autorización.
4. Revisar catálogo por arquitectura y límites de tiempo; evitar exploraciones masivas sin progreso. Comprobar mensajes de error y que los procesos auxiliares usan la arquitectura correcta si en el futuro se entrega un binario universal.
5. Ejecutar la regresión de idiomas y flujo plugins/cursor, actualizar mapa de arquitectura y documento final, y entregar una versión cerrada. La última entrega cerrada sigue siendo Silente-plugins-cursor-01.app.

## Archivos y comandos

Fuentes autoritativas: la carpeta Silente abierta mediante Silente.code-workspace en Visual Studio Code. Compilación normal: ./scripts/build.sh. Prueba focalizada: build/Silente_artefacts/Release/Silente.app/Contents/MacOS/Silente --check-devices RUTA_ABSOLUTA_DE_RESULTADOS. Intel: ./scripts/build_intel.sh. No ejecutar pruebas automáticamente mientras el usuario mantiene la pausa.

Compilación de desarrollo arm64 copiada a releases/macOS/Silente-audio18-en-desarrollo.app; Intel en releases/macOS/Silente-Intel.app. Son compilaciones de trabajo, no una validación completa. Respaldo de fuentes en backups/checkpoint-audio18-2026-10-29.tar.gz; excluye dependencias y carpetas de compilación, que pueden regenerarse.

## Referencias ya consultadas

- [SSL: compatibilidad de SSL 2/2+ MKI/MKII, Core Audio sin driver adicional](https://support.solidstatelogic.com/hc/en-gb/articles/4408964173073-SSL-2-2-MKI-MKII-Compatibility).
- [JUCE AudioDeviceManager](https://docs.juce.com/master/classjuce_1_1AudioDeviceManager.html); se revisó además el código local de callback Core Audio, que entrega los canales activos compactados.
- [Waves: Apple Silicon](https://www.waves.com/support/waves-with-apple-silicon-processors): V14 y posteriores soportan las arquitecturas nativas indicadas por el fabricante.
- [Waves: demo pese a licencia activada](https://www.waves.com/support/plugins-appear-in-demo-mode-but-my-license-activated). No se han inspeccionado ni cambiado archivos privados de licencia.
