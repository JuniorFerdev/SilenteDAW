# Punch in / punch out — investigación (19-11-2026)

Estado: solo investigación. No hay código. El usuario pidió aclarar el funcionamiento antes de desarrollar.

## Pro Tools (búsqueda web, 19-11-2026)

- **Punch automático:** se hace con una selección en la línea de tiempo más pre-roll y post-roll. La selección marca el punch-in y el punch-out.
- **Pre-roll:** material que suena antes del punch-in, para que el músico entre en el tiempo.
- **Post-roll:** material que suena después del punch-out, para no cortar la cola.
- **Modo Normal:** graba desde el punch-in hasta el punch-out. Antes suena el pre-roll y después el post-roll.
- **Modo Loop:** el pre-roll solo aplica en la primera pasada; después la selección se repite sin pre-roll, y el post-roll se ignora.
- **QuickPunch:** graba sin destruir material durante el pre-roll, así queda material extra para editar. Según Avid, solo QuickPunch cambia la monitorización al instante en el punch-out.
- **Número de modos:** las fuentes no coinciden (4 o 6 según la versión y el nivel del producto). Hay que confirmarlo con la documentación oficial.

Fuentes:
- [Avid — Recording modes overview](https://www.avid.com/pro-tools/user-guide/record-modes)
- [Production Expert — Pro Tools record modes](https://www.production-expert.com/production-expert-1/pro-tools-record-modes-every-option-explained)
- [ProMix Academy — how to punch in a Pro Tools recording](https://promixacademy.com/blog/how-to-punch-in-a-pro-tools-recording/)
- [Steinberg — Using Pre-Roll and Post-Roll (Nuendo 8)](https://steinberg.help/nuendo/v8/en/cubase_nuendo/topics/recording/recording_using_preroll_and_postroll_t.html)

No verificado: cómo se arma la pista y se pulsa grabar durante la reproducción (punch manual). La búsqueda no lo devolvió.

## Cubase (referencia del usuario)

El manual de Cubase Pro 15 (`Cubase_Pro_15_0_Operation_Manual_es.pdf`) tiene el capítulo de punch y pre/post-roll. Falta extraerlo y leerlo antes de programar.

## Preguntas para el usuario antes de desarrollar

1. ¿Punch manual (armar y grabar durante la reproducción), punch automático con selección, o ambos?
2. ¿Pre-roll y post-roll como en Cubase (ajustables), o solo punch sin pre/post-roll?
3. ¿Modo Normal, Loop o QuickPunch? ¿Cuál priorizas?
