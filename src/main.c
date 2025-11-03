#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <time.h>
#include <errno.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* ========================================================================
 * DEFINIÇÕES E ESTRUTURAS
 * ======================================================================== */

// Estrutura de mensagem de tempo para o ZBus
struct time_msg {
    int64_t timestamp;       // Unix timestamp em segundos
    int64_t uptime_ms;       // System uptime em milissegundos
    bool is_synchronized;    // Se o tempo foi sincronizado com SNTP
    struct tm time_info;     // Informação de tempo formatada
};

// Configurações do SNTP (podem ser sobrescritas por Kconfig)
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

// Tamanhos de stack e prioridades
#define SNTP_STACK_SIZE 2048
#define LOGGER_STACK_SIZE 1024
#define APP_STACK_SIZE 1024

#define SNTP_PRIORITY 5
#define LOGGER_PRIORITY 6
#define APP_PRIORITY 7

/* ========================================================================
 * CANAL ZBUS
 * ======================================================================== */

// Definição do canal ZBus para mensagens de tempo
ZBUS_CHAN_DEFINE(time_channel,
                 struct time_msg,
                 NULL,
                 NULL,
                 ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));

/* ========================================================================
 * THREAD SNTP CLIENT
 * ======================================================================== */

/**
 * @brief Simula sincronização SNTP e atualiza o relógio do sistema
 * 
 * NOTA: Esta é uma versão simulada. Para usar SNTP real, seria necessário:
 * - CONFIG_NET_IPV4=y ou CONFIG_NET_IPV6=y
 * - CONFIG_NET_SOCKETS=y
 * - CONFIG_DNS_RESOLVER=y
 * - #include <zephyr/net/sntp.h>
 * 
 * @return 0 se sucesso, negativo em caso de erro
 */
static int sync_sntp_time(void)
{
    // Simular timestamp SNTP (começa em 2025-11-02 14:30:00 UTC)
    // e incrementa com o uptime do sistema
    time_t simulated_time = 1730558400 + (k_uptime_get() / 1000);
    
    LOG_INF("Simulando sincronização SNTP com %s", SNTP_SERVER);
    
    // Atualizar relógio do sistema
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

/**
 * @brief Entry point da thread SNTP Client
 */
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
    
    // Aguardar um pouco para o sistema estar pronto
    k_sleep(K_SECONDS(2));
    
    while (1) {
        // Tentar sincronização com servidor SNTP
        ret = sync_sntp_time();
        
        if (ret == 0) {
            // Sincronização bem-sucedida, preparar mensagem
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            
            msg.timestamp = ts.tv_sec;
            msg.uptime_ms = k_uptime_get();
            msg.is_synchronized = true;
            
            // Converter para struct tm (formato legível)
            gmtime_r(&ts.tv_sec, &msg.time_info);
            
            // Publicar no canal ZBus
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
            
            // Publicar mensagem indicando falha na sincronização
            msg.timestamp = 0;
            msg.uptime_ms = k_uptime_get();
            msg.is_synchronized = false;
            memset(&msg.time_info, 0, sizeof(struct tm));
            
            zbus_chan_pub(&time_channel, &msg, K_SECONDS(1));
        }
        
        // Aguardar intervalo antes da próxima sincronização
        k_sleep(K_SECONDS(SNTP_SYNC_INTERVAL));
    }
}

// Definição da thread e stack do SNTP
K_THREAD_STACK_DEFINE(sntp_stack_area, SNTP_STACK_SIZE);
struct k_thread sntp_thread;

/* ========================================================================
 * THREAD LOGGER
 * ======================================================================== */

// Relógio interno do logger
static struct tm logger_clock;
static bool logger_clock_initialized = false;
static int logger_sync_count = 0;

/**
 * @brief Callback chamado quando nova mensagem de tempo é recebida
 */
