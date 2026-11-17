#include "KeyCommands.h"
#include "../i18n/Translations.h"

namespace silente::app
{
namespace
{
using K = juce::KeyPress;
constexpr int C = juce::ModifierKeys::commandModifier, A = juce::ModifierKeys::altModifier, S = juce::ModifierKeys::shiftModifier;

std::vector<KeyCommand> build()
{
    std::vector<KeyCommand> list;
    juce::String catEs, catEn;
    const auto cat = [&](const char* es, const char* en) { catEs = juce::String::fromUTF8(es); catEn = en; };
    const auto add = [&](const char* id, const char* es, const char* en, int key, int mods = 0)
    { list.push_back({ id, catEs, catEn, juce::String::fromUTF8(es), en, key, mods }); };

    cat("Añadir pista", "Add Track");
    add("addTrack", "Añadir pista", "Add Track", 'T');
    cat("Audio", "Audio");
    add("adjustFades", "Ajustar fundidos al rango", "Adjust Fades to Range", 'A');
    add("", "Auto-rejilla", "Auto Grid", 'Q', S);
    add("crossfade", "Fundido cruzado", "Crossfade", 'X');
    cat("Automatización", "Automation");
    add("", "Abrir panel", "Open Panel", K::F6Key);
    add("", "«Leer automatización» en todas las pistas act./desact.", "Toggle Read Enable All Tracks", 'R', A);
    add("", "«Escribir automatización» en todas las pistas act./desact.", "Toggle Write Enable All Tracks", 'W', A);
    cat("Acordes", "Chords");
    add("", "Mostrar/Ocultar pads de acorde", "Show/Hide Chord Pads", 'C', C | S);
    cat("Dispositivos (estudio)", "Devices (Studio)");
    add("audioConnections", "Conexiones de audio", "Audio Connections", K::F4Key);
    add("", "Monitor de rendimiento de audio", "Audio Performance Monitor", K::F12Key);
    add("mixConsole", "MixConsole", "MixConsole", K::F3Key);
    add("mixConsoleLower", "MixConsole en la ventana de proyecto", "MixConsole in Project Window", K::F3Key, A);
    add("", "Teclado en pantalla", "On-Screen Keyboard", 'K', A);
    add("", "Reproductor de video", "Video Player", K::F8Key);
    add("vstInstruments", "Instrumentos VST", "VST Instruments", K::F11Key);
    cat("Procesado offline directo", "Direct Offline Processing");
    add("", "Procesado offline directo", "Direct Offline Processing", K::F7Key);
    cat("Edición", "Edit");
    add("", "Activar/Desactivar objeto activo", "Activate/Deactivate Focused Object", 'A', A);
    add("autoScroll", "Auto-desplazamiento act./desact.", "Auto-Scroll On/Off", 'F');
    add("copy", "Copiar", "Copy", 'C', C);
    add("cut", "Cortar", "Cut", 'X', C);
    add("", "Cortar intervalo de tiempo", "Cut Time", 'X', C | S);
    add("delete", "Suprimir", "Delete", K::backspaceKey);
    add("delete", "Suprimir (Supr)", "Delete (Del)", K::deleteKey);
    add("deleteTime", "Eliminar intervalo de tiempo", "Delete Time", K::backspaceKey, S);
    add("duplicate", "Duplicar", "Duplicate", 'D', C);
    add("enterTempo", "Introducir tempo", "Enter Tempo", 'T', S);
    add("", "Introducir tipo de compás", "Enter Time Signature", 'C', S);
    add("findTrack", "Buscar pista/canal", "Find Track/Channel", 'F', C);
    add("", "Grupo", "Group", 'G', C);
    add("", "Edición en grupo en pistas seleccionadas act./desact.", "Group Editing of Selected Tracks On/Off", 'K');
    add("insertSilence", "Insertar silencio", "Insert Silence", 'E', C | S);
    add("", "Invertir", "Invert", 'F', A);
    add("", "Bloquear", "Lock", 'L', C | S);
    add("moveToCursor", "Mover inicio de evento/inicio de rango a cursor", "Move Event Start to Cursor", 'L', C);
    add("muteTracks", "Enmudecer", "Mute", 'M');
    add("", "Enmudecer eventos", "Mute Events", 'M', S);
    add("open", "Abrir", "Open", 'E', C);
    add("paste", "Pegar", "Paste", 'V', C);
    add("pasteOrigin", "Pegar al origen", "Paste at Origin", 'V', A);
    add("recordEnable", "Habilitar grabación", "Record Enable", 'R');
    add("redo", "Rehacer", "Redo", 'Z', C | S);
    add("renameTrack", "Renombrar primera pista seleccionada", "Rename First Selected Track", K::F2Key, A);
    add("renameEvent", "Renombrar eventos seleccionados", "Rename Selected Events", K::F2Key);
    add("repeat", "Repetir", "Repeat", 'K', C);
    add("", "Seleccionar todo", "Select All", 'A', C);
    add("", "Anular selección", "Deselect All", 'A', C | S);
    add("snap", "Ajustar act./desact.", "Snap On/Off", 'J');
    add("soloTracks", "Solo", "Solo", 'S');
    add("splitAtCursor", "Dividir en el cursor", "Split at Cursor", 'X', A);
    add("splitRange", "Dividir rango", "Split Range", 'X', S);
    add("stationary", "Cursor estacionario", "Stationary Cursor", 'C', A);
    add("undo", "Deshacer", "Undo", 'Z', C);
    add("", "Desagrupar", "Ungroup", 'U', C);
    add("", "Escribir", "Write", 'W');
    cat("Editores", "Editors");
    add("openEditor", "Abrir/Cerrar editor", "Open/Close Editor", K::returnKey);
    cat("Archivo", "File");
    add("new", "Nuevo", "New", 'N', C);
    add("openProject", "Abrir", "Open", 'O', C);
    add("save", "Guardar", "Save", 'S', C);
    add("saveAs", "Guardar como", "Save As", 'S', C | S);
    add("import", "Importar archivo de audio", "Import Audio File", 'I', C | A);
    add("exportMidi", "Exportar MIDI", "Export MIDI", 0);
    add("exportMidiSelected", "Exportar MIDI de la pista seleccionada", "Export Selected Track as MIDI", 0);
    cat("Foco", "Focus");
    add("", "Siguiente", "Next", K::tabKey);
    add("", "Anterior", "Previous", K::tabKey, S);
    cat("Marcador", "Marker");
    add("addCycleMarker", "Añadir marcador de ciclo en pista activa", "Insert Cycle Marker", 0);
    add("addMarker", "Añadir marcador de posición en pista activa", "Insert Marker", K::insertKey);
    cat("Medios", "Media");
    add("", "Abrir MediaBay", "Open MediaBay", K::F5Key);
    cat("Navegar", "Navigate");
    add("trackDown", "Abajo: seleccionar siguiente pista", "Down: Select Next Track", K::downKey);
    add("trackUp", "Arriba: seleccionar pista anterior", "Up: Select Previous Track", K::upKey);
    add("eventLeft", "Izquierda: seleccionar evento anterior", "Left: Select Previous Event", K::leftKey);
    add("eventRight", "Derecha: seleccionar evento siguiente", "Right: Select Next Event", K::rightKey);
    add("trackTop", "Superior: primera pista", "Top: First Track", K::homeKey);
    add("trackBottom", "Inferior: última pista", "Bottom: Last Track", K::endKey);
    cat("Empujar", "Nudge");
    add("nudgeLeft", "Izquierda", "Left", K::leftKey, C);
    add("nudgeRight", "Derecha", "Right", K::rightKey, C);
    add("nudgeStartLeft", "Empujar inicio hacia la izquierda", "Trim Start Left", K::leftKey, A);
    add("nudgeStartRight", "Empujar inicio hacia la derecha", "Trim Start Right", K::rightKey, A);
    add("nudgeEndLeft", "Empujar final hacia la izquierda", "Trim End Left", K::leftKey, A | S);
    add("nudgeEndRight", "Empujar final hacia la derecha", "Trim End Right", K::rightKey, A | S);
    cat("Proyecto", "Project");
    add("colour", "Ajustar color de pista/evento", "Set Track/Event Color", 'C', A | S);
    add("deleteTracks", "Eliminar pistas seleccionadas", "Remove Selected Tracks", K::deleteKey, S);
    add("tapTempo", "Marcar tempo", "Tap Tempo", K::spaceKey, S);
    add("", "Abrir Pool", "Open Pool", 'P', C);
    add("", "Abrir marcadores", "Open Markers", 'M', C);
    add("", "Configuración", "Project Setup", 'S', S);
    cat("Cuantizar", "Quantize");
    add("quantize", "Cuantizar", "Quantize", 'Q');
    cat("Herramientas", "Tools");
    // Complete Tool category from Cubase 15.0.30's command catalog. A zero key
    // means unassigned, not the '0' key. A listed ID does not imply an action.
    add("toolNext", "Herramienta siguiente", "Next Tool", K::F10Key);
    add("toolPrevious", "Herramienta anterior", "Previous Tool", K::F9Key);
    add("toolSelect", "Herramienta de selección", "Object Selection Tool", '1');
    add("toolSelectNormal", "Seleccionar: redimensionado normal", "Object Selection Tool: Normal Sizing", 0);
    add("toolSelectMoveContents", "Seleccionar: redimensionar moviendo contenido", "Object Selection Tool: Sizing Moves Content", 0);
    add("toolSelectTimeStretch", "Seleccionar: redimensionar con corrección de tiempo", "Object Selection Tool: Sizing Applies Time Stretch", 0);
    add("toolRange", "Herramienta de selección de rango", "Range Selection Tool", '2');
    add("toolSplit", "Herramienta dividir", "Split Tool", '3');
    add("toolGlue", "Herramienta pegar", "Glue Tool", '4');
    add("toolErase", "Herramienta borrar", "Erase Tool", '5');
    add("toolZoom", "Herramienta zoom", "Zoom Tool", '6');
    add("toolMute", "Herramienta enmudecer", "Mute Tool", '7');
    add("toolDraw", "Herramienta dibujar", "Draw Tool", '8');
    add("toolPlay", "Herramienta reproducir", "Play Tool", '9');
    add("toolDrumstick", "Herramienta baqueta", "Drumstick Tool", '0');
    add("toolScrub", "Herramienta arrastrar", "Scrub Tool", 0);
    add("toolTrim", "Herramienta recortar", "Trim Tool", 0);
    add("toolColour", "Herramienta color", "Color Tool", 0);
    add("toolHitpoint", "Herramienta hitpoints", "Hitpoint Tool", 0);
    add("toolCurve", "Herramienta curva", "Curve Tool", 0);
    add("toolCurveLine", "Curva: línea", "Curve Tool: Line", 0);
    add("toolCurveParabola", "Curva: parábola", "Curve Tool: Parabola", 0);
    add("toolCurveSine", "Curva: seno", "Curve Tool: Sine", 0);
    add("toolCurveTriangle", "Curva: triángulo", "Curve Tool: Triangle", 0);
    add("toolCurveSquare", "Curva: cuadrado", "Curve Tool: Square", 0);
    add("toolCurvePaint", "Curva: pintar", "Curve Tool: Paint", 0);
    add("toolSlot1", "Herramienta 1", "Tool 1", 0);
    add("toolSlot2", "Herramienta 2", "Tool 2", 0);
    add("toolSlot3", "Herramienta 3", "Tool 3", 0);
    add("toolSlot4", "Herramienta 4", "Tool 4", 0);
    add("toolSlot5", "Herramienta 5", "Tool 5", 0);
    add("toolSlot6", "Herramienta 6", "Tool 6", 0);
    add("toolSlot7", "Herramienta 7", "Tool 7", 0);
    add("toolSlot8", "Herramienta 8", "Tool 8", 0);
    add("toolSlot9", "Herramienta 9", "Tool 9", 0);
    add("toolSlot10", "Herramienta 10", "Tool 10", 0);
    add("toolPopup", "Caja de herramientas emergente", "Popup Tools", 0);
    add("toolFreeWarp", "Herramienta Free Warp", "Free Warp Tool", 0);
    add("toolAudioTempoDefinition", "Herramienta de definición de tempo de audio", "Audio Tempo Definition Tool", 0);
    add("toolComp", "Herramienta Comp", "Comp Tool", 0);
    add("toolTimeWarp", "Herramienta Time Warp", "TimeWarp Tool", 0);
    add("toolPitchWarp", "Herramienta de edición de tono/Warp", "Edit Pitch/Warp Tool", 0);
    add("combineSelectionTools", "Combinar herramientas de selección act./desact.", "Combine Selection Tools On/Off", '1', A | S);
    cat("Transporte", "Transport");
    add("metronome", "Activar metrónomo", "Metronome On", 'C');
    add("punchIn", "Activar punch in", "Activate Punch In", 'I');
    add("punchOut", "Activar punch out", "Activate Punch Out", 'O');
    add("cycle", "Ciclo", "Cycle", K::numberPadDivide);
    add("enterLeft", "Introducir localizador izquierdo", "Input Left Locator", 'L', S);
    add("enterRight", "Introducir localizador derecho", "Input Right Locator", 'R', S);
    add("enterPosition", "Introducir posición del cursor del proyecto", "Input Position", 'P', S);
    add("enterRange", "Introducir duración de rango de localizadores", "Input Locator Range Duration", 'D', S);
    add("playSelection", "Reproducir rango de selección", "Play Selection Range", K::spaceKey, A);
    add("forward", "Avanzar", "Forward", K::numberPadAdd);
    add("rewind", "Rebobinar", "Rewind", K::numberPadSubtract);
    add("goStart", "Ir al inicio del proyecto", "Return to Start Position", K::numberPadDecimalPoint);
    add("goLeft", "Ir al localizador izquierdo", "To Left Locator", K::numberPad1);
    add("goRight", "Ir al localizador derecho", "To Right Locator", K::numberPad2);
    add("nextEvent", "Ir al evento siguiente", "Locate Next Event", 'N');
    add("previousEvent", "Ir al evento anterior", "Locate Previous Event", 'B');
    add("selectionStart", "Ir al inicio de la selección", "Locate Selection Start", 'L');
    add("locatorsToSelection", "Localizadores a la selección", "Locators to Selection", 'P');
    add("loopSelection", "Reproducir selección en bucle", "Loop Selection", 'P', A);
    add("record", "Grabar", "Record", K::numberPadMultiply);
    add("setLeft", "Establecer localizador izquierdo a posición del cursor", "Set Left Locator", K::numberPad1, C);
    add("setRight", "Establecer localizador derecho a posición del cursor", "Set Right Locator", K::numberPad2, C);
    add("startStop", "Iniciar/Detener", "Start/Stop", K::spaceKey);
    add("stop", "Detener", "Stop", K::numberPad0);
    cat("Zonas de ventana", "Window Zones");
    add("zoneLeft", "Mostrar/Ocultar zona izquierda", "Show/Hide Left Zone", 'L', C | A);
    add("zoneChannel", "Mostrar/Ocultar canal", "Show/Hide Channel", 'C', C | A);
    add("zoneRight", "Mostrar/Ocultar zona derecha", "Show/Hide Right Zone", 'R', C | A);
    add("zoneLower", "Mostrar/Ocultar zona inferior", "Show/Hide Lower Zone", 'E', C | A);
    add("zoneTransport", "Mostrar/Ocultar zona de transporte", "Show/Hide Transport Zone", 'T', C | A);
    add("infoLine", "Mostrar/Ocultar línea de información", "Show/Hide Info Line", 'I', C);
    cat("Zoom", "Zoom");
    add("zoomFull", "Alejar al máximo", "Zoom Full", 'F', S);
    add("zoomIn", "Acercar zoom", "Zoom In", 'H');
    add("zoomOut", "Alejar zoom", "Zoom Out", 'G');
    add("zoomInVertical", "Ampliar zoom vertical", "Zoom In Vertically", 'H', S);
    add("zoomOutVertical", "Reducir zoom vertical", "Zoom Out Vertically", 'G', S);
    add("zoomSelection", "Zoom a la selección", "Zoom to Selection", 'S', A);
    return list;
}

int normalised(int code) { return code >= 'a' && code <= 'z' ? code - 'a' + 'A' : code; }
int relevantModifiers(const juce::ModifierKeys& m)
{
    return (m.isCommandDown() ? C : 0) | (m.isAltDown() ? A : 0) | (m.isShiftDown() ? S : 0);
}
}

juce::String KeyCommand::category() const { return i18n::language() == i18n::Language::english ? categoryEn : categoryEs; }
juce::String KeyCommand::name() const { return i18n::language() == i18n::Language::english ? nameEn : nameEs; }

const std::vector<KeyCommand>& keyCommands()
{
    static const auto list = build();
    return list;
}

const KeyCommand* findKeyCommand(const juce::KeyPress& press)
{
    int code = normalised(press.getKeyCode());
    int mods = relevantModifiers(press.getModifiers());
    // Laptop keyboards: the keypad commands also answer to the main-keyboard symbols.
    if (code == '/') code = K::numberPadDivide;
    else if (code == '*') { code = K::numberPadMultiply; mods &= ~S; }
    else if (code == '+') { code = K::numberPadAdd; mods &= ~S; }
    else if (code == '-' && mods == 0) code = K::numberPadSubtract;
    for (const auto& c : keyCommands())
        if (c.keyCode != 0 && c.keyCode == code && c.modifiers == mods) return &c;
    return nullptr;
}

juce::String describeKey(const KeyCommand& c)
{
    if(c.keyCode==0)return i18n::tr("Sin asignación");
    juce::String text;
    if (c.modifiers & C) text << juce::String::fromUTF8("⌘");
    if (c.modifiers & A) text << juce::String::fromUTF8("⌥");
    if (c.modifiers & S) text << juce::String::fromUTF8("⇧");
    const std::pair<int, const char*> keypad[] { { K::numberPadDivide, "Num /" }, { K::numberPadMultiply, "Num *" }, { K::numberPadAdd, "Num +" },
        { K::numberPadSubtract, "Num -" }, { K::numberPadDecimalPoint, "Num ." }, { K::numberPad0, "Num 0" }, { K::numberPad1, "Num 1" }, { K::numberPad2, "Num 2" } };
    for (const auto& [code, label] : keypad) if (c.keyCode == code) return text + label;
    return text + juce::KeyPress(c.keyCode).getTextDescription().toUpperCase();
}
}
