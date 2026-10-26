# Multijugador ad-hoc (Fase 1) — como aplicar y resubir

## Que incluye esta fase
- Boton **Multijugador** en el menu principal
- Pantalla **sin IP**: Crear partida / Buscar / Unirse a lista
- Modulo `net_adhoc` (WiFi ad-hoc PSP)

Aun **no** sincroniza el mundo (eso es fase 2). En emulador puede salir modo prueba.

## En el telefono (pasos)

1. Descarga el **codigo fuente** (zip del repo oficial o el de tu Release).
2. Descomprimelo (ZArchiver).
3. Copia encima estos archivos (misma ruta), desde la carpeta `patches/adhoc/` de este repo:
   - `src/platform/net_adhoc.h`
   - `src/platform/net_adhoc.cpp`
   - `src/client/gui/screens/screen_join.cpp`
   - `src/client/gui/screens/screen_title.cpp`
4. En el `Makefile`:
   - En `OBJS`, anade: `src/platform/net_adhoc.o \`
   - En `LIBS`, anade: `-lpspnet -lpspnet_adhoc -lpspnet_adhocctl`
5. **Compilar** no se puede bien solo en el telefono: usa PC con PSPSDK o GitHub Actions.
6. Cuando tengas el `EBOOT.PBP` nuevo + `data/`, subelo a **Releases** de MCPSP.

## Controles en Multijugador
- **Crear partida**: esta PSP hostea y espera
- **Buscar**: lista partidas cercanas (sin escribir IP)
- **X/O**: elegir
- **Triangulo**: detener red
- **Atras**: volver al titulo

## Dos PSP reales
1. PSP A → Multijugador → Crear partida
2. PSP B → Multijugador → Buscar → elegir la partida de A
3. Fase 1 confirma deteccion; jugar el mismo mundo = fase 2
