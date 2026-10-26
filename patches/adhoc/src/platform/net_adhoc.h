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

#endif
