#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>    // Para controlar o tempo
#include <unistd.h>  // Para a função sleep/usleep
#include <sys/time.h>
#include <termios.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#define MAX_NOTES 2000
#define LEVEL_FILENAME "notes.txt"
#define TARGET_FPS 30
#define FRAME_DELAY (1000 / TARGET_FPS)

#define ALTURA_DA_PISTA 20
#define TEMPO_DE_ANTEVISAO 4.0f

// Estrutura para guardar uma única nota do mapa
typedef struct {
    float timestamp; // Em que segundo a nota aparece
    char note_name[5];
    int note_index;  // A qual "pista" a nota pertence (0 a 4, por exemplo)
    int foi_processada; // Flag para saber se já acertamos ou erramos essa nota
} GameNote;

// Estrutura para guardar todo o estado do jogo
typedef struct {
    GameNote level_notes[MAX_NOTES];
    int note_count;

    int score;
    int combo;
    
    Uint32 start_time; // Momento em que o jogo começou
} GameState;

//mapeamento dinamico
typedef struct {
    char note_name[5];
    int track_index;
} NoteMapping;

struct termios orig_termios;

void carregar_nivel(GameState *state);
void inicializar_jogo(GameState *state);
void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    // Registra a função de desabilitar para ser chamada quando o programa sair
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG );

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
//tecla pressionada
int kbhit(void) {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

int main() {
    enableRawMode();

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) { // Inicializa também o timer da SDL
        printf("Nao foi possivel inicializar o SDL: %s\n", SDL_GetError());
        return -1;
    }

    // Formato: Frequência 44100Hz, formato padrão, 2 canais (estéreo), buffer de 2048
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("Nao foi possivel inicializar o SDL_mixer: %s\n", Mix_GetError());
        return -1;
    }

    const char *arquivo_musica = "musica_sweet.mp3";

    Mix_Music *musica_de_fundo = Mix_LoadMUS(arquivo_musica);
    if (musica_de_fundo == NULL) {
        printf("Nao foi possivel carregar a musica '%s': %s\n", arquivo_musica, Mix_GetError());
        return -1;
    }

    GameState game_state;
    carregar_nivel(&game_state);
    inicializar_jogo(&game_state);

    printf("Comecando a musica!\n");
    Mix_PlayMusic(musica_de_fundo, 1);

    float tempo_final_do_nivel = 0;
    if (game_state.note_count > 0) {
        tempo_final_do_nivel = game_state.level_notes[game_state.note_count - 1].timestamp + 2.0f;
    } else {
        tempo_final_do_nivel = 5.0f;
    }


    sleep(3);

    Uint32 frame_start_time;
    int frame_time_spent;
    int jogo_esta_rodando = 1;
    double tempo_decorrido = 0;

    while (jogo_esta_rodando && Mix_PlayingMusic()) {

        frame_start_time = SDL_GetTicks();

        struct timeval tempo_atual;
        gettimeofday(&tempo_atual, NULL);
        
        double tempo_decorrido = (double)(SDL_GetTicks() - game_state.start_time) / 1000.0;

        int jogador_apertou_pista = -1; 
        
        if (kbhit()) { 
            char ch = getchar();

            if (ch == 3) {
                jogo_esta_rodando = 0;
                printf("\nCtrl+C pressionado. Encerrando o jogo...\n");
                continue;
            }
            if (ch >= '0' && ch <= '4') {
                jogador_apertou_pista = ch - '0';
                printf(">>> JOGADOR APERTOU A PISTA: %d\n", jogador_apertou_pista);
            }
        }

        for (int i = 0; i < game_state.note_count; i++) {
            if (game_state.level_notes[i].foi_processada) {
                continue;
            }
            float timestamp_nota = game_state.level_notes[i].timestamp;
            int pista_nota = game_state.level_notes[i].note_index;
            
            //se o jogador apertou a teclha no tempo certo
            if ((jogador_apertou_pista == pista_nota) && (tempo_decorrido > timestamp_nota - 0.2 && tempo_decorrido < timestamp_nota + 0.2)) {
                printf(">>> ACERTOU! Nota %s <<<\n", game_state.level_notes[i].note_name);
                game_state.score += 10 * game_state.combo;
                game_state.combo++;
                game_state.level_notes[i].foi_processada = 1;
                break;
            }
            if (tempo_decorrido > timestamp_nota + 0.2) {
                if (!game_state.level_notes[i].foi_processada) {
                    printf("ERROU! Nota %s\n", game_state.level_notes[i].note_name);
                    game_state.combo = 1;
                    game_state.level_notes[i].foi_processada = 1;
                }
            }
            if (tempo_decorrido > tempo_final_do_nivel) {
                jogo_esta_rodando = 0; // Desliga a flag quando o tempo acaba
            }
        
        }
        

        system("clear");

        char pista_visual[ALTURA_DA_PISTA][5];

        for (int i = 0; i < ALTURA_DA_PISTA; i++) {
            sprintf(pista_visual[i], "    "); // 4 espaços
        }

        for (int i = 0; i < game_state.note_count; i++) {
            if (!game_state.level_notes[i].foi_processada) {
                float tempo_da_nota = game_state.level_notes[i].timestamp;
                float dist_temporal = tempo_da_nota - tempo_decorrido;

                // Verifica se a nota está dentro do nosso campo de visão
                if (dist_temporal >= 0 && dist_temporal < TEMPO_DE_ANTEVISAO) {
                    // Converte a distância temporal numa posição na pista
                    int linha = ALTURA_DA_PISTA - 1 - (int)((dist_temporal / TEMPO_DE_ANTEVISAO) * ALTURA_DA_PISTA);
                    
                    if (linha >= 0 && linha < ALTURA_DA_PISTA) {
                        // Coloca o número da pista (1 a 4) no local correto
                        int pista_da_nota = game_state.level_notes[i].note_index;
                        pista_visual[linha][pista_da_nota] = (pista_da_nota + 1) + '0'; // Converte 0-3 para '1'-'4'
                    }
                }
            }
        }

        printf("PISTA 1  2  3  4\n");
        printf("+-- -- -- --+\n");
        for (int i = 0; i < ALTURA_DA_PISTA; i++) {
            printf("| %c  %c  %c  %c |\n", pista_visual[i][0], pista_visual[i][1], pista_visual[i][2], pista_visual[i][3]);
        }
        printf("+-- -- -- --+  <-- ZONA DE ACERTO\n");

        printf("Tempo: %.2f s | Pontos: %d | Combo: x%d\n", tempo_decorrido, game_state.score, game_state.combo);

        frame_time_spent = SDL_GetTicks() - frame_start_time;
        if (frame_time_spent < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time_spent);
        }

        usleep(16000);
    }

    printf("\nA musica terminou! Pontuacao Final: %d\n", game_state.score);
    // Libera a memória da música
    Mix_FreeMusic(musica_de_fundo);
    musica_de_fundo = NULL;

    // Fecha os sistemas de áudio
    Mix_Quit();
    SDL_Quit();

    return 0;
}