static void logger_time_callback(const struct zbus_channel *chan)
{
    const struct time_msg *msg = zbus_chan_const_msg(chan);
    
    if (msg->is_synchronized) {
        // Atualizar relógio interno
        memcpy(&logger_clock, &msg->time_info, sizeof(struct tm));
        logger_clock_initialized = true;
        logger_sync_count++;
        
        // Registrar log com timestamp atualizado
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

// Definir listener para o canal de tempo
ZBUS_LISTENER_DEFINE(logger_listener, logger_time_callback);

// Adicionar observer ao canal
ZBUS_CHAN_ADD_OBS(time_channel, logger_listener, 0);

/**
 * @brief Entry point da thread Logger
 */
void logger_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("========================================");
    LOG_INF("Thread Logger iniciada");
    LOG_INF("Aguardando sincronização de tempo...");
    LOG_INF("========================================");
    
    // A thread aguarda eventos ZBus (callbacks automáticos)
    // Pode fazer outras tarefas enquanto isso
    
    while (1) {
        // Sleep para reduzir uso de CPU
        k_sleep(K_SECONDS(15));
        
        if (logger_clock_initialized) {
            // Heartbeat periódico do logger
            LOG_DBG("Logger heartbeat - Relógio interno: %02d:%02d:%02d",
                    logger_clock.tm_hour,
                    logger_clock.tm_min,
                    logger_clock.tm_sec);
            LOG_DBG("Total de sincronizações recebidas: %d", logger_sync_count);
        }
    }
}

// Definição da thread e stack do Logger
K_THREAD_STACK_DEFINE(logger_stack_area, LOGGER_STACK_SIZE);
struct k_thread logger_thread;

/* ========================================================================
 * THREAD APPLICATION
 * ======================================================================== */

// Armazenamento de timestamps de eventos
static int64_t last_event_timestamp = 0;
static int64_t current_timestamp = 0;
static bool time_available = false;
static int event_counter = 0;

/**
 * @brief Callback chamado quando nova mensagem de tempo é recebida
 */
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

// Definir listener para o canal de tempo
ZBUS_LISTENER_DEFINE(app_listener, app_time_callback);

// Adicionar observer ao canal
ZBUS_CHAN_ADD_OBS(time_channel, app_listener, 0);

/**
 * @brief Simula processamento de evento da aplicação
 */
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
    
    // Converter timestamp para formato legível
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
    
    // Aqui você pode adicionar lógica de aplicação real:
    // - Controle de eventos baseado em tempo
    // - Agendamento de tarefas
    // - Cálculo de intervalos para operações periódicas
    // - etc.
}

/**
 * @brief Entry point da thread Application
 */
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
        // Aguardar um período (simulando operação periódica)
        k_sleep(K_SECONDS(30));
        
        if (time_available) {
            // Processar evento periódico
            LOG_INF("Application: Processando evento periódico...");
            process_application_event();
            
            // Calcular próximo evento
            if (current_timestamp > 0) {
                LOG_INF("Application: Próximo evento agendado para daqui a 30 segundos");
            }
        } else {
            LOG_WRN("Application: Aguardando sincronização de tempo para processar eventos");
        }
    }
}

// Definição da thread e stack da Application
K_THREAD_STACK_DEFINE(app_stack_area, APP_STACK_SIZE);
struct k_thread app_thread;

/* ========================================================================
 * MAIN
 * ======================================================================== */

void main(void)
{
    LOG_INF("╔════════════════════════════════════════════════════╗");
    LOG_INF("║                                                    ║");
    LOG_INF("║     Sistema Embarcado - Atividade 04              ║");
    LOG_INF("║     SNTP + ZBus - Sincronização de Tempo          ║");
    LOG_INF("║                                                    ║");
    LOG_INF("╠════════════════════════════════════════════════════╣");
    LOG_INF("║ Arquitetura:                                       ║");
    LOG_INF("║  • Thread SNTP: Sincroniza tempo (Publisher)      ║");
    LOG_INF("║  • Thread Logger: Registra timestamps (Subscriber)║");
    LOG_INF("║  • Thread App: Processa eventos (Subscriber)      ║");
    LOG_INF("║  • Canal ZBus: time_channel                       ║");
    LOG_INF("╚════════════════════════════════════════════════════╝");
    
    LOG_INF("");
    LOG_INF("Inicializando threads do sistema SNTP/ZBus...");
    LOG_INF("");
    
    // Criar thread do cliente SNTP (Publisher)
    k_thread_create(&sntp_thread,
                   sntp_stack_area,
                   K_THREAD_STACK_SIZEOF(sntp_stack_area),
                   sntp_client_entry,
                   NULL, NULL, NULL,
                   SNTP_PRIORITY,
                   0,
                   K_NO_WAIT);
    k_thread_name_set(&sntp_thread, "sntp_client");
    
    // Criar thread do Logger (Subscriber)
    k_thread_create(&logger_thread,
                   logger_stack_area,
                   K_THREAD_STACK_SIZEOF(logger_stack_area),
                   logger_entry,
                   NULL, NULL, NULL,
                   LOGGER_PRIORITY,
                   0,
                   K_NO_WAIT);
    k_thread_name_set(&logger_thread, "logger");
    
    // Criar thread da Application (Subscriber)
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
