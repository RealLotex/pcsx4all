# Handoff técnico — PCSX4All / RS-97 V3.0: optimización del dynarec MIPS

**Fecha:** 2026-10-27  
**Repositorio de trabajo:** https://github.com/RealLotex/pcsx4all  
**Upstream:** https://github.com/retrofw/pcsx4all  
**Rama principal:** `master`  
**HEAD verificado al preparar el handoff:** `4a235df9840014f99a64943d448db56e29f47d79`  
**Pruebas CI de ese commit:** success — https://github.com/RealLotex/pcsx4all/actions/runs/40576738623

## Objetivo de la siguiente etapa

Mejorar los FPS / la latencia de los juegos exigentes de PlayStation en una **RS-97 V3.0 con RetroFW**, trabajando específicamente sobre el recompilador dinámico **MIPS → MIPS**, sin depender de cambios en audio o frameskip. Mantener la semántica de PS1, la compatibilidad con juegos y la seguridad del dispositivo. Una mejora de un microbenchmark de PC no es prueba de mayor FPS en la consola.

## Qué se hizo y qué está realmente verificado

Se fusionaron en `master` tres PR de renderizado:

- **PR #1:** https://github.com/RealLotex/pcsx4all/pull/1 — conversiones BGR555→RGB565 empaquetadas (256/384/512/640 px) y relleno opaco por pares.
- **PR #2:** https://github.com/RealLotex/pcsx4all/pull/2 — lectura de scanlines cuando cruzan el límite de VRAM y relleno rápido de polígonos planos opacos.
- **PR #3:** https://github.com/RealLotex/pcsx4all/pull/3 — sprites CLUT de 4 bpp sin iluminación, mezcla ni efectos de máscara: escrituras en pares con fallback ante solapamientos.

Pruebas host disponibles en `tests/test_rs97_render.cpp`, `tests/test_rs97_hotpaths.cpp`, `tests/test_rs97_sprite4bpp.cpp`; CI en `.github/workflows/rs97-renderer-tests.yml`. Las pruebas host C++03, ASan y UBSan concluyeron correctamente en el commit verificado. **No se ha confirmado una compilación completa del OPK ni un benchmark real en la RS-97.** El CI actual ejercita rutinas de renderizado, **no** el dynarec, ni puede garantizar ausencia de bloqueos o de brick.

**El código del dynarec todavía no fue modificado en esta línea de trabajo.**

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
