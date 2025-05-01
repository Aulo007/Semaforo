/**
 * @file semaforo_pico.c
 * @brief Implementação de um semáforo com modos normal e noturno usando Raspberry Pi Pico
 * 
 * Este programa implementa um semáforo de trânsito com dois modos de operação:
 * - Modo Normal: Ciclo completo verde-amarelo-vermelho
 * - Modo Noturno: Pisca amarelo intermitente
 * 
 * O sistema inclui:
 * - Controle de LEDs RGB para indicação visual
 * - Buzzer para indicação sonora sincronizada com as cores
 * - Display OLED para mostrar informações
 * - Botão para alternar entre os modos de operação
 * - FreeRTOS para gerenciamento de tarefas concorrentes
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include <stdio.h>
#include "lib/leds.h"
#include "lib/matrizRGB.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

/**
 * Definição de constantes e pinos
 */
#define I2C_PORT i2c1           // Porta I2C utilizada
#define I2C_SDA 14              // Pino de dados SDA
#define I2C_SCL 15              // Pino de clock SCL
#define DISPLAY_ADDR 0x3C       // Endereço I2C do display OLED
#define BUZZER_PIN 21           // Pino do buzzer
#define BUZZER_FREQUENCY 100    // Frequência base do buzzer em Hz
#define BOTAO_MODO 5            // Botão para troca de modo (A)
#define BOTAO_RESET 6           // Botão para reset (B)
#define DEBOUNCE_DELAY_MS 300   // Tempo de debounce para o botão em ms

/**
 * Enumeração para os modos de operação do semáforo
 */
typedef enum {
    MODO_NORMAL = 0,            // Funcionamento padrão (verde-amarelo-vermelho)
    MODO_NOTURNO = 1,           // Funcionamento noturno (amarelo piscante)
} ModoOperacao;

/**
 * Enumeração para os estados possíveis do semáforo
 */
typedef enum {
    ESTADO_VERDE = 0,           // Luz verde acesa
    ESTADO_AMARELO = 1,         // Luz amarela acesa
    ESTADO_VERMELHO = 2,        // Luz vermelha acesa
    ESTADO_AMARELO_NOTURNO = 3, // Luz amarela no modo noturno
    ESTADO_DESLIGADO = 4,       // Todas as luzes desligadas
} EstadoSemaforo;

/**
 * Tempos de duração para cada estado no modo normal (em ms)
 */
typedef enum {
    TEMPO_VERDE = 5000,         // Duração do sinal verde
    TEMPO_AMARELO = 2000,       // Duração do sinal vermelho
    TEMPO_VERMELHO = 5000,      // Duração do sinal amarelo
} TempoEstadoNormal;

/**
 * Tempos de ativação do buzzer para cada estado (em ms)
 */
typedef enum {
    DURACAO_BUZZER_VERDE = 100,     // Duração do beep no estado verde
    DURACAO_BUZZER_AMARELO = 100,   // Duração do beep no estado amarelo
    DURACAO_BUZZER_VERMELHO = 500,  // Duração do beep no estado vermelho
    DURACAO_BUZZER_NOTURNO = 1500,  // Duração do beep no modo noturno
} DuracaoBuzzer;

/**
 * Intervalo entre beeps para cada estado (em ms)
 */
typedef enum {
    INTERVALO_BUZZER_VERDE = 1000,      // Intervalo entre beeps no verde
    INTERVALO_BUZZER_AMARELO = 100,     // Intervalo entre beeps no amarelo
    INTERVALO_BUZZER_VERMELHO = 1500,   // Intervalo entre beeps no vermelho
    INTERVALO_BUZZER_NOTURNO = 500,     // Intervalo entre beeps no modo noturno
} IntervaloBuzzer;

/**
 * Variáveis globais compartilhadas entre tasks
 */
// Variáveis de controle do estado atual
volatile ModoOperacao modo_atual = MODO_NORMAL;
volatile EstadoSemaforo estado_atual = ESTADO_VERDE;
volatile uint8_t contador_ciclo = 1;  // Contador para reiniciar o ciclo

// Variáveis de temporização
volatile uint32_t tempo_global = 0;        // Contador de tempo global
volatile uint32_t tempo_ultima_mudanca = 0; // Momento da última mudança de estado
volatile uint32_t tempo_ultimo_botao = 0;   // Momento do último pressionamento do botão
volatile uint32_t tempo_ultimo_beep = 0;    // Momento do último beep do buzzer
volatile bool buzzer_ativo = false;         // Estado atual do buzzer

