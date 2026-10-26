#ifndef MCPSP_NETWORK_SYNC_PROTOCOL_H
#define MCPSP_NETWORK_SYNC_PROTOCOL_H

#include <stdint.h>

/* MCPSP - Protocolo de sincronizacion de mundo (Fase 2)
 *
 * Va sobre el canal de datos que ya abre net_adhoc.cpp (PDP, sin
 * conexion, como UDP). Cada paquete empieza con SyncHeader seguido del
 * payload correspondiente a su `type`. Formato binario fijo, sin
 * padding raro: todo son enteros de tamano explicito (uint8/16/32) para
 * que el tamano en memoria sea igual en host y cliente.
 *
 * Diseno pensado para las limitaciones reales de PSP ad-hoc:
 *  - MTU practico de unos 2 KB por paquete (ver MCPSP_PDP_BUFSIZE).
 *  - Sin garantia de entrega ni de orden (como UDP): el protocolo es
 *    tolerante a perdida de paquetes (ver notas en cada mensaje).
 *  - Maximo 8 peers conectados en el mismo grupo ad-hoc.
 */

#define SYNC_PROTOCOL_VERSION 1
#define SYNC_MAGIC            0x4D43 /* "MC" */

enum SyncMsgType {
    MSG_HELLO         = 1,  /* cliente -> host: "quiero unirme" */
    MSG_WELCOME       = 2,  /* host -> cliente: acepta, manda seed/spawn */
    MSG_PLAYER_STATE  = 3,  /* ambos sentidos: posicion/mirada de un jugador */
    MSG_BLOCK_CHANGE  = 4,  /* ambos sentidos: alguien rompio/puso un bloque */
    MSG_CHUNK_REQUEST = 5,  /* cliente -> host: "mandame este chunk" */
    MSG_CHUNK_DATA    = 6,  /* host -> cliente: contenido de un chunk */
    MSG_PLAYER_LEAVE  = 7,  /* ambos sentidos: un jugador se desconecto */
    MSG_PING          = 8,  /* ambos sentidos: keepalive */
    MSG_PONG          = 9   /* respuesta a MSG_PING */
};

#pragma pack(push, 1)

/* Cabecera comun a todo mensaje. 8 bytes. */
typedef struct {
    uint16_t magic;      /* debe ser SYNC_MAGIC, descarta paquetes ajenos */
    uint8_t  version;    /* SYNC_PROTOCOL_VERSION */
    uint8_t  type;       /* SyncMsgType */
    uint8_t  playerId;   /* 0 = host, 1..7 = clientes, 0xFF = desconocido aun */
    uint8_t  reserved[3];
} SyncHeader;

/* MSG_HELLO: el cliente se presenta. El host le asigna playerId en el
 * MSG_WELCOME de respuesta. */
typedef struct {
    SyncHeader header;
    char       playerName[24];
} MsgHello;

/* MSG_WELCOME: el host confirma y manda el estado inicial minimo para
 * que el cliente pueda empezar a pedir chunks. */
typedef struct {
    SyncHeader header;
    uint8_t    assignedPlayerId;
    uint32_t   worldSeed;
    float      spawnX, spawnY, spawnZ;
} MsgWelcome;

/* MSG_PLAYER_STATE: se manda a ~10 Hz por jugador. Al ir por un canal
 * sin garantia de entrega no pasa nada si se pierde uno: el siguiente
 * trae la posicion actualizada igual. */
typedef struct {
    SyncHeader header;
    float      x, y, z;
    float      yaw, pitch;
    uint8_t    selectedSlot; /* item/bloque seleccionado, para que el
                                 otro jugador vea que vas a colocar */
} MsgPlayerState;

/* MSG_BLOCK_CHANGE: a diferencia de la posicion, ESTE mensaje si importa
 * que llegue. Fase 2 lo manda por duplicado (ver world_sync.cpp,
 * NOTIFY_RESEND_COUNT) para compensar la falta de ACK explicito; un
 * cliente que ya aplico el mismo cambio simplemente lo ignora la
 * segunda vez (mismo x,y,z,blockType -> no-op). */
typedef struct {
    SyncHeader header;
    int16_t    x, y, z;      /* coordenadas de bloque en el mundo */
    uint8_t    blockType;    /* nuevo tipo de bloque (0 = aire/roto) */
} MsgBlockChange;

/* MSG_CHUNK_REQUEST: el cliente pide un chunk por sus coordenadas de
 * chunk (no de bloque). */
typedef struct {
    SyncHeader header;
    int16_t    chunkX, chunkY, chunkZ;
} MsgChunkRequest;

/* MSG_CHUNK_DATA: snapshot plano de un chunk. MCPSP_SYNC_CHUNK_VOLUME
 * debe coincidir con el tamano de chunk real del motor (ver nota en
 * world_sync.h). Si el chunk del motor es mas grande que lo que cabe
 * en un paquete, world_sync.cpp lo parte en varios MSG_CHUNK_DATA con
 * `part`/`partCount`. */
#define MCPSP_SYNC_CHUNK_VOLUME 512 /* ej. chunk de 8x8x8, como en Bloques PSP */

typedef struct {
    SyncHeader header;
    int16_t    chunkX, chunkY, chunkZ;
    uint8_t    part;       /* 0-based */
    uint8_t    partCount;
    uint16_t   byteOffset; /* offset de `blocks` dentro del chunk completo */
    uint16_t   byteCount;  /* cuantos bytes trae este paquete */
    uint8_t    blocks[MCPSP_SYNC_CHUNK_VOLUME];
} MsgChunkData;

typedef struct {
    SyncHeader header;
} MsgPlayerLeave;

typedef struct {
    SyncHeader header;
    uint32_t   tick; /* para medir latencia ida-vuelta si se quiere */
} MsgPing;

typedef struct {
    SyncHeader header;
    uint32_t   tick;
} MsgPong;

#pragma pack(pop)

#endif
