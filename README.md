# MCPSP

Base de trabajo sobre **Minecraft PE para PSP** (MCPSP v1.9.9).

## Qué hay aquí

- El **EBOOT + data** que tienes en local (`MCPSP-v1.9.9.zip`) es la build de prueba.
- El **código fuente** del port está en el proyecto upstream:
  - https://github.com/Pencea-Flavius/Minecraft-PE-PSP

## Objetivos de este fork / repo

1. Multijugador funcional en el menú (según lo que permita el port y el hardware PSP).
2. Mejoras de texturas **sin redistribuir assets oficiales de Mojang** (packs propios o que tú aportes legalmente).

## Cómo jugar la build v1.9.9 (PPSSPP / PSP)

1. Descomprime `MCPSP-v1.9.9.zip`.
2. Deja juntos `EBOOT.PBP` y la carpeta `data/`.
3. PPSSPP: abre `EBOOT.PBP`.
4. PSP real: `PSP/GAME/MCPSP/EBOOT.PBP` + `data/`.

## Desarrollo

Para cambiar menú, red, etc. hace falta el **source** (no solo el ZIP):

```bash
git clone https://github.com/Pencea-Flavius/Minecraft-PE-PSP.git
# PSPSDK + make
```

Este repositorio se usará para commits de trabajo propios encima de esa base.

## Legal

Minecraft y sus texturas son de Mojang/Microsoft. Este repo **no** incluye ni redistribuye el ZIP completo ni assets oficiales.