// Protótipos de funções
void inicializar_buzzer(uint pino);
void ativar_buzzer(uint pino);
void desativar_buzzer(uint pino);

/**
 * @brief Task para manter o contador de tempo global
 * 
 * Esta tarefa mantém um contador de tempo global que é usado por outras tarefas
 * para coordenar mudanças de estado e ações sincronizadas.
 */
void tarefa_contador_tempo() {
    while (true) {
        tempo_global = to_ms_since_boot(get_absolute_time());
    }
}

/**
 * @brief Task para controle da lógica do semáforo e LEDs
 * 
 * Esta tarefa gerencia os estados do semáforo e as transições entre eles,
 * com base no modo atual (normal ou noturno).
 */
void tarefa_controle_semaforo() {
    while (true) {
        if (modo_atual == MODO_NORMAL) {
            // Inicialização do ciclo
            if (contador_ciclo == 1) {
                acender_led_rgb_cor(COLOR_GREEN);
                estado_atual = ESTADO_VERDE;
                contador_ciclo = 2;
                tempo_ultima_mudanca = tempo_global;
            }
            
            // Transição verde -> amarelo
            if ((tempo_global - tempo_ultima_mudanca >= TEMPO_VERDE) && (estado_atual == ESTADO_VERDE)) {
                acender_led_rgb_cor(COLOR_YELLOW);
                estado_atual = ESTADO_AMARELO;
                tempo_ultima_mudanca = tempo_global;
            }
            
            // Transição amarelo -> vermelho
            if ((tempo_global - tempo_ultima_mudanca >= TEMPO_AMARELO) && (estado_atual == ESTADO_AMARELO)) {
                acender_led_rgb_cor(COLOR_RED);
                estado_atual = ESTADO_VERMELHO;
                tempo_ultima_mudanca = tempo_global;
            }
            
            // Transição vermelho -> verde (completa o ciclo)
            if ((tempo_global - tempo_ultima_mudanca >= TEMPO_VERMELHO) && (estado_atual == ESTADO_VERMELHO)) {
                acender_led_rgb_cor(COLOR_GREEN);
                estado_atual = ESTADO_VERDE;
                tempo_ultima_mudanca = tempo_global;
            }
        }
        
        else if (modo_atual == MODO_NOTURNO) {
            // Inicialização do modo noturno
            if (contador_ciclo == 2) {
                acender_led_rgb_cor(COLOR_YELLOW);
                estado_atual = ESTADO_AMARELO_NOTURNO;
                contador_ciclo = 1;
                tempo_ultima_mudanca = tempo_global;
            }
            
            // Transição amarelo noturno -> desligado
            if ((tempo_global - tempo_ultima_mudanca >= DURACAO_BUZZER_NOTURNO) && (estado_atual == ESTADO_AMARELO_NOTURNO)) {
                acender_led_rgb_cor(COLOR_BLACK);
                estado_atual = ESTADO_DESLIGADO;
                tempo_ultima_mudanca = tempo_global;
            }
            
            // Transição desligado -> amarelo noturno
            if ((tempo_global - tempo_ultima_mudanca >= 500) && (estado_atual == ESTADO_DESLIGADO)) {
                acender_led_rgb_cor(COLOR_YELLOW);
                estado_atual = ESTADO_AMARELO_NOTURNO;
                tempo_ultima_mudanca = tempo_global;
            }
        }
        
        // Pequeno delay para não sobrecarregar o processador
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Task para controle do buzzer
 * 
 * Esta tarefa gerencia os sinais sonoros do buzzer, sincronizados
 * com os estados do semáforo, usando o sistema de temporização
 * baseado no tempo global.
 */
void tarefa_controle_buzzer() {
    // Inicializa variáveis de controle
    tempo_ultimo_beep = tempo_global;
    buzzer_ativo = false;
    
    while (true) {
        // Modo normal
        if (modo_atual == MODO_NORMAL) {
            switch (estado_atual) {
                case ESTADO_VERDE:
                    // Controle do beep no estado verde
                    if (!buzzer_ativo && (tempo_global - tempo_ultimo_beep >= INTERVALO_BUZZER_VERDE)) {
                        // Inicia o beep
                        ativar_buzzer(BUZZER_PIN);
                        tempo_ultimo_beep = tempo_global;
                        buzzer_ativo = true;
                    }
                    else if (buzzer_ativo && (tempo_global - tempo_ultimo_beep >= DURACAO_BUZZER_VERDE)) {
                        // Finaliza o beep
                        desativar_buzzer(BUZZER_PIN);
                        buzzer_ativo = false;
                    }
                    break;
                    
                case ESTADO_AMARELO:
                    // Controle do beep no estado amarelo
                    if (!buzzer_ativo && (tempo_global - tempo_ultimo_beep >= INTERVALO_BUZZER_AMARELO)) {
                        // Inicia o beep
                        ativar_buzzer(BUZZER_PIN);
                        tempo_ultimo_beep = tempo_global;
                        buzzer_ativo = true;
                    }
                    else if (buzzer_ativo && (tempo_global - tempo_ultimo_beep >= DURACAO_BUZZER_AMARELO)) {
                        // Finaliza o beep
                        desativar_buzzer(BUZZER_PIN);
                        buzzer_ativo = false;
                    }
                    break;
                    
                case ESTADO_VERMELHO:
                    // Controle do beep no estado vermelho
                    if (!buzzer_ativo && (tempo_global - tempo_ultimo_beep >= INTERVALO_BUZZER_VERMELHO)) {
                        // Inicia o beep
                        ativar_buzzer(BUZZER_PIN);
                        tempo_ultimo_beep = tempo_global;
                        buzzer_ativo = true;
                    }
                    else if (buzzer_ativo && (tempo_global - tempo_ultimo_beep >= DURACAO_BUZZER_VERMELHO)) {
                        // Finaliza o beep
                        desativar_buzzer(BUZZER_PIN);
                        buzzer_ativo = false;
                    }
                    break;
                    
                default:
                    // Estado desconhecido - desliga o buzzer
                    if (buzzer_ativo) {
                        desativar_buzzer(BUZZER_PIN);
                        buzzer_ativo = false;
                    }
                    break;
            }
        }
        // Modo noturno
        else if (modo_atual == MODO_NOTURNO) {
            switch (estado_atual) {
                case ESTADO_AMARELO_NOTURNO:
                    // Buzzer ativo apenas quando o amarelo está aceso
                    if (!buzzer_ativo) {
                        ativar_buzzer(BUZZER_PIN);
                        buzzer_ativo = true;
                        tempo_ultimo_beep = tempo_global;
                    }
                    break;
                    
                case ESTADO_DESLIGADO:
                case ESTADO_VERDE:
                case ESTADO_AMARELO:
                case ESTADO_VERMELHO:
                default:
                    // Desliga o buzzer em qualquer outro estado
                    if (buzzer_ativo) {
                        desativar_buzzer(BUZZER_PIN);
                        buzzer_ativo = false;
                    }
                    break;
            }
        }
        
        // Pequeno delay para não sobrecarregar o processador
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Task para controle do display OLED
 * 
 * Esta tarefa atualiza o display OLED periodicamente com
 * informações sobre o estado atual do semáforo.
 */
void tarefa_controle_display() {
    // Inicialização do I2C
    i2c_init(I2C_PORT, 400 * 1000);  // Configuração para 400kHz
    
    // Configuração dos pinos I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Inicialização do display OLED
    ssd1306_t display;
    ssd1306_init(&display, WIDTH, HEIGHT, false, DISPLAY_ADDR, I2C_PORT);
    ssd1306_config(&display);
    ssd1306_send_data(&display);
    
    // Limpa o display
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);
    
    char buffer_info[64]; // Buffer para informações do display
    
    while (true) {
        // Limpa o display
        ssd1306_fill(&display, false);
        
        // Nome do projeto/autor
        ssd1306_draw_string(&display, "Semaforo", 30, 5);
        
        // Estado atual do semáforo
        const char* estado_texto;
        switch (estado_atual) {
            case ESTADO_VERDE: estado_texto = "Verde"; break;
            case ESTADO_AMARELO: estado_texto = "Amarelo"; break;
            case ESTADO_VERMELHO: estado_texto = "Vermelho"; break;
            case ESTADO_AMARELO_NOTURNO:
            case ESTADO_DESLIGADO: estado_texto = "Piscante"; break;
            default: estado_texto = "Desconhecido"; break;
        }
        
        // Modo atual de operação
        const char* modo_texto = (modo_atual == MODO_NORMAL) ? "Normal" : "Noturno";
        
        // Formata e exibe as informações
        ssd1306_draw_string(&display, modo_texto, 30, 25);
        ssd1306_draw_string(&display, estado_texto, 30, 45);
        
        // Envia os dados para o display
        ssd1306_send_data(&display);
        
        // Atualização a cada 1 segundo
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Task para monitoramento do botão de troca de modo
 * 
 * Esta tarefa verifica o estado do botão e alterna entre os modos
 * normal e noturno quando o botão é pressionado.
 */
void tarefa_monitoramento_botao() {
    while (true) {
        // Verifica se já passou o tempo de debounce desde o último pressionamento
        if ((tempo_global - tempo_ultimo_botao > DEBOUNCE_DELAY_MS) && 
            (gpio_get(BOTAO_MODO) == 0)) {  // Botão pressionado (nível baixo)
            
            // Alterna entre os modos
            modo_atual = (modo_atual == MODO_NORMAL) ? MODO_NOTURNO : MODO_NORMAL;
            
            // Atualiza o timestamp do último pressionamento
            tempo_ultimo_botao = tempo_global;
        }
        
        // Pequeno delay para não sobrecarregar o processador
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Handler de interrupção para o botão de reset
 * 
 * Esta função é chamada quando o botão de reset é pressionado,
 * entrando no modo bootloader USB.
 */
void gpio_irq_handler(uint gpio, uint32_t events) {
    // Entra no modo bootloader USB quando o botão B é pressionado
    reset_usb_boot(0, 0);
}

/**
 * @brief Inicializa o PWM para o buzzer
 * 
 * Configura o pino do buzzer como saída de PWM para controle de som.
 * 
 * @param pino Número do pino GPIO do buzzer
 */
void inicializar_buzzer(uint pino) {
    // Configura o pino como saída de PWM
    gpio_set_function(pino, GPIO_FUNC_PWM);
    
    // Obtém o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pino);
    
    // Configura o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    // Cálculo do divisor para obter a frequência desejada
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096));
    pwm_init(slice_num, &config, true);
    
    // Inicializa o PWM no nível baixo (sem som)
    pwm_set_gpio_level(pino, 0);
}

/**
 * @brief Ativa o buzzer
 * 
 * Configura o PWM para ativar o buzzer.
 * 
 * @param pino Número do pino GPIO do buzzer
 */
void ativar_buzzer(uint pino) {
    // Obtém o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pino);
    
    // Configura o duty cycle para 50% (ativo)
    pwm_set_gpio_level(pino, 2048);
}

/**
 * @brief Desativa o buzzer
 * 
 * Configura o PWM para desativar o buzzer.
 * 
 * @param pino Número do pino GPIO do buzzer
 */
void desativar_buzzer(uint pino) {
    // Obtém o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pino);
    
    // Desativa o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pino, 0);
}

/**
 * @brief Função principal
 * 
 * Inicializa os componentes, cria as tarefas e inicia o scheduler do FreeRTOS.
 */
int main() {
    // Inicialização do sistema
    stdio_init_all();
    
    // Inicialização dos pinos de botões
    gpio_init(BOTAO_MODO);
    gpio_init(BOTAO_RESET);
    gpio_set_dir(BOTAO_MODO, GPIO_IN);
    gpio_set_dir(BOTAO_RESET, GPIO_IN);
    gpio_pull_up(BOTAO_MODO);
    gpio_pull_up(BOTAO_RESET);
    
    // Configura interrupção para o botão de reset
    gpio_set_irq_enabled_with_callback(BOTAO_RESET, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    // Inicialização dos periféricos
    inicializar_buzzer(BUZZER_PIN);
    led_init();  // Inicializa os LEDs
    npInit(7);   // Inicializa a matriz de LEDs RGB no pino 7
    
    // Cria as tarefas do sistema - mantendo a estrutura original do FreeRTOS
    xTaskCreate(tarefa_contador_tempo, "Contador de Tempo", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    
    xTaskCreate(tarefa_controle_semaforo, "Controle do Semaforo", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    
    xTaskCreate(tarefa_controle_buzzer, "Controle do Buzzer", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    
    xTaskCreate(tarefa_controle_display, "Controle do Display", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    
    xTaskCreate(tarefa_monitoramento_botao, "Monitoramento do Botao", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    
    // Inicia o scheduler do FreeRTOS
    vTaskStartScheduler();
    
    // Nunca deve chegar aqui se o scheduler iniciar corretamente
    panic_unsupported();
    return 0;
}