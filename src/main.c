#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <time.h>
#include <errno.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

struct time_msg {
    int64_t timestamp;
    int64_t uptime_ms;
    bool is_synchronized;
    struct tm time_info;
};

#ifndef CONFIG_SNTP_SERVER
#define SNTP_SERVER "pool.ntp.org"
#else
#define SNTP_SERVER CONFIG_SNTP_SERVER
#endif

#ifndef CONFIG_SNTP_SYNC_INTERVAL
#define SNTP_SYNC_INTERVAL 60
#else
#define SNTP_SYNC_INTERVAL CONFIG_SNTP_SYNC_INTERVAL
#endif

#define SNTP_STACK_SIZE 2048
#define LOGGER_STACK_SIZE 1024
#define APP_STACK_SIZE 1024

#define SNTP_PRIORITY 5
#define LOGGER_PRIORITY 6
#define APP_PRIORITY 7

ZBUS_CHAN_DEFINE(time_channel,
                 struct time_msg,
                 NULL,
                 NULL,
                 ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));

static int sync_sntp_time(void)
{
    time_t simulated_time = 1730558400 + (k_uptime_get() / 1000);
    
    LOG_INF("Simulando sincronização SNTP com %s", SNTP_SERVER);
    
    struct timespec ts;
    ts.tv_sec = simulated_time;
    ts.tv_nsec = 0;
    
    int ret = clock_settime(CLOCK_REALTIME, &ts);
    if (ret < 0) {
        LOG_ERR("Falha ao configurar relógio: %d", errno);
        return -errno;
    }
    
    LOG_INF("Sincronização SNTP bem-sucedida: %lld.%09ld",
            (long long)ts.tv_sec, ts.tv_nsec);
    
    return 0;
}

void sntp_client_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    struct time_msg msg;
    int ret;
    
    LOG_INF("========================================");
    LOG_INF("Thread SNTP Client iniciada");
    LOG_INF("Servidor: %s", SNTP_SERVER);
    LOG_INF("Intervalo de sincronização: %d segundos", SNTP_SYNC_INTERVAL);
    LOG_INF("NOTA: Usando simulação de SNTP");
    LOG_INF("========================================");
    
    k_sleep(K_SECONDS(2));
    
    while (1){
        ret = sync_sntp_time();
        
        if (ret == 0) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            
            msg.timestamp = ts.tv_sec;
            msg.uptime_ms = k_uptime_get();
            msg.is_synchronized = true;
            
            gmtime_r(&ts.tv_sec, &msg.time_info);
            
            ret = zbus_chan_pub(&time_channel, &msg, K_SECONDS(1));
            if (ret < 0) {
                LOG_ERR("Falha ao publicar no ZBus: %d", ret);
            } else {
                LOG_INF(">>> Horário sincronizado e publicado no ZBus <<<");
                LOG_INF("Data/Hora: %04d-%02d-%02d %02d:%02d:%02d UTC",
                        msg.time_info.tm_year + 1900,
                        msg.time_info.tm_mon + 1,
                        msg.time_info.tm_mday,
                        msg.time_info.tm_hour,
                        msg.time_info.tm_min,
                        msg.time_info.tm_sec);
                LOG_INF("Timestamp: %lld", msg.timestamp);
            }
        } else {
            LOG_WRN("Sincronização SNTP falhou, tentando novamente em %d segundos",
                    SNTP_SYNC_INTERVAL);
            
            msg.timestamp = 0;
            msg.uptime_ms = k_uptime_get();
            msg.is_synchronized = false;
            memset(&msg.time_info, 0, sizeof(struct tm));
            
            zbus_chan_pub(&time_channel, &msg, K_SECONDS(1));
        }
        
        k_sleep(K_SECONDS(SNTP_SYNC_INTERVAL));
    }
}

K_THREAD_STACK_DEFINE(sntp_stack_area, SNTP_STACK_SIZE);
struct k_thread sntp_thread;

static struct tm logger_clock;
static bool logger_clock_initialized = false;
static int logger_sync_count = 0;

