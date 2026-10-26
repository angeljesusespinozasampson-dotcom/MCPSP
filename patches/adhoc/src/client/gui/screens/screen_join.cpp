#include <stdio.h>
#include <string.h>

/* UI para buscar y unirse a partidas multijugador (Fase 2)
 * Pantalla de lista de partidas disponibles
 */

struct JoinScreen {
    int selectedPeer;
    int peerCount;
    int isLoading;
    char status[96];
};

void screenJoinInit(JoinScreen* s) {
    if (!s) return;
    memset(s, 0, sizeof(JoinScreen));
    s->selectedPeer = 0;
    snprintf(s->status, 96, "Cargando partidas disponibles...");
}

void screenJoinDraw(JoinScreen* s) {
    if (!s) return;
    
    /* Renderizar título */
    printf("\n=== Unirse a Partida ===\n");
    printf("Estado: %s\n\n", s->status);
    
    if (s->peerCount == 0) {
        printf("No hay partidas disponibles\n");
        printf("Presiona ATRAS para volver\n");
        return;
    }
    
    /* Renderizar lista de partidas */
    printf("Partidas disponibles (%d):\n", s->peerCount);
    for (int i = 0; i < s->peerCount; i++) {
        char marker = (i == s->selectedPeer) ? '>' : ' ';
        printf("%c [%d] Partida de peer%d\n", marker, i + 1, i);
    }
    
    printf("\nX/O para unirse\n");
    printf("D-pad para seleccionar\n");
    printf("ATRAS para cancelar\n");
}

int screenJoinUpdate(JoinScreen* s) {
    if (!s) return 0;
    
    /* TODO: Leer input */
    /* X = unirse, ATRAS = volver */
    
    return 0;
}
