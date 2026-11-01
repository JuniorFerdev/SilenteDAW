# Silente · español e inglés · 29-10-2026

La misma aplicación incluye ambos idiomas. Español es el valor inicial; el botón superior Español/English y el menú Idioma permiten cambiarlo sin reiniciar. La selección se conserva para la siguiente apertura en language.json dentro de la carpeta de preferencias de Silente.

Menús, controles, ayudas, mensajes, ajustes y diálogos propios usan el idioma elegido. Los nombres escritos por el usuario, proyectos y datos musicales se conservan. Los editores y metadatos de plugins externos dependen de su fabricante; los mensajes del sistema operativo pueden seguir el idioma de macOS. Los selectores de archivos de Silente usan la interfaz JUCE traducida.

## Código separado

- resources/locales/es.json y en.json: 315 entradas por idioma, incluidas traducciones de controles JUCE.
- src/i18n/Translations.h/.cpp: carga, búsqueda, selección, preferencia y traducciones JUCE.
- src/i18n/UiTranslation.h/.cpp: actualización de botones y ayudas existentes.
- resources/macos/es.lproj e en.lproj: mensajes localizados de permiso de micrófono.
- tests/LocalisationUiTests.cpp: prueba de integración de interfaz.
- scripts/check_locales.sh: comprobación de catálogos, compilación y prueba de cambio de idioma. También disponible como tarea de Visual Studio Code: Silente: comprobar idiomas.

## Verificación observada

Compilación Release completada. Cobertura de las llamadas literales a i18n::tr y coincidencia de las 315 claves comprobadas. Búsquedas, traducciones JUCE y guardado/recarga de preferencia comprobados en un directorio aislado, sin cambiar la preferencia personal.

Prueba ES → EN → ES aprobada: cambian los controles y se conservan nombre de proyecto, nombre de pista, color, notas, tempo y cantidad de pistas. Capturas generadas por la propia aplicación en ambos idiomas. Registros: build/i18n-check/localisation.txt y language-switch.txt. La interacción manual con el botón y los menús queda para revisión del usuario; la prueba automatizada llama al mismo servicio de cambio de idioma.

## Entrega y revisión

Abrir releases/macOS/Silente-es-en-01.app, cerrando antes una versión anterior si está abierta. Seleccionar English y luego Español; cerrar y abrir para revisar la preferencia. Capturas en releases/macOS/Silente-es.png y Silente-en.png.

Esta etapa comprueba la localización; no añade validación de audio, grabación ni compatibilidad de plugins. La separación general del motor y las vistas sigue pendiente según ARQUITECTURA.md.
