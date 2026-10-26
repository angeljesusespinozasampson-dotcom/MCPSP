#ifndef MCPSP_PLATFORM_NET_ADHOC_H
#define MCPSP_PLATFORM_NET_ADHOC_H

/* Multijugador local PSP: WiFi ad-hoc, sin escribir IP.
 * Fase 1: crear partida (host) y buscar partidas cercanas (scan).
 */

#define MCPSP_ADHOC_PRODUCT   "MCPSP01"
#define MCPSP_ADHOC_MAX_PEERS 8
#define MCPSP_ADHOC_NAME_LEN  24

enum AdhocRole {
    ADHOC_ROLE_NONE = 0,
    ADHOC_ROLE_HOST = 1,
    ADHOC_ROLE_CLIENT = 2
};

enum AdhocState {
    ADHOC_OFF = 0,
    ADHOC_INIT_FAILED,
    ADHOC_IDLE,
    ADHOC_HOSTING,
    ADHOC_SCANNING,
    ADHOC_CONNECTED
};

struct AdhocPeer {
    char name[MCPSP_ADHOC_NAME_LEN];
    char mac[18];
    int  valid;
};

struct AdhocSession {
    AdhocState state;
    AdhocRole  role;
    char       localName[MCPSP_ADHOC_NAME_LEN];
    char       status[96];
    AdhocPeer  peers[MCPSP_ADHOC_MAX_PEERS];
    int        peerCount;
    int        selected;
    int        inited;
};

int  adhocInit(AdhocSession* s);
void adhocShutdown(AdhocSession* s);
int  adhocStartHost(AdhocSession* s, const char* displayName);
int  adhocStartScan(AdhocSession* s);
void adhocUpdateScan(AdhocSession* s);
void adhocStop(AdhocSession* s);
int  adhocJoinSelected(AdhocSession* s);
const char* adhocStateLabel(const AdhocSession* s);

/* ---- Fase 2: transporte de datos entre peers (usado por world_sync) ---- */

/* Manda `data` (len bytes) a un peer especifico. Devuelve bytes enviados
 * o <0 si fallo. No bloqueante. */
int adhocSendTo(AdhocSession* s, const AdhocPeer* peer, const void* data, int len);

/* Manda `data` a todos los peers validos de la sesion (el host lo usa
 * para difundir cambios de mundo a todos los clientes). Devuelve cuantos
 * peers recibieron el envio exitosamente. */
int adhocBroadcast(AdhocSession* s, const void* data, int len);

/* Revisa si llego un paquete (no bloqueante). Si llego, lo copia a `buf`
 * (hasta bufLen bytes) y llena `outPeer` con el remitente (si no es NULL).
 * Devuelve bytes recibidos, o <0 si no hay nada pendiente este frame. */
int adhocRecv(AdhocSession* s, AdhocPeer* outPeer, void* buf, int bufLen);

#endif

/* ===== Fase 2: Sincronización de Mundo ===== */

typedef struct {
    uint8_t type;
    uint16_t len;
    uint32_t timestamp;
    uint8_t data[512];
} AdhocMessage;

/* Enviar cambios de bloques a peers */
int adhocSendBlockChange(int x, int y, int z, uint8_t blockType);

/* Enviar posición del jugador */
int adhocSendPlayerPos(float x, float y, float z, float yaw, float pitch);

/* Procesar mensajes recibidos */
int adhocPollMessage(AdhocMessage* outMsg);
void adhocProcessMessage(const AdhocMessage* msg);

