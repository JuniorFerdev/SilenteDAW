# Referencia · MixConsole de Cubase Pro 15 (para Silente)

Fuente: manual oficial en español, capítulo MixConsole —
https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_c.html
(consultado el 31-10-2026). Aquí se guarda el **mapa** y un **resumen propio**, no el texto del manual:
para el detalle de cada función, abrir su enlace. Silente replica distribución y hábitos de
trabajo, con identidad y gráficos propios (AGENTS.md).

## Mapa de la ventana (resumen)

- **Apertura:** F3 abre MixConsole en ventana propia; Alt/Opción-F3 lo muestra en la zona inferior
  de la ventana de proyecto. Ambas vistas tienen visibilidad de canales independiente.
- **Barra de herramientas:** deshacer/rehacer de parámetros, instantáneas (guardar, menú),
  buscar canal, configuraciones de visibilidad, agentes de visibilidad, filtro por tipo de
  canal, enlazar canales, disposición de ventana, localizadores y funciones.
- **Zona izquierda** (ocultable): pestañas Visibilidad (Canal / Zonas: anclar canales a izquierda
  o derecha), Historial (cambios de parámetros) e Instantáneas.
- **Zona derecha** (ocultable): Control Room, Medidor, Automatización.
- **Disposición de ventana:** vista general de canales, panel de medidores, zonas, nombre de canal,
  curva de EQ, secciones de canal, imágenes, bloc de notas, latencia, fader.
- **Secciones de canal (racks), de arriba abajo:** Hardware, Enrutado (entrada/salida, grupos),
  Pre (filtros HP/LP, ganancia de entrada, fase), Moduladores, Inserciones (pre/post fader, bypass,
  side-chain, presets de cadena), EQ (4 bandas, curva, presets), Strip (puerta, compresor, EQ,
  herramientas, saturación, limitador), Envíos (a canales FX, pre/post, bypass), Envíos cue,
  Enrutado directo, Controles rápidos y Paneles de dispositivo. Se pliegan/expanden; opción
  «expandir solo una sección a la vez».
- **Sección de faders:** todos los canales en el orden de la lista de pistas (audio, instrumento,
  MIDI, grupo, FX, VCA, entrada/salida). Por canal: enlace de grupo, panorama (balance o combinado,
  bypass), Mute/Solo/Escuchar, botón «e» de configuración de canal, volumen, lectura/escritura de
  automatización, nivel de entrada, medidor con indicador de clip y retención de pico.
- **Otros:** grupos enlazados y Quick Link, reordenar canales, colores, imágenes y notas por canal,
  latencias, configuración de canal (ventana «e»), comparación de canales, navegación por teclado.

## Estado en Silente (31-10-2026)

Existe un «Mezclador» en la zona inferior: un canal por pista + salida estéreo, con dos ranuras de
efecto, insertar, M/S, panorama, fader y medidor; inspector con EQ de 3 bandas y 4 inserciones; la
pista de carpeta no tiene canal. Faltan: ventana MixConsole (F3) propia, racks, envíos y ruteo a
FX/grupo en el motor, instantáneas, historial, visibilidad/zonas, enlaces, medidores con pico/clip.

**31-10-2026 · mixconsole02 hecho:** salida a grupo y envíos pre/post a FX en el motor y en la ventana.
**31-10-2026 · mixconsole01 hecho:** ventana F3, barra, zona izquierda de visibilidad/zonas y sección de faders (ver ETAPAS.md).

## Tramos propuestos (uno por entrega, en este orden)

1. **mixconsole01 · ventana y faders:** ventana F3 con barra de herramientas, zona izquierda
   (Visibilidad con mostrar/ocultar y anclar), sección de faders al estilo Cubase (nombre, color,
   tipo, panorama, M/S, «e», fader en dB con escala, medidor con retención de pico y clip,
   lectura numérica), racks Enrutado/Inserciones/EQ plegables con lo que el motor ya tiene.
2. **mixconsole02 · enrutado real:** salida de pista hacia grupo y envíos pre/post hacia FX en el
   motor, con pruebas de señal.
3. **mixconsole03 · historial e instantáneas.**
4. **mixconsole04 · Pre (filtros, ganancia, fase) y EQ de 4 bandas con curva.**
5. **mixconsole05 · Strip (puerta, compresor, limitador) y grupos enlazados.**

## Índice del capítulo (enlaces)

