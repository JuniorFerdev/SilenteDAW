# Silente — instrucciones compartidas

Proyecto independiente de TopoVía y de los documentos académicos de carpetas vecinas. C++20, JUCE, CMake; editor Visual Studio Code.

## Continuidad y alcance

- Leer `ETAPAS.md` para conocer el estado y la etapa activa. Consultar `docs/ARQUITECTURA.md` solo para el módulo pertinente.
- Una tarea y una entrega comprobable por tramo. No iniciar otra etapa automáticamente.
- Pedido de Fernando (04-11-2026): anunciar la función concreta antes de desarrollarla y dar avances durante la ejecución. Para replicar una interacción visual, inspeccionar capturas reales y registrar la referencia; no deducir el diseño solo del texto. Mantener una checklist explícita de todas las mejoras solicitadas, con dependencias y pendientes, sin presentar una base parcial como una función completa.
- No usar subagentes salvo autorización explícita del usuario para el tramo actual. Fernando autorizó (16-11-2026) los tres de `.claude/agents/`: `silente-escriba` (Haiku: lee, toma notas, actualiza .md y reparte), `silente-programador` (Sonnet: código acotado, pruebas, errores) y `silente-arquitecto` (Opus: diseño y cambios entre módulos). Solo la sesión principal empaqueta, borra y hace commits.
- Silente tiene repositorio git local propio (rama main, sin remoto). Hacer commit al cerrar cada tramo y antes de scripts que reescriban archivos; autor Luis Fernando Muñoz Bolaños <fermubol@gmail.com>.
- Conservar todo el código fuente en este proyecto. No depender del historial de un chat para entender decisiones o compilar.
- No leer build/, .venv/, third_party/ ni manuales completos de forma rutinaria. Buscar y leer solo archivos pertinentes.
- Al entregar: cambio concreto, verificación observada, limitaciones y próximo paso pendiente en pocas líneas. Actualizar el estado existente; no duplicarlo.
- Pedido de Fernando (05-11-2026): actualizar la aplicación al cerrar cambios verificados y mantener una sola app Silente ejecutable en `releases/macOS/Silente-Universal.app`. Reemplazar y borrar variantes anteriores y artefactos de pruebas después de validarlos. **No crear copias de respaldo**: Fernando canceló expresamente esa política para ahorrar disco. Conservar fuentes y proyectos musicales; organizar módulos para Visual Studio Code.

## Arquitectura y verificación

- Separar interfaz, comandos de aplicación, modelo de proyecto, audio, MIDI, plugins y persistencia. El mapa objetivo está en docs/ARQUITECTURA.md; su existencia no significa que la separación esté implementada.
- La interfaz manda comandos y muestra estado. No procesar muestras ni alojar algoritmos DSP en componentes visuales.
- La mezcladora visual y el mezclador de audio son módulos diferentes. Colores, tipografía e iconos pertenecen al tema visual.
- Evitar asignaciones, E/S de archivos y bloqueos del hilo de audio; registrar limitaciones existentes y comprobar correcciones.
- Refactorizar por extracciones pequeñas, conservar comportamiento y compilar después de cada extracción. No reescribir todo a la vez.
- `./scripts/build.sh` compila. Que compile o abra no prueba audio, grabación, MIDI ni compatibilidad de plugins. Las pruebas deben producir evidencia antes de declararse aprobadas.
- Mantener la distribución y hábitos de Cubase como referencia UX, con identidad Silente y recursos propios. No tocar activaciones o instalaciones de software ajeno.
- Al terminar la entrega: `python3 scripts/clean_generated_apps.py --apply` retira las apps temporales y actualiza Launch Services; no crea respaldos. Para volver a desarrollar después, reconfigurar primero con `scripts/build.sh` (el empaquetado temporal fue retirado).
- Referencias de Cubase: consultar el manual y fuentes oficiales de internet. Fernando indicó el 05-11-2026 que su instalación no tiene licencia; no intentar abrir Cubase para validar funciones.
- Interfaz bilingüe español/inglés: textos nuevos mediante i18n::tr y ambos catálogos en resources/locales/. No traducir nombres o datos del usuario. Comprobar con ./scripts/check_locales.sh.
