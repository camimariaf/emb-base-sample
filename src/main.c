#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(periodic_activity, LOG_LEVEL_INF);

static struct k_timer periodic_timer;

static uint32_t current_cycle_count = 0;

static void log_activity(void)
{
	LOG_INF("Hello World! Ciclo #%u", current_cycle_count);

	printk("Hello World! Ciclo #%u (via printk)\n", current_cycle_count);

	LOG_DBG("Configuração: Intervalo de %d ms", CONFIG_ACTIVITY_INTERVAL_MS);

	if ((current_cycle_count % 7U) == 0U) {
		LOG_ERR("Erro simulado: Falha de comunicação no ciclo %u", current_cycle_count);
	}
}


static void periodic_timer_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	
	current_cycle_count++;
	log_activity();
}

int main(void)
{
	k_timer_init(&periodic_timer, periodic_timer_handler, NULL);
	
	k_timer_start(&periodic_timer, 
	              K_MSEC(CONFIG_ACTIVITY_INTERVAL_MS), 
	              K_MSEC(CONFIG_ACTIVITY_INTERVAL_MS));
	return 0;
}

