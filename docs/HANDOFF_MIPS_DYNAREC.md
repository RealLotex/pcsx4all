# Handoff técnico — PCSX4All / RS-97 V3.0: optimización del dynarec MIPS

**Fecha inicial:** 2026-10-27; estado actualizado el 2026-10-31
**Repositorio de trabajo:** https://github.com/RealLotex/pcsx4all  
**Upstream:** https://github.com/retrofw/pcsx4all  
**Rama principal:** `master`  
**HEAD verificado al preparar el handoff:** `4a235df9840014f99a64943d448db56e29f47d79`  
**Pruebas CI de ese commit:** success — https://github.com/RealLotex/pcsx4all/actions/runs/40576738623

## Escena completa y presentación — actualización 2026-10-31

- La [PR #11](https://github.com/RealLotex/pcsx4all/pull/11) reúne los controles gráficos, el renderizado de toda la escena y la presentación agrupada. No contiene los prototipos descartados del asignador de registros, divisiones ni Gouraud uniforme. El código CPU/GTE de master se conserva.
- El antiguo nombre Internal resolution describía sólo muestreo de ciertos polígonos. Se reemplazó por superficies reducidas para todos los tipos de primitivas, incluidos sprites, texto y HUD; ambos buffers se conservan entre flips. En 320x240, Half/Quarter/Eighth dibujan a 160x120/80x60/40x30. El usuario autorizó reducir también las fuentes. Es un modo gráfico deliberadamente inexacto; Original sigue siendo el valor predeterminado.
- Compilar con `INTERNAL_RESOLUTION=1` habilita el menú y selecciona por defecto la implementación de escena completa (`FULL_SCENE=1`). El build predeterminado conserva `INTERNAL_RESOLUTION=0`; `FULL_SCENE=0` queda para comparar el prototipo selectivo antiguo. Detalles y límites: [FULL_SCENE_RESOLUTION.md](FULL_SCENE_RESOLUTION.md).
- En la primera comparación de escena completa, Quarter redujo CPU QEMU 12,4% en GT y 10,5% en Silent Hill frente a Original dentro del mismo binario. Después, un perfil externo por bloques MIPS identificó gasto redundante de presentación. Agrupar muestras y copiar filas repetidas redujo CPU otro 17,1% en GT (18,24 a 15,12 s de mediana) y 18,9% en Silent Hill (15,59 a 12,64 s), comparando Quarter escalar/agrupado dentro de otro único ejecutable. **No sumar porcentajes de distintas comparaciones ni convertirlos en FPS de consola.**
- La presentación agrupada produce los mismos píxeles: 9.216 casos host/sanitizadores/MIPS, RGB15/RGB24, formatos de salida, orígenes impares y vueltas de VRAM. En Quarter lee/convierte 4.800 muestras en vez de 76.800, conservando los 76.800 píxeles de salida. Las 48 capturas de juego coinciden entre las dos rutas y con la entrega anterior.
- Dos candidatas adicionales se descartaron: tabla exacta de divisores de raster (39.635.123 casos correctos, pero GT +3,2%/+2,3% CPU en Original/Quarter; atajos cero/uno casi iguales), e interpolación Gouraud constante (40.320 casos equivalentes, pero ningún caso aplicable en el perfil GT). No repetirlas sin evidencia nueva.
- Todavía faltan FPS de esta entrega en la RS-97. El último dato recibido, aproximadamente 40/60 en GT, corresponde a builds anteriores. El objetivo de rendimiento real en el dispositivo no está demostrado por CI o QEMU. Firmware y bootloader no se modifican.

## Estado histórico verificado — 2026-10-30

- `master` contiene la [PR #8](https://github.com/RealLotex/pcsx4all/pull/8), commit `29f7ebc7bcc758d41c2e8e6124d6015b49d877da`: comprobación exacta del desbordamiento de acumuladores GTE de 64 a 32 bits con menos ramas. Conserva el valor completo y los flags; no reducir el retorno a 32 bits ni volver a presentar esta optimización como nueva. Está habilitada por defecto; un árbol limpio con `GTE_SIGNED32_BOUNDS=0` recupera la ruta anterior.
- Se verificaron compilaciones completas de OPK de referencia y optimizado con el SDK oficial RetroFW, pruebas host con ASan/UBSan, tres millones de entradas del helper y 220.000 comparaciones de estado completo de los 22 comandos aritméticos GTE en host y MIPS/QEMU. [CI de esa PR](https://github.com/RealLotex/pcsx4all/actions/runs/41135055000). Alcance y comandos: [RS97_GTE_SIGNED32_BOUNDS.md](RS97_GTE_SIGNED32_BOUNDS.md).
- La comparación dentro del mismo ejecutable MIPS dio 5,2% menos tiempo de CPU en Gran Turismo, 4,0% menos en Digimon World y 3,2% menos en la calle inicial con niebla de Silent Hill, con calidad original, HLE/NTSC/rec 2.0 y frameskip desactivado. Son tres escenas locales y tiempos de QEMU; **no FPS medidos en la RS-97**. Los parches de replay, permisos JIT y lectura ISO usados en QEMU no forman parte del producto.
- La [PR #4](https://github.com/RealLotex/pcsx4all/pull/4) conserva como experimento la política de expulsar un registro y pruebas reales intérprete/dynarec, incluida una corrección de escritura IRGB. El usuario no observó ganancia de FPS en Gran Turismo con aquella build. No está fusionada ni demuestra una mejora de rendimiento general.
- Las [PR #6](https://github.com/RealLotex/pcsx4all/pull/6) y [#7](https://github.com/RealLotex/pcsx4all/pull/7) contienen opciones gráficas experimentales. Las cuatro primeras funcionaron en hardware, pero sin ganancia de FPS reportada. La resolución de rasterizado 3D posterior tiene mediciones locales y pérdida visible de calidad, sin medición de consola. No sustituir mejoras nativas del GTE/dynarec por estos controles ni prometer protección universal de fuentes.
- Ya se descartaron por resultados pequeños, desiguales o negativos bajo QEMU: caché de división de proyección, ruta NCLIP de coordenadas acotadas, determinante NCLIP de dos productos y emisión directa, desplegado de RTPT y especialización de MVMVA. Las variantes de saturación `LIM` con intervalos sin signo tampoco se incorporaron: la general empeoró Gran Turismo alrededor de un 2% y la restringida a mínimo cero dio sólo alrededor de un 1% adicional en tres escenas. El SDK ya emite MADD en productos matriciales. No repetirlos sin evidencia nueva que cambie la hipótesis. La paleta iluminada de 4 bpp de la PR #5 sigue siendo experimental.

Los apartados históricos siguientes describen la etapa inicial. La mejora de FPS reales en la consola sigue pendiente de comprobar; no dar el objetivo completo por alcanzado a partir de las mediciones de QEMU.

## Objetivo de la siguiente etapa

Mejorar los FPS / la latencia de los juegos exigentes de PlayStation en una **RS-97 V3.0 con RetroFW**, trabajando específicamente sobre el recompilador dinámico **MIPS → MIPS**, sin depender de cambios en audio o frameskip. Mantener la semántica de PS1, la compatibilidad con juegos y la seguridad del dispositivo. Una mejora de un microbenchmark de PC no es prueba de mayor FPS en la consola.

## Qué se hizo y qué está realmente verificado

Se fusionaron en `master` tres PR de renderizado:

- **PR #1:** https://github.com/RealLotex/pcsx4all/pull/1 — conversiones BGR555→RGB565 empaquetadas (256/384/512/640 px) y relleno opaco por pares.
- **PR #2:** https://github.com/RealLotex/pcsx4all/pull/2 — lectura de scanlines cuando cruzan el límite de VRAM y relleno rápido de polígonos planos opacos.
- **PR #3:** https://github.com/RealLotex/pcsx4all/pull/3 — sprites CLUT de 4 bpp sin iluminación, mezcla ni efectos de máscara: escrituras en pares con fallback ante solapamientos.

Pruebas host disponibles en `tests/test_rs97_render.cpp`, `tests/test_rs97_hotpaths.cpp`, `tests/test_rs97_sprite4bpp.cpp`; CI en `.github/workflows/rs97-renderer-tests.yml`. Las pruebas host C++03, ASan y UBSan concluyeron correctamente en el commit inicial verificado. Esa CI del renderer ejercita rutinas de renderizado, **no** el dynarec, ni puede garantizar ausencia de bloqueos o de brick. Las compilaciones completas posteriores y la CI del GTE se describen en la actualización anterior.

La optimización fusionada en esta etapa afecta al helper C del GTE llamado por el dynarec y el intérprete. La política experimental del asignador de registros y sus pruebas específicas permanecen en la PR #4.

## Arquitectura existente: no reimplementar lo que ya funciona

Compilación objetivo en `Makefile`: `RS97=1`, `RECOMPILER=mips`, `GPU=gpu_unai`, `USE_GPULIB=1`, herramienta cruzada `/opt/mipsel-RetroFW-linux-uclibc/usr/bin/mipsel-linux-*`, `-mips32`, `-O2`, `-flto`, `DYNAREC_SKIP_DCACHE_FLUSH`, espejo RAM en `/tmp`. **No habilitar instrucciones MIPS32r2 sin confirmar el CPU y la cadena de herramientas de la RS-97 V3.0.**

Archivos que conviene leer primero:

| Archivo | Contenido |
| --- | --- |
| `src/recompiler/mips/recompiler.cpp` | recompilación de bloques, dispatcher, entradas/salidas, caché de código, `recClear`, invalidación |
| `src/recompiler/mips/rec_lsu.cpp.h` | generación de loads/stores, accesos directos, secuencias LSU y caché temporal |
| `src/recompiler/mips/rec_bcu.cpp.h` | branches y branch delay slots, caché de PC |
| `src/recompiler/mips/rec_alu.cpp.h` | instrucciones ALU y propagación de constantes |
| `src/recompiler/mips/rec_gte.cpp.h` | accesos a GTE y pipeline LWC2/SWC2 |
| `src/recompiler/mips/regcache.h` | asignación y expulsión de registros MIPS host |
| `src/recompiler/mips/mips_codegen.cpp` y `mips_codegen.h` | análisis / emisión de instrucciones |
| `src/recompiler/mips/mem_mapping.cpp` | mapeos y espejos de memoria virtual para RAM y punteros de bloque |
| `src/recompiler/mips/readme.txt` | TODO histórico y casos de compatibilidad |

**Funciones/flags que ya están habilitados:** `ASM_EXECUTE_LOOP`, `USE_CODE_DISCARD`, `USE_DIRECT_BLOCK_RETURN_JUMPS`, `USE_DIRECT_FASTPATH_BLOCK_RETURN_JUMPS`, `USE_CONST_ADDRESSES`, `USE_CONST_FUZZY_ADDRESSES`, `USE_DIRECT_MEM_ACCESS`, `USE_VIRTUAL_PSXMEM_MAPPING`, `USE_VIRTUAL_RECRAM_MAPPING`, `USE_LSU_CACHING`, `USE_PC_CACHING`, `USE_CONST_BRANCH_OPTIMIZATIONS`, `USE_CONDITIONAL_MOVE_OPTIMIZATIONS`, `USE_GTE_DIRECT_MEM_ACCESS` y `USE_GTE_MEM_PIPELINING`. Revisar sus invariantes antes de agregar optimizaciones. Las ramas `mipsrec` y `dynarec_work[_part2/_part3/_part4]` son hitos históricos **ya antecesores de master**, no mejoras nuevas para fusionar en bloque.

## Plan concreto, por orden

1. **Baseline reproducible.** Compilar el HEAD actual con el SDK RetroFW como OPK *de usuario*; conservar el OPK original. Registrar CPU, clocks, tipo de juego, BIOS/HLE, escena y FPS/tiempo de frame con el mismo estado guardado. Comparar dynarec vs intérprete donde sea posible para diferenciar error de CPU de error de GPU.
2. **Instrumentación de coste muy ligera y desactivada en builds normales.** Contar por separado bloques recompilados, cache hits/misses del dispatcher, `recClear` e invalidaciones, fallback C en load/store y llamadas GTE. No imprimir por instrucción ni introducir overhead en mediciones finales. Separar tiempo de compilación JIT de ejecución de código generado.
3. **Primera optimización candidata (bajo riesgo relativo):** revisar el *host register allocation* en `regcache.h` y la emisión LSU para reducir cargas redundantes o spills **sin alterar memoria, excepciones o branch delay slots**. Implementar una sola optimización pequeña detrás de `#ifdef` reversible; inspeccionar las instrucciones MIPS host emitidas.
4. **Siguientes candidatas, si las métricas lo justifican:** reducción de lookup/retorno de bloques; fusiones de patrones ALU/LSU con propagación de constantes; reducir sobrecarga de llamadas GTE preservando sus efectos visibles y hazards. No asumir que más inline siempre gana: puede degradar I-cache/TLB.
5. **Áreas de mayor riesgo, postergar hasta disponer de tests de CPU sólidos:** invalidación de código automodificable, sincronización I/D-cache, mappings `MAP_FIXED`, delay slots (incluidos load delays), excepciones y caché aislada. El README todavía pide invalidación por páginas más completa y mejor tratamiento de branches dentro de delay slots: son objetivos de **corrección**, no shortcuts de performance.

## Gate de pruebas para TODA modificación del dynarec

- Añadir tests de semántica PS1/R3000A (ALU, overflow/excepciones, sign-extension, load/store no alineado `LWL/LWR/SWL/SWR`, `DIV/DIVU`, multiplicación y HI/LO, saltos y delay slots, MFC2/MTC2/LWC2/SWC2, auto-modificación).
- Ejecutar casos idénticos con **intérprete vs dynarec** y comparar estado CPU + RAM + registros GTE + excepciones; usar mini-programas PS1 reproducibles antes de depender de juegos.
- Usar tests host (ASan/UBSan) para la lógica C++; **compilar realmente para MIPS** y examinar el código host para validar ABI y opcode ISA. Un test x86 no ejecuta el JIT MIPS.
- Pruebas en hardware RS-97 por etapas: arranque, frontend, BIOS, test homebrew, guardados desechables y juegos de regresión. Problemas citados en `src/recompiler/mips/readme.txt`: `Tekken 2/3`, `Skullmonkeys`, `Tomb Raider 2/4/5`, `R-Types`, `Jackie Chan Stuntmaster`, `Destruction Derby 2`, `Next Tetris`, `Soul Blade`. No interpretar esa lista como resultados recientes.
- Medir FPS/frametime en escenas iguales y al mismo clock con el OPK anterior y el nuevo. Investigar cualquier pérdida de compatibilidad incluso si el promedio de FPS aumenta.
- Mantener backup de partidas/config y OPK; instalar **solo OPK**, nunca escribir bootloader, particiones o firmware. Riesgo de brick permanente por código de usuario aparentemente bajo, pero las pruebas actuales no pueden demostrar riesgo nulo.
- Crear rama por cambio, PR con diff y justificación, pasar las pruebas adecuadas y fusionar solo si no hay fallos. Las acciones peligrosas para hardware o semántica sin tests suficientes deben quedar fuera de `master`.

## Definición de hecho para la próxima iteración

Entregar una optimización del dynarec **real, pequeña y reversible**, evidencia de comparación funcional con intérprete y pruebas automáticas, PR con CI verde y compilación cruzada verificada. Reportar **por separado** benchmark de host, rendimiento del código generado y FPS reales; no atribuir mejoras de FPS hasta probarlas en la RS-97.

## Primer pedido recomendado al retomar

> Trabajá en `RealLotex/pcsx4all` desde `master` y seguí `docs/HANDOFF_MIPS_DYNAREC.md`. Comenzá con una auditoría instrumentada del dynarec MIPS (`recompiler.cpp`, `rec_lsu.cpp.h`, `regcache.h`). Elegí un cuello de botella medible, creá un cambio reversible y tests diferenciales intérprete/dynarec. Validá ISA/ABI MIPS y CI; no afirmes FPS en hardware sin medirlo. Evitá modificaciones del firmware o de mapeos críticos sin pruebas en dispositivo.