void inicializar_jogo(GameState *state) {
    state->score = 0;
    state->combo = 1;
    state->start_time=SDL_GetTicks();
    for (int i = 0; i < state->note_count; i++) {
        state->level_notes[i].foi_processada = 0;
    }
    printf("Jogo iniciado! Pressione Ctrl+C para sair.\n");
}

void carregar_nivel(GameState *state) {
    FILE *file = fopen(LEVEL_FILENAME, "r");
    if (!file) {
        perror("Nao foi possivel abrir o arquivo de nivel");
        exit(1);
    }

    state->note_count = 0;
    float timestamp;
    char note_name[5];
    
    printf("Carregando e mapeando notas para 5 pistas de jogo...\n");

    while (fscanf(file, "%f %s", &timestamp, note_name) == 2) {
        if (state->note_count < MAX_NOTES) {
            int pista_mapeada = -1;

            switch (note_name[0]) {
                case 'C':
                case 'D':
                    pista_mapeada = 0; // Pista 0 (Verde)
                    break;
                case 'E':
                case 'F':
                    pista_mapeada = 1; // Pista 1 (Vermelho)
                    break;
                case 'G':
                case 'A':
                    pista_mapeada = 2; // Pista 2 (Amarelo)
                    break;
                case 'B':
                    pista_mapeada = 3; // Pista 3 (Azul)
                    break;
            }

            // Se a nota foi mapeada para uma pista válida...
            if (pista_mapeada != -1) {
                state->level_notes[state->note_count].timestamp = timestamp;
                strcpy(state->level_notes[state->note_count].note_name, note_name);
                state->level_notes[state->note_count].note_index = pista_mapeada;
                state->level_notes[state->note_count].foi_processada = 0;
                
                // Só incrementa o contador se a nota for usada no jogo
                state->note_count++;
            }
        }
    }

    fclose(file);
    printf("%d notas carregadas e mapeadas para o jogo.\n", state->note_count);
}
