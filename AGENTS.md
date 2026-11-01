# Silente — instrucciones compartidas

Proyecto independiente de TopoVía y de los documentos académicos de carpetas vecinas. C++20, JUCE, CMake; editor Visual Studio Code.

## Continuidad y alcance

- Leer `ETAPAS.md` para conocer el estado y la etapa activa. Consultar `docs/ARQUITECTURA.md` solo para el módulo pertinente.
- Una tarea y una entrega comprobable por tramo. No iniciar otra etapa automáticamente.
- No usar subagentes salvo autorización explícita del usuario para el tramo actual.
- Conservar todo el código fuente en este proyecto. No depender del historial de un chat para entender decisiones o compilar.
- No leer build/, .venv/, third_party/ ni manuales completos de forma rutinaria. Buscar y leer solo archivos pertinentes.
- Al entregar: cambio concreto, verificación observada, limitaciones y próximo paso pendiente en pocas líneas. Actualizar el estado existente; no duplicarlo.

## Arquitectura y verificación

- Separar interfaz, comandos de aplicación, modelo de proyecto, audio, MIDI, plugins y persistencia. El mapa objetivo está en docs/ARQUITECTURA.md; su existencia no significa que la separación esté implementada.
- La interfaz manda comandos y muestra estado. No procesar muestras ni alojar algoritmos DSP en componentes visuales.
- La mezcladora visual y el mezclador de audio son módulos diferentes. Colores, tipografía e iconos pertenecen al tema visual.
- Evitar asignaciones, E/S de archivos y bloqueos del hilo de audio; registrar limitaciones existentes y comprobar correcciones.
- Refactorizar por extracciones pequeñas, conservar comportamiento y compilar después de cada extracción. No reescribir todo a la vez.
- `./scripts/build.sh` compila. Que compile o abra no prueba audio, grabación, MIDI ni compatibilidad de plugins. Las pruebas deben producir evidencia antes de declararse aprobadas.
- Mantener la distribución y hábitos de Cubase como referencia UX, con identidad Silente y recursos propios. No tocar activaciones o instalaciones de software ajeno.
- Interfaz bilingüe español/inglés: textos nuevos mediante i18n::tr y ambos catálogos en resources/locales/. No traducir nombres o datos del usuario. Comprobar con ./scripts/check_locales.sh.
