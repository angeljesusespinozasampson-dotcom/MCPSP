/* MCPSP - Fase 2: implementacion de sincronizacion de mundo.
 * Ver world_sync.h para como integrar esto al motor del juego.
 */
#include "world_sync.h"
#include <string.h>

#ifndef MCPSP_ADHOC_STUB
#include <pspkernel.h>
#else
#include <time.h>
#endif

/* Cada cuanto se manda MSG_PLAYER_STATE propio (10 Hz = cada 100 ms).
 * Mas rapido gasta ancho de banda ad-hoc (limitado); mas lento se ve
 * menos fluido el movimiento del otro jugador. 100 ms es un punto medio
 * razonable para 2-4 jugadores. */
#define NOTIFY_STATE_INTERVAL_MS 100
#define PING_INTERVAL_MS         2000
#define MAX_PACKETS_PER_FRAME    16  /* evita que un frame se cuelgue
                                        drenando una cola enorme */

static uint32_t now_ms(void)
{
#ifndef MCPSP_ADHOC_STUB
    /* Tiempo del sistema en microsegundos desde que arranco la PSP. */
    return (uint32_t)(sceKernelGetSystemTimeWide() / 1000ULL);
#else
    return (uint32_t)((clock() * 1000ULL) / CLOCKS_PER_SEC);
#endif
}

static void fill_header(SyncHeader* h, SyncMsgType type, uint8_t playerId)
{
    h->magic = SYNC_MAGIC;
    h->version = SYNC_PROTOCOL_VERSION;
    h->type = (uint8_t)type;
    h->playerId = playerId;
    h->reserved[0] = h->reserved[1] = h->reserved[2] = 0;
}

static int header_valid(const SyncHeader* h)
{
    return h->magic == SYNC_MAGIC && h->version == SYNC_PROTOCOL_VERSION;
}

void worldSyncInit(WorldSync* sync, AdhocSession* adhoc, const WorldSyncCallbacks* cb)
{
    memset(sync, 0, sizeof(*sync));
    sync->adhoc = adhoc;
    sync->cb = *cb;
    sync->isHost = (adhoc->role == ADHOC_ROLE_HOST) ? 1 : 0;
    sync->localPlayerId = sync->isHost ? 0 : 0xFF; /* cliente aun no tiene id */
    sync->ready = sync->isHost ? 1 : 0; /* el host ya esta "listo" desde el inicio */
    sync->lastSendTickMs = now_ms();
    sync->lastPingTickMs = now_ms();

    if (sync->isHost) {
        sync->connectedPlayers[0] = 1; /* el host se cuenta a si mismo como jugador 0 */
    }

    /* El cliente manda HELLO de inmediato para iniciar el handshake;
     * el host simplemente espera mensajes entrantes. */
    if (!sync->isHost && adhoc->peerCount > 0) {
        MsgHello hello;
        fill_header(&hello.header, MSG_HELLO, 0xFF);
        strncpy(hello.playerName, adhoc->localName, sizeof(hello.playerName) - 1);
        hello.playerName[sizeof(hello.playerName) - 1] = '\0';
        /* El unico peer conocido en este punto (Fase 1) es el host al
         * que nos acabamos de unir con adhocJoinSelected(). */
        adhocSendTo(sync->adhoc, &sync->adhoc->peers[sync->adhoc->selected],
                    &hello, sizeof(hello));
    }
}

/* ---------- Envio de los distintos tipos de mensaje ---------- */

static void send_welcome(WorldSync* sync, const AdhocPeer* to, uint8_t assignedId)
{
    MsgWelcome msg;
    fill_header(&msg.header, MSG_WELCOME, 0 /* host */);
    msg.assignedPlayerId = assignedId;
    msg.worldSeed = 0; /* el motor real puede rellenar esto si usa seed;
                           0 = "usa el mundo que ya tengas cargado" */
    msg.spawnX = 0.0f; msg.spawnY = 0.0f; msg.spawnZ = 0.0f;
    adhocSendTo(sync->adhoc, to, &msg, sizeof(msg));
}