- [MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_c.html)
- [MixConsole en zona inferior](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_in_lower_zone_r.html)
- [Mostrar/Ocultar canales de MixConsole en la zona inferior](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_in_lower_zone_showing_hiding_channels_t.html)
- [Anclar canales de MixConsole a la izquierda/derecha en la zona inferior](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_in_lower_zone_determining_channel_order_t.html)
- [Ventana de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_mixconsole_window_r.html)
- [Configurar panel de disposición de ventanas](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_set_up_window_layout_pane_r.html)
- [Zona izquierda de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_selector_r.html)
- [Visibilidad de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_visibility_c.html)
- [Sincronizar la visibilidad de pista y canal](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_synchronizing_channel_and_track_visibility_t.html)
- [Zonas de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_zones_c.html)
- [Historial de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_history_c.html)
- [Instantáneas de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_snapshots_c.html)
- [Barra de herramientas de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_mixconsole_toolbar_r.html)
- [Buscar canales](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_finding_channels_t.html)
- [Deshacer/Rehacer cambios de parámetros de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_undo_redo_parameter_c.html)
- [Guardar instantáneas de ajustes de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_snapshots_saving_t.html)
- [Renombrar instantáneas de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_snapshots_renaming_t.html)
- [Actualizar una instantánea de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_updating_a_snapshot_t.html)
- [Recuperar instantáneas (pestaña Instantáneas)](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_snapshots_applying_t.html)
- [Recuperar instantáneas (barra de herramientas)](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_snapshots_from_the_mixconsole_toolbar_recalling_t.html)
- [Añadir notas a instantáneas](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_notes_adding_to_snapshots_t.html)
- [Ajustes de recuperación de instantánea](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_snapshot_settings_r.html)
- [Filtrar tipos de canales](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_types_filtering_t.html)
- [Menú Configuraciones de visibilidad de canales](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_visibility_configurations_r.html)
- [Guardar configuraciones](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_saving_configurations_t.html)
- [Agentes de visibilidad de canal](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_visibility_agents_c.html)
- [Menú Agentes de visibilidad de canal](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_showing_channels_with_specific_properties_r.html)
- [Deshacer/Rehacer cambios de visibilidad](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_undoing_visibility_changes_t.html)
- [Enlazar canales](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_linking_channels_c.html)
- [Diálogo Ajustes de grupo enlazado](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_link_group_settings_r.html)
- [Línea de visualización](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_display_line_r.html)
- [Crear grupos enlazados](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_creating_a_linked_group_t.html)
- [Cambiar los ajustes de grupo enlazado](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_changing_link_settings_c.html)
- [Añadir canales a grupos enlazados](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channels_adding_to_link_groups_t.html)
- [Eliminar canales de grupos enlazados](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channels_removing_from_link_groups_t.html)
- [Usar Quick Link (enlace rápido)](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_using_quick_link_t.html)
- [Menú Funciones](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_functions_menu_r.html)
- [Guardar ajustes de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_saving_and_loading_mixconsole_settings_t.html)
- [Cargar ajustes de MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_loading_mixconsole_settings_t.html)
- [Sección de faders](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_fader_section_c.html)
- [Control de panorama](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_setting_pan_c.html)
- [Panoramizador de balance estéreo](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_stereo_balance_panner_c.html)
- [Panoramizador combinado estéreo](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_stereo_combined_panner_c.html)
- [Omitir panorama](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_panning_bypass_c.html)
- [Usar Solo y Enmudecer](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_using_solo_and_mute_c.html)
- [Modo escuchar](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_listen_mode_c.html)
- [Volumen](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_setting_volume_c.html)
- [Menú Configuración de medidores](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_meter_options_r.html)
- [Configurar colores de medidores](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_meter_colors_setting_up_t.html)
- [Medidores de nivel](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_level_meters_c.html)
- [Niveles de entrada](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_input_levels_c.html)
- [Ajustar niveles de entrada](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_input_levels_setting_t.html)
- [Clipping](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_clipping_c.html)
- [Secciones de canal](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_working_with_channel_racks_c.html)
- [Copiar y mover configuraciones de secciones y canales](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_copying_and_moving_rack_and_channel_settings_c.html)
- [Sección Enrutado](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_routing_c.html)
- [Configurar enrutado](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_routing_setting_up_t.html)
- [Buses de entrada](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_input_busses_c.html)
- [Enrutado de entrada para canales mono](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_input_routing_configurations_for_mono_channels_c.html)
- [Enrutado de entrada para canales estéreo](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_input_routing_configurations_for_stereo_channels_c.html)
- [Enrutado de entrada para canales surround](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_input_routing_configurations_for_surround_channels_c.html)
- [Buses de salida](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_output_busses_c.html)
- [Usar canales de grupo](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_using_group_channels_t.html)
- [Sección Pre](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_pre_filtersgainphase_c.html)
- [Hacer ajustes de filtrado](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_making_filter_settings_t.html)
- [Hacer ajustes de ganancia de entrada](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_making_input_gain_settings_t.html)
- [Hacer ajustes de fase](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_making_phase_settings_t.html)
- [Opciones de visualización de pre](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_view_options_pre_r.html)
- [Sección Moduladores](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_modulators_c.html)
- [Añadir moduladores](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_adding_modulators_t.html)
- [Omitir moduladores](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_bypassing_modulators_c.html)
- [Opciones de visualización de moduladores](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_view_options_modulators_r.html)
- [Sección Inserciones](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_inserts_c.html)
- [Añadir efectos de inserción](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_adding_insert_effects_t.html)
- [Cambiar el número de ranuras pre-fader/post-fader](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_moving_inserts_to_postfader_or_prefader_position_t.html)
- [Bypass de efectos de inserción](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_bypassing_insert_effects_c.html)
- [Activar side-chaining en inserciones](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_activating_sidechain_for_inserts_t.html)
- [Guardar/Cargar presets de cadena de FX](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_saving_loading_fx_chain_presets_t.html)
- [Opciones de visualización de inserciones](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_view_options_inserts_r.html)
- [Sección Ecualizador](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_equalizers_c.html)
- [Activar bandas del ecualizador](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_activating_equalizer_bands_t.html)
- [Ajustes de ecualizador](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_equalizer_settings_r.html)
- [Guardar/Cargar presets de EQ](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_saving_loading_eq_presets_t.html)
- [Sección Strip](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_strip_modules_r.html)
- [Puerta de ruido](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_gate_r.html)
- [Compresor](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_compressor_r.html)
- [Standard Compressor](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_standard_compressor_r.html)
- [Tube Compressor](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_tube_compressor_r.html)
- [VintageCompressor](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_vintage_compressor_r.html)
- [EQ](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_eq_r.html)
- [Herramientas](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_trans_r.html)
- [DeEsser](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_deesser_r.html)
- [EnvelopeShaper](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_envelopeshaper_r.html)
- [Sat](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_sat_r.html)
- [Magneto II](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_magneto_ii_r.html)
- [Tape Saturation](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_tape_saturation_r.html)
- [Tube Saturation](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_tube_saturation_r.html)
- [Limitador](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_limit_r.html)
- [Brickwall Limiter](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_brickwall_limiter_r.html)
- [Maximizer](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_maximizer_r.html)
- [Standard Limiter](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_standard_limiter_r.html)
- [Guardar/Cargar presets de strip](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_saving_loading_strip_presets_t.html)
- [Opciones de visualización de módulos de strip](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_view_options_strip_r.html)
- [Sección Envíos](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_sends_c.html)
- [Añadir efectos de envío](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_adding_send_effects_t.html)
- [Bypass de efectos de envío](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_bypassing_send_effects_c.html)
- [Añadir pistas de canal de efecto a un efecto de envío](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_adding_fx_channels_to_a_send_t.html)
- [Opciones de visualización de efectos de envío](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_view_options_sends_r.html)
- [Sección Envíos cue](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_cue_sends_c.html)
- [Añadir envíos cue](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_adding_cue_sends_t.html)
- [Sección Enrutado directo](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_direct_routing_c.html)
- [Configurar Enrutado directo](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_setting_up_direct_routing_t.html)
- [Automatizar cambios de destinos](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_automating_destination_switches_t.html)
- [Alimentar señales a múltiples destinos](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_feeding_signals_to_multiple_destinations_t.html)
- [Mezcla automática (downmix)](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_automatic_downmixing_c.html)
- [Sección Controles rápidos de pista](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_track_quick_controls_c.html)
- [Añadir controles rápidos de pista](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_adding_track_quick_controls_t.html)
- [Opciones de visualización de controles rápidos](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_view_options_quick_controls_r.html)
- [Sección Paneles de dispositivo](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_device_panels_c.html)
- [Reordenar canales](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channels_reordering_t.html)
- [Añadir imágenes a canales](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_adding_pictures_to_mixconsole_channels_t.html)
- [Añadir notas a un canal](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_adding_notes_to_a_mixconsole_channel_t.html)
- [Aplicar colores de canal a controles de canal](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_applying_channel_colors_to_channel_controls_t.html)
- [Vista general de latencia de canal](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_latencies_r.html)
- [Configuraciones de canal](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_using_channel_settings_c.html)
- [Barra de herramientas de configuraciones de canal](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_toolbar_r.html)
- [Configuraciones de canal – Inserciones](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_inserts_section_r.html)
- [Configuraciones de canal – Channel strip](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_channel_strip_equalizer_r.html)
- [Standard Compressor – Vista Detalles](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_standard_compressor_details_r.html)
- [Tube Compressor – Vista Detalles](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_tube_compressor_details_r.html)
- [VintageCompressor – Vista Detalles](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_vintage_compressor_details_r.html)
- [Maximizer – Vista Detalles](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_maximizer_details_view_r.html)
- [Configuraciones de canal – Ecualizador](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_equalizer_r.html)
- [Panel Ajustes de ecualizador](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_equalizer_settings_panel_r.html)
- [Comparar canales](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_comparing_channels_t.html)
- [Configuraciones de canal – Envíos](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_channel_sends_r.html)
- [Configuraciones de canal – Enrutado directo](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_direct_routing_r.html)
- [Configuraciones de canal – Faders](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_channel_settings_channel_faders_r.html)
- [Foco en teclado en MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_keyboard_navigation_c.html)
- [Activar foco en teclado](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_activating_a_section_for_keyboard_navigation_t.html)
- [Navegar en una sección](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/mixconsole/mixconsole_navigating_t.html)