static void logger_time_callback(const struct zbus_channel *chan)
{
    const struct time_msg *msg = zbus_chan_const_msg(chan);
    
    if (msg->is_synchronized) {
        memcpy(&logger_clock, &msg->time_info, sizeof(struct tm));
        logger_clock_initialized = true;
        logger_sync_count++;
        
        LOG_INF("╔════════════════════════════════════════╗");
        LOG_INF("║         LOGGER - NOVA ENTRADA          ║");
        LOG_INF("╠════════════════════════════════════════╣");
        LOG_INF("║ Sincronização #%d", logger_sync_count);
        LOG_INF("║ Timestamp: %lld", msg->timestamp);
        LOG_INF("║ Data/Hora: %04d-%02d-%02d %02d:%02d:%02d UTC",
                logger_clock.tm_year + 1900,
                logger_clock.tm_mon + 1,
                logger_clock.tm_mday,
                logger_clock.tm_hour,
                logger_clock.tm_min,
                logger_clock.tm_sec);
        LOG_INF("║ Uptime: %lld ms", msg->uptime_ms);
        LOG_INF("║ Status: ✓ Relógio sincronizado via SNTP");
        LOG_INF("╚════════════════════════════════════════╝");
    } else {
        LOG_WRN("Logger: Recebida mensagem de tempo NÃO sincronizada");
        LOG_WRN("Uptime: %lld ms", msg->uptime_ms);
    }
}

ZBUS_LISTENER_DEFINE(logger_listener, logger_time_callback);

ZBUS_CHAN_ADD_OBS(time_channel, logger_listener, 0);

void logger_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("========================================");
    LOG_INF("Thread Logger iniciada");
    LOG_INF("Aguardando sincronização de tempo...");
    LOG_INF("========================================");
    
    
    while (1) {
        k_sleep(K_SECONDS(15));
        
        if (logger_clock_initialized) {
            LOG_DBG("Logger heartbeat - Relógio interno: %02d:%02d:%02d",
                    logger_clock.tm_hour,
                    logger_clock.tm_min,
                    logger_clock.tm_sec);
            LOG_DBG("Total de sincronizações recebidas: %d", logger_sync_count);
        }
    }
}

K_THREAD_STACK_DEFINE(logger_stack_area, LOGGER_STACK_SIZE);
struct k_thread logger_thread;

static int64_t last_event_timestamp = 0;
static int64_t current_timestamp = 0;
static bool time_available = false;
static int event_counter = 0;

static void app_time_callback(const struct zbus_channel *chan)
{
    const struct time_msg *msg = zbus_chan_const_msg(chan);
    
    if (msg->is_synchronized) {
        int64_t previous_timestamp = current_timestamp;
        current_timestamp = msg->timestamp;
        time_available = true;
        
        LOG_INF("┌──────────────────────────────────────┐");
        LOG_INF("│      APPLICATION - Tempo Recebido    │");
        LOG_INF("├──────────────────────────────────────┤");
        LOG_INF("│ Horário: %04d-%02d-%02d %02d:%02d:%02d UTC",
                msg->time_info.tm_year + 1900,
                msg->time_info.tm_mon + 1,
                msg->time_info.tm_mday,
                msg->time_info.tm_hour,
                msg->time_info.tm_min,
                msg->time_info.tm_sec);
        
        if (previous_timestamp > 0) {
            int64_t time_diff = current_timestamp - previous_timestamp;
            LOG_INF("│ Δt desde última atualização: %lld s", time_diff);
        }
        
        if (last_event_timestamp > 0) {
            int64_t event_diff = current_timestamp - last_event_timestamp;
            LOG_INF("│ Δt desde último evento: %lld s", event_diff);
        }
        
        LOG_INF("└──────────────────────────────────────┘");
    }
}

ZBUS_LISTENER_DEFINE(app_listener, app_time_callback);

ZBUS_CHAN_ADD_OBS(time_channel, app_listener, 0);