static void broadcast_block_change(WorldSync* sync, int16_t x, int16_t y, int16_t z, uint8_t blockType)
{
    MsgBlockChange msg;
    fill_header(&msg.header, MSG_BLOCK_CHANGE, sync->localPlayerId);
    msg.x = x; msg.y = y; msg.z = z;
    msg.blockType = blockType;
    adhocBroadcast(sync->adhoc, &msg, sizeof(msg));
}

static void send_player_state(WorldSync* sync, float x, float y, float z,
                               float yaw, float pitch, uint8_t slot)
{
    MsgPlayerState msg;
    fill_header(&msg.header, MSG_PLAYER_STATE, sync->localPlayerId);
    msg.x = x; msg.y = y; msg.z = z;
    msg.yaw = yaw; msg.pitch = pitch;
    msg.selectedSlot = slot;

    /* El host difunde a todos los clientes; un cliente solo tiene un
     * peer (el host), asi que adhocBroadcast tambien le sirve. */
    adhocBroadcast(sync->adhoc, &msg, sizeof(msg));
}

/* ---------- Manejo de mensajes entrantes ---------- */

static void handle_hello(WorldSync* sync, const AdhocPeer* from, const MsgHello* msg)
{
    if (!sync->isHost) return; /* solo el host procesa HELLO */

    /* Asignar el primer slot de jugador libre (1..7; 0 es el host). */
    uint8_t assigned = 0xFF;
    for (uint8_t i = 1; i < MCPSP_SYNC_MAX_PLAYERS; i++) {
        if (!sync->connectedPlayers[i]) { assigned = i; break; }
    }
    if (assigned == 0xFF) return; /* sala llena */

    sync->connectedPlayers[assigned] = 1;
    send_welcome(sync, from, assigned);
}

static void handle_welcome(WorldSync* sync, const MsgWelcome* msg)
{
    if (sync->isHost) return; /* el host no recibe WELCOME */
    sync->localPlayerId = msg->assignedPlayerId;
    sync->connectedPlayers[msg->assignedPlayerId] = 1;
    sync->connectedPlayers[0] = 1; /* el host siempre es el jugador 0 */
    sync->ready = 1;
    /* A partir de aqui el motor puede empezar a pedir chunks con
     * worldSyncRequestChunk() si lo necesita. */
}

static void handle_block_change(WorldSync* sync, const MsgBlockChange* msg)
{
    /* Idempotente: si el bloque ya es ese tipo, setBlock no deberia
     * tener efectos secundarios raros (el motor decide, pero lo normal
     * es que escribir el mismo valor dos veces sea inofensivo). Esto es
     * lo que nos protege de que MSG_BLOCK_CHANGE llegue duplicado por
     * la falta de ACK del transporte. */
    sync->cb.setBlock(msg->x, msg->y, msg->z, msg->blockType);

    /* El host debe re-difundir el cambio a los demas clientes (el
     * cliente que lo origino no esta en la lista de "demas", pero
     * adhocBroadcast igual le re-manda a todos; es barato y simplifica
     * la logica. Si se quiere optimizar, aqui se excluiria al origen). */
    if (sync->isHost) {
        adhocBroadcast(sync->adhoc, msg, sizeof(*msg));
    }
}

static void handle_player_state(WorldSync* sync, const MsgPlayerState* msg)
{
    if (msg->header.playerId == sync->localPlayerId) return; /* eco propio, ignorar */
    sync->cb.onPlayerState(msg->header.playerId, msg->x, msg->y, msg->z,
                            msg->yaw, msg->pitch, msg->selectedSlot);

    if (sync->isHost) {
        /* reenviar a los demas clientes para que se vean entre si */
        adhocBroadcast(sync->adhoc, msg, sizeof(*msg));
    }
}

