#ifndef MCPSP_NETWORK_WORLD_SYNC_H
#define MCPSP_NETWORK_WORLD_SYNC_H

#include "../platform/net_adhoc.h"
#include "sync_protocol.h"

/* MCPSP - Fase 2: sincronizar el mundo entre dos (o mas) PSP conectadas
 * por ad-hoc (Fase 1 ya deja la conexion lista en AdhocSession).
 *
 * COMO INTEGRAR ESTO AL MOTOR DE MCPSP:
 * -------------------------------------
 * Este modulo NO conoce las clases reales del juego (World, Player,
 * Chunk, etc. del repo base de MCPSP) porque ese codigo no esta en
 * este repo de patches. En su lugar expone un set de CALLBACKS que el
 * motor debe rellenar con 4-5 lineas cada uno, y un set de funciones
 * NOTIFY_* que el motor debe llamar cuando algo cambia localmente.
 *
 * Lado HOST o CLIENTE (simetrico, el mismo codigo sirve para ambos):
 *
 *   1. Despues de adhocJoinSelected() o adhocStartHost() exitoso:
 *        WorldSyncCallbacks cb = { getBlock, setBlock, onPlayerState,
 *                                   onPlayerLeave, onChunkRequested };
 *        worldSyncInit(&sync, &adhocSession, &cb);
 *
 *   2. Cada frame del juego:
 *        worldSyncUpdate(&sync);  // procesa red entrante y manda estado
 *
 *   3. Cuando el jugador local rompe/coloca un bloque (en tu codigo que
 *      ya modifica World::setBlock):
 *        worldSyncNotifyBlockChange(&sync, x, y, z, newBlockType);
 *
 *   4. Cada frame, despues de mover al jugador local:
 *        worldSyncNotifyPlayerState(&sync, px, py, pz, yaw, pitch, slot);
 *
 *   5. Al salir del mundo / desconectar:
 *        worldSyncShutdown(&sync);
 *
 * Los callbacks getBlock/setBlock son la unica parte que DEBE tocar
 * las clases reales del motor (ahi es donde se llama a World::getBlock
 * y World::setBlock o lo que exista en el repo base).
 */

#define MCPSP_SYNC_MAX_PLAYERS 8

typedef uint8_t (*WorldSync_GetBlockFn)(int16_t x, int16_t y, int16_t z);
typedef void    (*WorldSync_SetBlockFn)(int16_t x, int16_t y, int16_t z, uint8_t blockType);
typedef void    (*WorldSync_OnPlayerStateFn)(uint8_t playerId, float x, float y, float z,
                                              float yaw, float pitch, uint8_t selectedSlot);
typedef void    (*WorldSync_OnPlayerLeaveFn)(uint8_t playerId);
/* Devuelve 1 si el chunk existe y fue escrito en outBlocks (tamano
 * MCPSP_SYNC_CHUNK_VOLUME), 0 si no hay nada que mandar. Solo el host
 * necesita implementar esto de verdad; un cliente puede dejarlo NULL. */
typedef int     (*WorldSync_ProvideChunkFn)(int16_t chunkX, int16_t chunkY, int16_t chunkZ,
                                             uint8_t* outBlocks);

typedef struct {
    WorldSync_GetBlockFn      getBlock;       /* obligatorio */
    WorldSync_SetBlockFn      setBlock;       /* obligatorio */
    WorldSync_OnPlayerStateFn onPlayerState;  /* obligatorio (para dibujar al otro jugador) */
    WorldSync_OnPlayerLeaveFn onPlayerLeave;  /* opcional, puede ser NULL */
    WorldSync_ProvideChunkFn  provideChunk;   /* obligatorio solo en el host */
} WorldSyncCallbacks;

typedef struct {
    AdhocSession*      adhoc;
    WorldSyncCallbacks cb;
    uint8_t            localPlayerId;   /* 0 = host, asignado por MSG_WELCOME si es cliente */
    uint8_t            isHost;
    uint32_t           lastSendTickMs;  /* control de frecuencia de MSG_PLAYER_STATE */
    uint32_t           lastPingTickMs;
    uint8_t            connectedPlayers[MCPSP_SYNC_MAX_PLAYERS]; /* 1 = conectado */
    int                ready; /* 1 cuando MSG_WELCOME ya se recibio (clientes) */
} WorldSync;

/* Debe llamarse una sola vez, despues de que AdhocSession ya esta en
 * ADHOC_HOSTING (host) o ADHOC_CONNECTED (cliente). */
void worldSyncInit(WorldSync* sync, AdhocSession* adhoc, const WorldSyncCallbacks* cb);

/* Llamar una vez por frame. Procesa todos los paquetes pendientes
 * (drena la cola no bloqueante de net_adhoc) y manda el estado del
 * jugador local a intervalos fijos (~10 Hz, ver NOTIFY_STATE_INTERVAL_MS
 * en el .cpp). */
void worldSyncUpdate(WorldSync* sync);

/* El motor llama esto cuando el jugador local rompe o coloca un bloque.
 * Lo aplica localmente via setBlock (si aun no lo aplicaste tu mismo,
 * ver nota en el .cpp) y lo difunde a los demas peers. */
void worldSyncNotifyBlockChange(WorldSync* sync, int16_t x, int16_t y, int16_t z, uint8_t blockType);

/* El motor llama esto cada frame con la posicion/orientacion actual del
 * jugador local. worldSyncUpdate decide cuando realmente mandarlo por
 * red (no todos los frames, para no saturar el canal). */
void worldSyncNotifyPlayerState(WorldSync* sync, float x, float y, float z,
                                 float yaw, float pitch, uint8_t selectedSlot);

/* Pide al host un chunk especifico (solo tiene efecto si somos cliente;
 * en el host no hace nada porque el host ya tiene el mundo completo). */
void worldSyncRequestChunk(WorldSync* sync, int16_t chunkX, int16_t chunkY, int16_t chunkZ);

void worldSyncShutdown(WorldSync* sync);

#endif
