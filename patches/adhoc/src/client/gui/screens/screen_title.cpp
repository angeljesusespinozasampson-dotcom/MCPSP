#include <stdio.h>
#include <string.h>

/* Pantalla de título con Multijugador (Fase 2)
 * Menú principal extendido con opción Multijugador
 */

enum TitleMenuOption {
    TITLE_SINGLE_PLAYER = 0,
    TITLE_MULTIPLAYER = 1,      /* NUEVO en Fase 2 */
    TITLE_SETTINGS = 2,
    TITLE_QUIT = 3
};

struct TitleScreen {
    int selectedOption;
    int optionCount;
    const char* options[4];
};

void screenTitleInit(TitleScreen* s) {
    if (!s) return;
    memset(s, 0, sizeof(TitleScreen));
    
    s->options[TITLE_SINGLE_PLAYER] = "Juego Normal";
    s->options[TITLE_MULTIPLAYER] = "Multijugador";    /* NUEVA OPCIÓN */
    s->options[TITLE_SETTINGS] = "Configuración";
    s->options[TITLE_QUIT] = "Salir";
    
    s->optionCount = 4;
    s->selectedOption = 0;
}

void screenTitleDraw(TitleScreen* s) {
    if (!s) return;
    
    printf("\n");
    printf("===============================\n");
    printf("   MINECRAFT PE PSP - Fase 2\n");
    printf("   Multijugador ad-hoc WiFi\n");
    printf("===============================\n\n");
    
    for (int i = 0; i < s->optionCount; i++) {
        char marker = (i == s->selectedOption) ? '>' : ' ';
        printf("%c [%d] %s\n", marker, i + 1, s->options[i]);
    }
    
    printf("\nX/O para seleccionar\n");
    printf("D-pad para navegar\n");
}

int screenTitleUpdate(TitleScreen* s) {
    if (!s) return 0;
    
    /* TODO: Leer input
     * D-pad arriba/abajo: cambiar selección
     * X/O: confirmar
     */
    
    /* Retorna opción seleccionada o -1 si no hay cambio */
    return -1;
}

/* Manejo de opciones seleccionadas */
int screenTitleHandleSelection(TitleScreen* s) {
    if (!s) return -1;
    
    switch (s->selectedOption) {
        case TITLE_SINGLE_PLAYER:
            printf("Iniciando juego normal...\n");
            return TITLE_SINGLE_PLAYER;
            
        case TITLE_MULTIPLAYER:
            printf("Iniciando multijugador (Fase 2)...\n");
            /* Ir a pantalla de Multijugador (Crear/Buscar) */
            return TITLE_MULTIPLAYER;
            
        case TITLE_SETTINGS:
            printf("Abriendo configuración...\n");
            return TITLE_SETTINGS;
            
        case TITLE_QUIT:
            printf("Saliendo...\n");
            return TITLE_QUIT;
            
        default:
            return -1;
    }
}
