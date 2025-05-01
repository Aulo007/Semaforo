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

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define BUZZER_PIN 21
#define BUZZER_FREQUENCY 100

typedef enum
{
    MODO_NORMAL = 0,
    MODO_NORTURNO = 1,
} MODO;

typedef enum
{
    verde = 0,
    amarelo = 1,
    vermelho = 2,
    vermelho_noturno = 3,
} ESTADO_SEMAFORO;

typedef enum
{
    tempo_verde = 5000,
    tempo_amarelo = 2000,
    tempo_vermelho = 5000,
} MODO_NORMAL_TEMPO;

typedef enum
{
    tempo_buzzer_verde = 100,
    tempo_buzzer_amarelo = 100,
    tempo_buzzer_vermelho = 500,
    tempo_buzzer_amarelo_noturno = 1500,
} TEMPO_BUZZER;

void pwm_init_buzzer(uint pin);
void beep(uint pin, uint duration_ms);

volatile MODO modo_atual = MODO_NORMAL;
volatile MODO_NORMAL_TEMPO tempo_atual = tempo_verde;
volatile int contador = 0;
volatile ESTADO_SEMAFORO estado_semaforo = verde;
volatile bool button_pressed = false;

void vBlinkLed1Task()
{
    while (1) // Modificado para sempre executar
    {
        if (modo_atual == MODO_NORMAL)
        {
            acender_led_rgb_cor(COLOR_GREEN); // Verde
            vTaskDelay(pdMS_TO_TICKS(tempo_verde));
            estado_semaforo = amarelo;
            acender_led_rgb_cor(COLOR_YELLOW);
            vTaskDelay(pdMS_TO_TICKS(tempo_amarelo));
            estado_semaforo = vermelho;
            acender_led_rgb_cor(COLOR_RED);
            vTaskDelay(pdMS_TO_TICKS(tempo_vermelho));
            estado_semaforo = verde;
        }
        else
        {
            // Adicionar um pequeno delay para não sobrecarregar o processador
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void vModoNoturno()
{
    while (true)
    {
        if (modo_atual == MODO_NORTURNO)
        {
            acender_led_rgb_cor(COLOR_YELLOW); // Vermelho noturno
            vTaskDelay(pdMS_TO_TICKS(tempo_buzzer_amarelo_noturno / 2));
            turn_off_leds();
        }
    }
}

void vBlinkLed2Task()
{
    while (1) // Modificado para sempre executar
    {
        if (modo_atual == MODO_NORMAL)
        {
            switch (estado_semaforo)
            {
            case verde:
                beep(BUZZER_PIN, tempo_buzzer_verde);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            case amarelo:
                beep(BUZZER_PIN, tempo_buzzer_amarelo);
                vTaskDelay(pdMS_TO_TICKS(tempo_buzzer_amarelo));
                break;
            case vermelho:
                beep(BUZZER_PIN, tempo_buzzer_vermelho);
                vTaskDelay(pdMS_TO_TICKS(1500));
                break;

            default:
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            }
        }
        else if (modo_atual == MODO_NORTURNO)
        {
            beep(BUZZER_PIN, tempo_buzzer_amarelo_noturno);
            vTaskDelay(pdMS_TO_TICKS(tempo_buzzer_vermelho));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void vDisplay3Task()
{
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_t ssd;                                                // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    while (1) // Adicionado loop infinito para não terminar a task
    {
        // Aqui você pode adicionar código para atualizar o display
        vTaskDelay(pdMS_TO_TICKS(1000));                 // Delay de 1 segundo
        ssd1306_fill(&ssd, false);                       // Limpa o display
        ssd1306_draw_string(&ssd, "Aulo Cezar", 60, 28); // Desenha o texto no display
        ssd1306_send_data(&ssd);                         // Envia os dados para o display
    }
}

#define botao_A 5
#define DEBOUNCE_DELAY_MS 200 // Atraso para evitar múltiplas detecções
static uint32_t last_button_press_time = 0;

void vTrocaModo()
{

    while (true)
    {
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        if (current_time - last_button_press_time > DEBOUNCE_DELAY_MS)

            if (gpio_get(botao_A) == 0)
            {

                modo_atual = (modo_atual == MODO_NORMAL) ? MODO_NORTURNO : MODO_NORMAL;
                last_button_press_time = current_time;
            }
    }
}

#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botao_A);
    gpio_init(botaoB);
    gpio_set_dir(botao_A, GPIO_IN);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botao_A);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // Fim do trecho para modo BOOTSEL com botão B

    stdio_init_all();

    // Inicialização do buzzer
    pwm_init_buzzer(BUZZER_PIN);
    led_init(); // Inicializa os LEDs
    npInit(7);  // Inicializa a matriz de LEDs

    // Tasks descomentadas
    xTaskCreate(vBlinkLed1Task, "Blink Task Led1", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vBlinkLed2Task, "Blink Task Led2", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vDisplay3Task, "Cont Task Disp3", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTrocaModo, "Troca Modo", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    vTaskStartScheduler();
    panic_unsupported();
}

// Definição de uma função para inicializar o PWM no pino do buzzer
void pwm_init_buzzer(uint pin)
{
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
}

// Definição de uma função para emitir um beep com duração especificada
void beep(uint pin, uint duration_ms)
{
    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o duty cycle para 50% (ativo)
    pwm_set_gpio_level(pin, 2048);

    // Temporização
    sleep_ms(duration_ms);

    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pin, 0);

    // Pausa entre os beeps
    sleep_ms(100); // Pausa de 100ms
}