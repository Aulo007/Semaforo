#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include "pico/time.h"
#include "hardware/timer.h"
#include "lib/ssd1306.h"
#include "lib/matrizRGB.h"
#include "buzzer.h"
#include "extras/Desenho.h"

#define ADC_JOYSTICK_X 26
#define ADC_JOYSTICK_Y 27
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12
#define LED_RED_PIN 13
#define Button_B 6
#define MAX_JOYSTICK_VALUE 4074 // Maior Valor encontrando por meio de debbugação
#define MIN_JOYSTICK_VALUE 11   // Menor Valor encontrado por meio de debbugação
volatile int Center_Joystick_Y = 0;
volatile int Center_Joystick_X = 0;
volatile int cont = 0;

#define I2C_PORT i2c1     // Porta I2C utilizada
#define I2C_SDA 14        // Pino de dados SDA
#define I2C_SCL 15        // Pino de clock SCL
#define DISPLAY_ADDR 0x3C // Endereço I2C do display OLED

#define BUZZER_PIN 21 // Pino do buzzer

volatile int contador = 10;

// Função para controlar estado em que o sistema se encontra.
volatile uint8_t mode = 0;

QueueHandle_t xQueueJoystickData;
QueueHandle_t xQueueSensorDataDisplay;
QueueHandle_t xQueueSensorDataMatriz;

void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

typedef struct
{
    uint16_t x_pos;
    uint16_t y_pos;
} joystick_data_t;

typedef struct
{
    float water_level;
    float rain_level;
} sensor_data_t;

void vManagerTask(void *params);
void vJoystickTask(void *params);
void vConvertTask(void *params);
void vDisplayControlTask(void *params);
void vMatrizControlTask(void *params);

int main()
{
    // Ativa BOOTSEL via botão
    gpio_init(Button_B);
    gpio_set_dir(Button_B, GPIO_IN);
    gpio_pull_up(Button_B);
    gpio_set_irq_enabled_with_callback(Button_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    stdio_init_all();
    sleep_ms(1000); // tempo para terminal abrir via USB

    // Cria a fila para compartilhamento de valor do joystick
    xQueueJoystickData = xQueueCreate(10, sizeof(joystick_data_t));
    xQueueSensorDataDisplay = xQueueCreate(10, sizeof(sensor_data_t));
    xQueueSensorDataMatriz = xQueueCreate(10, sizeof(sensor_data_t));

    // Criação das tasks
    xTaskCreate(vManagerTask, "Manager Task", 256, NULL, 1, NULL);
    xTaskCreate(vJoystickTask, "Joystick Task", 256, NULL, 1, NULL);
    xTaskCreate(vConvertTask, "Convert Task", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayControlTask, "Display Control Task", 256, NULL, 1, NULL);
    xTaskCreate(vMatrizControlTask, "Matriz Control Task", 256, NULL, 1, NULL);

    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}

void vManagerTask(void *params)
{
    while (true)
    {
        if (mode == 0)
        {
            printf("oi\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 segundo
        }
    }
}
void vJoystickTask(void *params)
{
    adc_gpio_init(ADC_JOYSTICK_X);
    adc_gpio_init(ADC_JOYSTICK_Y);
    adc_init();

    joystick_data_t joystick_data;

    // Fase de inicialização - aguarda 1 segundo descartando leituras
    printf("Inicializando ADC - aguardando estabilização (1 segundo)...\n");

    absolute_time_t end_time = make_timeout_time_ms(2000); // 2 segundos

    while (!time_reached(end_time))
    {
        // Continua fazendo leituras e descartando durante o período de aquecimento
        adc_select_input(0);
        uint16_t dummy_y = adc_read();
        adc_select_input(1);
        uint16_t dummy_x = adc_read();
        printf("Descartando leitura: X=%u, Y=%u\n", dummy_x, dummy_y);
        vTaskDelay(pdMS_TO_TICKS(50)); // Pequenas pausas para permitir outras tarefas
        Center_Joystick_Y = Center_Joystick_Y + dummy_y;
        Center_Joystick_X = Center_Joystick_X + dummy_x;
        cont++;
    }

    Center_Joystick_Y = Center_Joystick_Y / cont;
    Center_Joystick_X = Center_Joystick_X / cont;

    printf("Inicialização completa - iniciando leituras reais\n");

    // Operação normal
    while (true)
    {
        adc_select_input(0);
        joystick_data.y_pos = adc_read();
        adc_select_input(1);
        joystick_data.x_pos = adc_read();

        // Debbugando valores do joystick
        // printf("[Tarefa: %s] Leitura do joystick - X: %u, Y: %u\n",
        //        pcTaskGetName(NULL),
        //        joystick_data.x_pos,
        //        joystick_data.y_pos);

        xQueueSend(xQueueJoystickData, &joystick_data, portMAX_DELAY);
    }
}

float map_joystick_to_percentage(int value, int min, int center, int max)
{
    if (value < center)
        return ((float)(value - min) / (center - min)) * 50.0f;
    else
        return 50.0f + ((float)(value - center) / (max - center)) * 50.0f;
}

void vConvertTask(void *params)
{

    sensor_data_t sensor_data;

    while (true)
    {
        joystick_data_t joystick_data;
        xQueueReceive(xQueueJoystickData, &joystick_data, portMAX_DELAY);
        sensor_data.water_level = map_joystick_to_percentage(joystick_data.y_pos, MIN_JOYSTICK_VALUE, Center_Joystick_Y, MAX_JOYSTICK_VALUE);
        sensor_data.rain_level = map_joystick_to_percentage(joystick_data.x_pos, MIN_JOYSTICK_VALUE, Center_Joystick_X, MAX_JOYSTICK_VALUE);
        printf("[Tarefa: %s] Nível de água: %.2f%%, Nível de chuva: %.2f%%\n",
               pcTaskGetName(NULL),
               sensor_data.water_level,
               sensor_data.rain_level);
        xQueueSend(xQueueSensorDataDisplay, &sensor_data, portMAX_DELAY);
        xQueueSend(xQueueSensorDataMatriz, &sensor_data, portMAX_DELAY);
    }
}

void vDisplayControlTask(void *params)
{
    // Inicialização do I2C
    i2c_init(I2C_PORT, 400 * 1000); // Configuração para 400kHz

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
    sensor_data_t sensor_data;

    absolute_time_t repeat_time = make_timeout_time_ms(50); // 1 segundo

    while (xQueueReceive(xQueueSensorDataDisplay, &sensor_data, portMAX_DELAY) == pdPASS) //
    {
        if (time_reached(repeat_time))
        {
            repeat_time = make_timeout_time_ms(50); // Atualiza o tempo de repetição
            sprintf(buffer_info, "Agua: %.1f%%", sensor_data.water_level);
            ssd1306_draw_string(&display, buffer_info, 0, 0);

            sprintf(buffer_info, "Chuva: %.1f%%", sensor_data.rain_level);
            ssd1306_draw_string(&display, buffer_info, 0, 20);

            ssd1306_send_data(&display);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vMatrizControlTask(void *params)
{
    // Inicializa a matriz RGB
    npInit(7);

    sensor_data_t sensor_data;

    while (xQueueReceive(xQueueSensorDataMatriz, &sensor_data, portMAX_DELAY) == pdPASS)
    {
        if (sensor_data.water_level >= 70.0f || sensor_data.rain_level >= 80.0f)
        {
            npSetMatrixWithIntensity(caixa_de_desenhos[contador], 1); // Exibe imagem de alerta

            contador++;
            if (contador >= 22)
                contador = 10;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        else
        {
            npClear();
        }
    }
}
