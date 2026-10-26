#include "net_adhoc.h"
#include <string.h>
#include <stdio.h>

/* Implementación de Multijugador ad-hoc PSP - Fase 2
 * Sincronización de mundo, jugadores y bloques entre peers
 */

/* Buffer circular para mensajes de red */
#define ADHOC_MSG_QUEUE_SIZE 256
#define ADHOC_MSG_MAX_SIZE 512

typedef struct {
    uint8_t type;        /* ADHOC_MSG_* */
    uint16_t len;
    uint32_t timestamp;
    uint8_t data[ADHOC_MSG_MAX_SIZE];
} AdhocMessage;

typedef struct {
    AdhocMessage msgs[ADHOC_MSG_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} AdhocMsgQueue;

/* Protocolo de mensajes */
enum {
    ADHOC_MSG_PING = 1,
    ADHOC_MSG_BLOCK_CHANGE = 2,     /* x,y,z,type */
    ADHOC_MSG_PLAYER_POS = 3,       /* x,y,z,yaw,pitch */
    ADHOC_MSG_PLAYER_INV = 4,       /* inventario */
    ADHOC_MSG_SYNC_WORLD = 5,       /* chunk completo */
    ADHOC_MSG_DISCONNECT = 6
};

typedef struct {
    int x, y, z;
    uint8_t block_type;
} BlockChange;

typedef struct {
    float x, y, z;
    float yaw, pitch;
} PlayerPos;

/* Estado global de sincronización */
static struct {
    AdhocSession session;
    AdhocMsgQueue outQueue;
    AdhocMsgQueue inQueue;
    uint32_t lastPingTime;
    int isHost;
} g_adhoc = {0};

/* ===== API Pública ===== */

int adhocInit(AdhocSession* s) {
    if (!s) return 0;
    
    memset(s, 0, sizeof(AdhocSession));
    memset(&g_adhoc, 0, sizeof(g_adhoc));
    
    s->state = ADHOC_IDLE;
    s->role = ADHOC_ROLE_NONE;
    s->inited = 1;
    
    snprintf(s->localName, MCPSP_ADHOC_NAME_LEN, "PSP_%d", (int)s);
    snprintf(s->status, 96, "Listo para multijugador");
    
    return 1;
}

void adhocShutdown(AdhocSession* s) {
    if (!s) return;
    adhocStop(s);
    memset(s, 0, sizeof(AdhocSession));
}

int adhocStartHost(AdhocSession* s, const char* displayName) {
    if (!s || !displayName) return 0;
    
    strncpy(s->localName, displayName, MCPSP_ADHOC_NAME_LEN - 1);
    s->state = ADHOC_HOSTING;
    s->role = ADHOC_ROLE_HOST;
    s->peerCount = 0;
    g_adhoc.isHost = 1;
    
    snprintf(s->status, 96, "Esperando peers... (%d/%d)", s->peerCount, MCPSP_ADHOC_MAX_PEERS - 1);
    return 1;
}

int adhocStartScan(AdhocSession* s) {
    if (!s) return 0;
    
    s->state = ADHOC_SCANNING;
    s->role = ADHOC_ROLE_NONE;
    s->peerCount = 0;
    
    snprintf(s->status, 96, "Escaneando partidas...");
    return 1;
}

void adhocUpdateScan(AdhocSession* s) {
    if (!s || s->state != ADHOC_SCANNING) return;
    
    /* Simulación: agregar peers encontrados */
    if (s->peerCount < MCPSP_ADHOC_MAX_PEERS) {
        snprintf(s->status, 96, "Encontradas %d partidas", s->peerCount);
    }
}

void adhocStop(AdhocSession* s) {
    if (!s) return;
    
    s->state = ADHOC_IDLE;
    s->role = ADHOC_ROLE_NONE;
    s->peerCount = 0;
    g_adhoc.isHost = 0;
    
    snprintf(s->status, 96, "Multijugador detenido");
}

int adhocJoinSelected(AdhocSession* s) {
    if (!s || s->selected < 0 || s->selected >= s->peerCount) return 0;
    
    s->state = ADHOC_CONNECTED;
    s->role = ADHOC_ROLE_CLIENT;
    
    snprintf(s->status, 96, "Conectado a: %s", s->peers[s->selected].name);
    return 1;
}

const char* adhocStateLabel(const AdhocSession* s) {
    if (!s) return "ERROR";
    
    switch (s->state) {
        case ADHOC_OFF: return "Apagado";
        case ADHOC_INIT_FAILED: return "Fallo inicialización";
        case ADHOC_IDLE: return "Inactivo";
        case ADHOC_HOSTING: return "Hosting";
        case ADHOC_SCANNING: return "Escaneando";
        case ADHOC_CONNECTED: return "Conectado";
        default: return "Desconocido";
    }
}

/* ===== Sincronización de Mundo ===== */

int adhocSendBlockChange(int x, int y, int z, uint8_t blockType) {
    BlockChange change = {x, y, z, blockType};
    
    AdhocMessage* msg = &g_adhoc.outQueue.msgs[g_adhoc.outQueue.head];
    msg->type = ADHOC_MSG_BLOCK_CHANGE;
    msg->len = sizeof(BlockChange);
    memcpy(msg->data, &change, sizeof(BlockChange));
    
    g_adhoc.outQueue.head = (g_adhoc.outQueue.head + 1) % ADHOC_MSG_QUEUE_SIZE;
    if (g_adhoc.outQueue.count < ADHOC_MSG_QUEUE_SIZE) {
        g_adhoc.outQueue.count++;
    }
    
    return 1;
}

int adhocSendPlayerPos(float x, float y, float z, float yaw, float pitch) {
    PlayerPos pos = {x, y, z, yaw, pitch};
    
    AdhocMessage* msg = &g_adhoc.outQueue.msgs[g_adhoc.outQueue.head];
    msg->type = ADHOC_MSG_PLAYER_POS;
    msg->len = sizeof(PlayerPos);
    msg->timestamp = 0;
    memcpy(msg->data, &pos, sizeof(PlayerPos));
    
    g_adhoc.outQueue.head = (g_adhoc.outQueue.head + 1) % ADHOC_MSG_QUEUE_SIZE;
    if (g_adhoc.outQueue.count < ADHOC_MSG_QUEUE_SIZE) {
        g_adhoc.outQueue.count++;
    }
    
    return 1;
}

int adhocPollMessage(AdhocMessage* outMsg) {
    if (!outMsg || g_adhoc.outQueue.count == 0) return 0;
    
    memcpy(outMsg, &g_adhoc.outQueue.msgs[g_adhoc.outQueue.tail], sizeof(AdhocMessage));
    g_adhoc.outQueue.tail = (g_adhoc.outQueue.tail + 1) % ADHOC_MSG_QUEUE_SIZE;
    g_adhoc.outQueue.count--;
    
    return 1;
}

void adhocProcessMessage(const AdhocMessage* msg) {
    if (!msg) return;
    
    switch (msg->type) {
        case ADHOC_MSG_BLOCK_CHANGE: {
            BlockChange* change = (BlockChange*)msg->data;
            /* TODO: Aplicar cambio de bloque en el mundo remoto */
            break;
        }
        case ADHOC_MSG_PLAYER_POS: {
            PlayerPos* pos = (PlayerPos*)msg->data;
            /* TODO: Actualizar posición de otro jugador */
            break;
        }
        case ADHOC_MSG_PING: {
            /* TODO: Responder con PONG */
            break;
        }
    }
}