static void handle_chunk_request(WorldSync* sync, const AdhocPeer* from, const MsgChunkRequest* msg)
{
    if (!sync->isHost || !sync->cb.provideChunk) return;

    uint8_t blocks[MCPSP_SYNC_CHUNK_VOLUME];
    if (!sync->cb.provideChunk(msg->chunkX, msg->chunkY, msg->chunkZ, blocks)) return;

    /* Un chunk de 512 bytes cabe en un solo paquete PDP (buffer de 4096
     * bytes en net_adhoc.cpp), asi que se manda en una sola parte. Si en
     * el motor real los chunks son mas grandes que ~3500 bytes, aqui
     * habria que partir `blocks` en varios MsgChunkData con part/partCount
     * crecientes; la estructura ya trae esos campos preparados. */
    MsgChunkData out;
    fill_header(&out.header, MSG_CHUNK_DATA, 0);
    out.chunkX = msg->chunkX; out.chunkY = msg->chunkY; out.chunkZ = msg->chunkZ;
    out.part = 0; out.partCount = 1;
    out.byteOffset = 0;
    out.byteCount = MCPSP_SYNC_CHUNK_VOLUME;
    memcpy(out.blocks, blocks, MCPSP_SYNC_CHUNK_VOLUME);
    adhocSendTo(sync->adhoc, from, &out, sizeof(out));
}

static void handle_chunk_data(WorldSync* sync, const MsgChunkData* msg)
{
    /* Aplica los bloques recibidos al mundo local llamando setBlock por
     * cada posicion. Es mas trafico de llamadas que un memcpy directo a
     * la estructura de chunk del motor, pero as mantiene world_sync.cpp
     * desacoplado de como esta implementado el Chunk real. Si se quiere
     * mas velocidad, el motor puede exponer un setChunkBlocks() propio. */
    int16_t baseX = msg->chunkX * 8; /* ver nota MCPSP_SYNC_CHUNK_VOLUME: 8x8x8 */
    int16_t baseY = msg->chunkY * 8;
    int16_t baseZ = msg->chunkZ * 8;

    for (int i = msg->byteOffset; i < msg->byteOffset + msg->byteCount; i++) {
        int lx = i % 8;
        int ly = (i / 8) % 8;
        int lz = i / 64;
        sync->cb.setBlock(baseX + lx, baseY + ly, baseZ + lz, msg->blocks[i - msg->byteOffset]);
    }
}

static void handle_player_leave(WorldSync* sync, const MsgPlayerLeave* msg)
{
    uint8_t id = msg->header.playerId;
    if (id < MCPSP_SYNC_MAX_PLAYERS) sync->connectedPlayers[id] = 0;
    if (sync->cb.onPlayerLeave) sync->cb.onPlayerLeave(id);
    if (sync->isHost) adhocBroadcast(sync->adhoc, msg, sizeof(*msg));
}

static void handle_ping(WorldSync* sync, const AdhocPeer* from, const MsgPing* msg)
{
    MsgPong pong;
    fill_header(&pong.header, MSG_PONG, sync->localPlayerId);
    pong.tick = msg->tick;
    adhocSendTo(sync->adhoc, from, &pong, sizeof(pong));
}

/* ---------- Bucle principal ---------- */

