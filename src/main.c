#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_control, LOG_LEVEL_INF);

static const struct device* const uart_console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

#define PWM_PERIOD_US       20000U
#define PWM_STEPS           50U
#define PWM_STEP_US         (PWM_PERIOD_US / PWM_STEPS)

#define BLINK_INTERVAL_MS   500U
#define FADE_DELAY_MS       30U
#define LOG_INTERVAL_MS     100U

typedef enum {
    MODE_DIGITAL = 0,
    MODE_PWM_FADE
} operating_mode_t;

static struct system_state {
    volatile operating_mode_t mode;
    uint32_t duty_cycle_us;
    uint8_t fade_direction;
    int64_t last_log_time;
    bool led_state;
    uint32_t led_brightness_percent;
} state = {
    .mode = MODE_DIGITAL,
    .duty_cycle_us = 0,
    .fade_direction = 1,
    .last_log_time = 0,
    .led_state = false,
    .led_brightness_percent = 0
};

static void simulate_led_state(bool on)
{
    state.led_state = on;
}

static void simulate_led_pwm(uint32_t duty_us)
{
    state.led_brightness_percent = (100 * duty_us) / PWM_PERIOD_US;
}

static void run_digital_mode(void)
{
    state.led_state = !state.led_state;
    simulate_led_state(state.led_state);
    
    int64_t current_time = k_uptime_get();
    if (current_time - state.last_log_time >= LOG_INTERVAL_MS) {
        LOG_INF("Modo DIGITAL - LED: %s %s", 
                state.led_state ? "LIGADO " : "DESLIGADO",
                state.led_state ? "[########]" : "[--------]");
        state.last_log_time = current_time;
    }
    
    k_msleep(BLINK_INTERVAL_MS);
}

static void run_pwm_mode(void)
{
    simulate_led_pwm(state.duty_cycle_us);
    
    int64_t current_time = k_uptime_get();
    if (current_time - state.last_log_time >= LOG_INTERVAL_MS) {
        char bar[21] = {0};
        int filled = (state.led_brightness_percent * 20) / 100;
        for (int i = 0; i < 20; i++) {
            bar[i] = (i < filled) ? '#' : '-';
        }
        
        LOG_INF("Modo PWM - Brilho: %3d%% [%s]", 
                state.led_brightness_percent, bar);
        state.last_log_time = current_time;
    }
    
    if (state.fade_direction == 1) {
        state.duty_cycle_us += PWM_STEP_US;
        if (state.duty_cycle_us >= PWM_PERIOD_US) {
            state.duty_cycle_us = PWM_PERIOD_US;
            state.fade_direction = 0;
            LOG_INF(">>> Brilho máximo atingido (100%%), iniciando FADE OUT");
        }
    } else {
        if (state.duty_cycle_us <= PWM_STEP_US) {
            state.duty_cycle_us = 0;
            state.fade_direction = 1;
            LOG_INF(">>> Brilho mínimo atingido (0%%), iniciando FADE IN");
        } else {
            state.duty_cycle_us -= PWM_STEP_US;
        }
    }
    
    k_msleep(FADE_DELAY_MS);
}

static void switch_mode(void)
{
    if (state.mode == MODE_DIGITAL) {
        // Transição: DIGITAL -> PWM
        state.mode = MODE_PWM_FADE;
        state.duty_cycle_us = 0;
        state.fade_direction = 1;
        
        LOG_INF("================================================");
        LOG_INF("MODO ALTERADO: DIGITAL -> PWM (Fade In/Out)");
        LOG_INF("================================================");
        
    } else {
        // Transição: PWM -> DIGITAL
        state.mode = MODE_DIGITAL;
        
        LOG_INF("================================================");
        LOG_INF("MODO ALTERADO: PWM -> DIGITAL (ON/OFF)");
        LOG_INF("Preparando transição em 2 segundos...");
        LOG_INF("================================================");
        
        k_msleep(2000);
        
        state.led_state = false;
        simulate_led_state(false);
        
        LOG_INF("Modo DIGITAL ativado!");
    }
}


int main(void)
{
    unsigned char uart_char;
    
    if (!device_is_ready(uart_console)) {
        printk("Console UART não está pronto!\n");
        return -1;
    }
    
    LOG_INF("================================================");
    LOG_INF("   SISTEMA DE CONTROLE DE LED COM PWM");
    LOG_INF("   (Versão Simulada para QEMU)");
    LOG_INF("================================================");
    LOG_INF("");
    LOG_INF("Modos disponíveis:");
    LOG_INF("  Modo 1 (DIGITAL): LED pisca ON/OFF");
    LOG_INF("  Modo 2 (PWM): Variação gradual de brilho");
    LOG_INF("");
    LOG_INF("Controles:");
    LOG_INF("  - Pressione ENTER no console para alternar");
    LOG_INF("  - Pressione 'q' + ENTER para sair");
    LOG_INF("");
    LOG_INF("Modo inicial: DIGITAL");
    LOG_INF("================================================");
    LOG_INF("");
    
    while (1) {
        if (!uart_poll_in(uart_console, &uart_char)) {
            if (uart_char == '\n' || uart_char == '\r') {
                switch_mode();
            } else if (uart_char == 'q' || uart_char == 'Q') {
                LOG_INF("");
                LOG_INF("================================================");
                LOG_INF("Encerrando aplicação...");
                LOG_INF("================================================");
                k_msleep(500);
                return 0;
            }
        }
        
        switch (state.mode) {
            case MODE_PWM_FADE:
                run_pwm_mode();
                break;
                
            case MODE_DIGITAL:
            default:
                run_digital_mode();
                break;
        }
    }
    
    return 0;
}