static void process_application_event(void)
{
    if (!time_available) {
        LOG_WRN("Application: Tentativa de processar evento sem tempo sincronizado");
        return;
    }
    
    last_event_timestamp = current_timestamp;
    event_counter++;
    
    LOG_INF("┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓");
    LOG_INF("┃   EVENTO DA APLICAÇÃO PROCESSADO     ┃");
    LOG_INF("┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫");
    LOG_INF("┃ Evento #%d", event_counter);
    LOG_INF("┃ Timestamp: %lld", last_event_timestamp);
    
    struct tm event_time;
    gmtime_r(&last_event_timestamp, &event_time);
    LOG_INF("┃ Data/Hora: %04d-%02d-%02d %02d:%02d:%02d UTC",
            event_time.tm_year + 1900,
            event_time.tm_mon + 1,
            event_time.tm_mday,
            event_time.tm_hour,
            event_time.tm_min,
            event_time.tm_sec);
    LOG_INF("┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛");
}

void app_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("========================================");
    LOG_INF("Thread Application iniciada");
    LOG_INF("Aguardando sincronização de tempo...");
    LOG_INF("========================================");
    
    while (1) {
        k_sleep(K_SECONDS(30));
        
        if (time_available) {
            LOG_INF("Application: Processando evento periódico...");
            process_application_event();
            
            if (current_timestamp > 0) {
                LOG_INF("Application: Próximo evento agendado para daqui a 30 segundos");
            }
        } else {
            LOG_WRN("Application: Aguardando sincronização de tempo para processar eventos");
        }
    }
}

K_THREAD_STACK_DEFINE(app_stack_area, APP_STACK_SIZE);
struct k_thread app_thread;

void main(void)
{
    LOG_INF("╔════════════════════════════════════════════════════╗");
    LOG_INF("║                                                    ║");
    LOG_INF("║     Sistema Embarcado - Atividade 04               ║");
    LOG_INF("║     SNTP + ZBus - Sincronização de Tempo           ║");
    LOG_INF("║                                                    ║");
    LOG_INF("╠════════════════════════════════════════════════════╣");
    LOG_INF("║ Arquitetura:                                       ║");
    LOG_INF("║  • Thread SNTP: Sincroniza tempo (Publisher)       ║");
    LOG_INF("║  • Thread Logger: Registra timestamps (Subscriber) ║");
    LOG_INF("║  • Thread App: Processa eventos (Subscriber)       ║");
    LOG_INF("║  • Canal ZBus: time_channel                        ║");
    LOG_INF("╚════════════════════════════════════════════════════╝");
    
    LOG_INF("");
    LOG_INF("Inicializando threads do sistema SNTP/ZBus...");
    LOG_INF("");
    
    k_thread_create(&sntp_thread,
                   sntp_stack_area,
                   K_THREAD_STACK_SIZEOF(sntp_stack_area),
                   sntp_client_entry,
                   NULL, NULL, NULL,
                   SNTP_PRIORITY,
                   0,
                   K_NO_WAIT);
    k_thread_name_set(&sntp_thread, "sntp_client");
    
    k_thread_create(&logger_thread,
                   logger_stack_area,
                   K_THREAD_STACK_SIZEOF(logger_stack_area),
                   logger_entry,
                   NULL, NULL, NULL,
                   LOGGER_PRIORITY,
                   0,
                   K_NO_WAIT);
    k_thread_name_set(&logger_thread, "logger");
    
    k_thread_create(&app_thread,
                   app_stack_area,
                   K_THREAD_STACK_SIZEOF(app_stack_area),
                   app_entry,
                   NULL, NULL, NULL,
                   APP_PRIORITY,
                   0,
                   K_NO_WAIT);
    k_thread_name_set(&app_thread, "application");
    
    LOG_INF("✓ Todas as threads iniciadas com sucesso!");
    LOG_INF("");
    LOG_INF("╔════════════════════════════════════════════════════╗");
    LOG_INF("║          Sistema em operação                       ║");
    LOG_INF("╚════════════════════════════════════════════════════╝");
}