void worldSyncUpdate(WorldSync* sync)
{
    uint8_t buf[sizeof(MsgChunkData)]; /* el mensaje mas grande del protocolo */
    AdhocPeer from;

    for (int i = 0; i < MAX_PACKETS_PER_FRAME; i++) {
        int len = adhocRecv(sync->adhoc, &from, buf, sizeof(buf));
        if (len < (int)sizeof(SyncHeader)) break; /* nada mas pendiente */

        const SyncHeader* h = (const SyncHeader*)buf;
        if (!header_valid(h)) continue; /* paquete ajeno/corrupto, descartar */

        switch ((SyncMsgType)h->type) {
            case MSG_HELLO:
                if (len >= (int)sizeof(MsgHello))
                    handle_hello(sync, &from, (const MsgHello*)buf);
                break;
            case MSG_WELCOME:
                if (len >= (int)sizeof(MsgWelcome))
                    handle_welcome(sync, (const MsgWelcome*)buf);
                break;
            case MSG_BLOCK_CHANGE:
                if (len >= (int)sizeof(MsgBlockChange))
                    handle_block_change(sync, (const MsgBlockChange*)buf);
                break;
            case MSG_PLAYER_STATE:
                if (len >= (int)sizeof(MsgPlayerState))
                    handle_player_state(sync, (const MsgPlayerState*)buf);
                break;
            case MSG_CHUNK_REQUEST:
                if (len >= (int)sizeof(MsgChunkRequest))
                    handle_chunk_request(sync, &from, (const MsgChunkRequest*)buf);
                break;
            case MSG_CHUNK_DATA:
                if (len >= (int)sizeof(MsgChunkData))
                    handle_chunk_data(sync, (const MsgChunkData*)buf);
                break;
            case MSG_PLAYER_LEAVE:
                if (len >= (int)sizeof(MsgPlayerLeave))
                    handle_player_leave(sync, (const MsgPlayerLeave*)buf);
                break;
            case MSG_PING:
                if (len >= (int)sizeof(MsgPing))
                    handle_ping(sync, &from, (const MsgPing*)buf);
                break;
            case MSG_PONG:
                /* no hace falta accion; se podria medir latencia aqui */
                break;
            default:
                break;
        }
    }

    /* Keepalive: sin esto, si nadie rompe bloques ni se mueve, algunos
     * routers/firmwares ad-hoc podrian considerar la sesion inactiva. */
    uint32_t t = now_ms();
    if (t - sync->lastPingTickMs >= PING_INTERVAL_MS) {
        MsgPing ping;
        fill_header(&ping.header, MSG_PING, sync->localPlayerId);
        ping.tick = t;
        adhocBroadcast(sync->adhoc, &ping, sizeof(ping));
        sync->lastPingTickMs = t;
    }
}

void worldSyncNotifyBlockChange(WorldSync* sync, int16_t x, int16_t y, int16_t z, uint8_t blockType)
{
    /* Nota de integracion: aplica el cambio local AQUI (no antes, en el
     * codigo del motor) para que el flujo sea uno solo: el motor llama
     * worldSyncNotifyBlockChange() en lugar de World::setBlock()
     * directamente, y este modulo se encarga de aplicarlo Y difundirlo. */
    sync->cb.setBlock(x, y, z, blockType);
    broadcast_block_change(sync, x, y, z, blockType);
}

void worldSyncNotifyPlayerState(WorldSync* sync, float x, float y, float z,
                                 float yaw, float pitch, uint8_t selectedSlot)
{
    uint32_t t = now_ms();
    if (t - sync->lastSendTickMs < NOTIFY_STATE_INTERVAL_MS) return;
    sync->lastSendTickMs = t;
    send_player_state(sync, x, y, z, yaw, pitch, selectedSlot);
}

void worldSyncRequestChunk(WorldSync* sync, int16_t chunkX, int16_t chunkY, int16_t chunkZ)
{
    if (sync->isHost) return; /* el host no se pide chunks a si mismo */

    MsgChunkRequest msg;
    fill_header(&msg.header, MSG_CHUNK_REQUEST, sync->localPlayerId);
    msg.chunkX = chunkX; msg.chunkY = chunkY; msg.chunkZ = chunkZ;
    adhocBroadcast(sync->adhoc, &msg, sizeof(msg)); /* el unico peer es el host */
}

void worldSyncShutdown(WorldSync* sync)
{
    MsgPlayerLeave msg;
    fill_header(&msg.header, MSG_PLAYER_LEAVE, sync->localPlayerId);
    adhocBroadcast(sync->adhoc, &msg, sizeof(msg));
    memset(sync, 0, sizeof(*sync));
}
