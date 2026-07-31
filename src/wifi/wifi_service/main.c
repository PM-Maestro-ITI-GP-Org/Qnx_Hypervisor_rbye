#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdint.h>
#include <sys/sockio.h>
#include <net/route.h>
#include <sys/select.h>
#include <time.h>
#include <sys/stat.h>

#define WIFI_IFACE      "bcm0"
#define PHONE_PORT      9999
#define BUFFER_SIZE     4096

#define DEFAULT_CONF    "/etc/wifi/wpa_supplicant_default.conf"
#define REAL_CONF       "/etc/wifi/wpa_supplicant_real.conf"
#define DEFAULT_SSID    "QNX_wifi"
#define DEFAULT_PASS    "123456789"

#define TIMEOUT_REAL_MS     25000
#define TIMEOUT_DEFAULT_MS  15000

#define LED_GPIO_PIN        17
#define LED_DEV_PATH        "/dev/gpio/"

static int g_running = 1;
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv  = PTHREAD_COND_INITIALIZER;

typedef enum {
    STATE_INIT,
    STATE_TRY_REAL,
    STATE_ON_REAL,
    STATE_TRY_DEFAULT,
    STATE_ON_DEFAULT,
    STATE_EXIT
} state_t;

static state_t g_state = STATE_INIT;
static int g_creds_received = 0;
static char g_new_ssid[128] = {0};
static char g_new_pass[128] = {0};

void set_state(state_t s)
{
    pthread_mutex_lock(&g_mtx);
    g_state = s;
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mtx);
}

state_t get_state(void)
{
    state_t s;
    pthread_mutex_lock(&g_mtx);
    s = g_state;
    pthread_mutex_unlock(&g_mtx);
    return s;
}

void run(const char *cmd)
{
    system(cmd);
}

// --- Config file management ---

void write_default_conf(void)
{
    FILE *f = fopen(DEFAULT_CONF, "w");
    if (!f) return;
    fprintf(f, "ctrl_interface=/var/run/wpa_supplicant\n");
    fprintf(f, "ap_scan=1\n");
    fprintf(f, "network={\n");
    fprintf(f, "    ssid=\"%s\"\n", DEFAULT_SSID);
    fprintf(f, "    psk=\"%s\"\n", DEFAULT_PASS);
    fprintf(f, "    key_mgmt=WPA-PSK\n");
    fprintf(f, "}\n");
    fclose(f);
}

void write_real_conf(const char *ssid, const char *pass)
{
    FILE *f = fopen(REAL_CONF, "w");
    if (!f) return;
    fprintf(f, "ctrl_interface=/var/run/wpa_supplicant\n");
    fprintf(f, "ap_scan=1\n");
    fprintf(f, "network={\n");
    fprintf(f, "    ssid=\"%s\"\n", ssid);
    fprintf(f, "    psk=\"%s\"\n", pass);
    fprintf(f, "    key_mgmt=WPA-PSK\n");
    fprintf(f, "}\n");
    fclose(f);
    printf("[CFG] Real WiFi config written: '%s'\n", ssid);
}

int conf_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

void sleep_or_interrupted(int seconds)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += seconds;
    pthread_mutex_lock(&g_mtx);
    pthread_cond_timedwait(&g_cv, &g_mtx, &ts);
    pthread_mutex_unlock(&g_mtx);
}

// --- LED control ---

void led_init(void)
{
    char path[64];
    snprintf(path, sizeof(path), LED_DEV_PATH "%d", LED_GPIO_PIN);
    FILE *f = fopen(path, "w");
    if (!f) { printf("[LED] GPIO not available (rpi_gpio running?)\n"); return; }
    fprintf(f, "out");
    fclose(f);
    printf("[LED] GPIO %d configured as output\n", LED_GPIO_PIN);
}

void led_set(int on)
{
    char path[64];
    snprintf(path, sizeof(path), LED_DEV_PATH "%d", LED_GPIO_PIN);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%s", on ? "on" : "off");
    fclose(f);
    printf("[LED] %s\n", on ? "ON" : "OFF");
}

// --- WiFi control ---

void wifi_stop(void)
{
    run("slay -f wpa_supplicant 2>/dev/null");
    run("dhcpcd -k " WIFI_IFACE " 2>/dev/null");
    usleep(500000);
}

void wifi_start(const char *conf_path)
{
    wifi_stop();
    run("ifconfig " WIFI_IFACE " down 2>/dev/null");
    usleep(100000);
    run("gpio-rp1 set 32 op pd dl 2>/dev/null");
    usleep(300000);
    run("gpio-rp1 set 32 op pd dh 2>/dev/null");
    usleep(1500000);
    run("ifconfig " WIFI_IFACE " up 2>/dev/null");
    int i;
    for (i = 0; i < 10; i++) {
        char buf[16] = {0};
        FILE *fp = popen("ifconfig " WIFI_IFACE " 2>/dev/null | grep -c 'UP'", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp) && atoi(buf) > 0) {
                pclose(fp);
                break;
            }
            pclose(fp);
        }
        usleep(200000);
    }
    usleep(300000);
    run("mkdir -p /var/run/wpa_supplicant");
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "wpa_supplicant -D qwdi -i %s -c %s -B",
             WIFI_IFACE, conf_path);
    run(cmd);
    usleep(1000000);
    run("dhcpcd -b " WIFI_IFACE);
}

int wait_for_connect(int timeout_ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&g_mtx);
    int done = 0;
    while (!done && g_running) {
        if (g_state == STATE_ON_REAL || g_state == STATE_ON_DEFAULT) {
            done = 1;
            break;
        }
        int rc = pthread_cond_timedwait(&g_cv, &g_mtx, &ts);
        if (rc == ETIMEDOUT) break;
        if (g_state == STATE_ON_REAL || g_state == STATE_ON_DEFAULT) {
            done = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_mtx);
    return done;
}

// --- wpa_cli event monitor thread (polling) ---

void *wpa_monitor_thread(void *arg)
{
    printf("[MON] wpa_cli poll monitor started\n");
    int was_connected = 0;
    while (g_running) {
        FILE *fp = popen("wpa_cli -i " WIFI_IFACE " status 2>/dev/null | grep 'wpa_state='", "r");
        int connected = 0;
        if (fp) {
            char buf[64] = {0};
            if (fgets(buf, sizeof(buf), fp) && strstr(buf, "COMPLETED"))
                connected = 1;
            pclose(fp);
        }

        state_t s = get_state();
        if (connected && !was_connected) {
            printf("[MON] WiFi connected\n");
            if (s == STATE_TRY_REAL)
                set_state(STATE_ON_REAL);
            else if (s == STATE_TRY_DEFAULT)
                set_state(STATE_ON_DEFAULT);
        }
        if (!connected && was_connected) {
            printf("[MON] WiFi disconnected!\n");
            if (s == STATE_ON_REAL)
                set_state(STATE_TRY_DEFAULT);
            else if (s == STATE_ON_DEFAULT)
                set_state(STATE_TRY_REAL);
        }
        was_connected = connected;
        sleep(1);
    }
    printf("[MON] wpa_cli poll monitor exited\n");
    return NULL;
}

void start_wpa_monitor(void)
{
    pthread_t t;
    pthread_create(&t, NULL, wpa_monitor_thread, NULL);
    pthread_detach(t);
}

// --- Gateway detection ---

int get_gateway_ip(char *buf, size_t len)
{
    FILE *fp = popen("netstat -rn 2>/dev/null | grep 'default'", "r");
    if (!fp) return -1;
    char line[128];
    if (!fgets(line, sizeof(line), fp)) { pclose(fp); return -1; }
    pclose(fp);
    // Parse: "  default   x.x.x.x  ..."
    char *p = line;
    while (*p && *p == ' ') p++;
    while (*p && *p != ' ') p++;
    while (*p && *p == ' ') p++;
    if (!*p) return -1;
    char *gw = p;
    while (*p && *p != ' ') p++;
    *p = '\0';
    snprintf(buf, len, "%s", gw);
    return 1;
}

int guess_gateway_from_ip(const char *my_ip, char *gw, size_t gw_len)
{
    snprintf(gw, gw_len, "%s", my_ip);
    char *dot = strrchr(gw, '.');
    if (!dot) return -1;
    dot[1] = '1';
    dot[2] = '\0';
    return 0;
}

// --- Credential receiver thread ---

void *credential_receiver(void *arg)
{
    char phone_ip[64];
    strncpy(phone_ip, (const char *)arg, sizeof(phone_ip) - 1);
    free(arg);

    printf("[CRED] Connecting to phone at %s:%d...\n", phone_ip, PHONE_PORT);

    while (g_running && get_state() == STATE_ON_DEFAULT) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { sleep_or_interrupted(2); continue; }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PHONE_PORT);
        inet_pton(AF_INET, phone_ip, &addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(sock);
            sleep_or_interrupted(1);
            continue;
        }

        printf("[CRED] Connected to phone!\n");

        // Send greeting (newline-terminated for Java readLine)
        char greet[] = "{\"type\":\"rpi_ready\"}\n";
        write(sock, greet, strlen(greet));

        // Receive credentials line
        FILE *fp = fdopen(sock, "r");
        if (!fp) { close(sock); sleep_or_interrupted(1); continue; }

        char buf[BUFFER_SIZE] = {0};
        if (!fgets(buf, sizeof(buf), fp)) {
            fclose(fp);
            sleep_or_interrupted(1);
            continue;
        }

        // Trim trailing newline
        char *nl = strchr(buf, '\n');
        if (nl) *nl = '\0';

        printf("[CRED] Received: %s\n", buf);

        // Parse JSON: {"ssid":"...","password":"..."}
        char ssid[128] = {0}, pass[128] = {0};
        char *p = strstr(buf, "\"ssid\"");
        if (p) {
            p = strchr(p, ':');
            if (p) {
                p++;
                while (*p == ' ' || *p == '\t' || *p == '"') p++;
                int i = 0;
                while (*p && *p != '"' && i < 127) ssid[i++] = *p++;
            }
        }
        p = strstr(buf, "\"password\"");
        if (p) {
            p = strchr(p, ':');
            if (p) {
                p++;
                while (*p == ' ' || *p == '\t' || *p == '"') p++;
                int i = 0;
                while (*p && *p != '"' && i < 127) pass[i++] = *p++;
            }
        }

        if (ssid[0]) {
            pthread_mutex_lock(&g_mtx);
            g_creds_received = 1;
            memcpy(g_new_ssid, ssid, sizeof(g_new_ssid) - 1);
            g_new_ssid[sizeof(g_new_ssid) - 1] = '\0';
            memcpy(g_new_pass, pass, sizeof(g_new_pass) - 1);
            g_new_pass[sizeof(g_new_pass) - 1] = '\0';
            pthread_cond_broadcast(&g_cv);
            pthread_mutex_unlock(&g_mtx);

            printf("[CRED] Received real WiFi: '%s' / '%s'\n", ssid, pass);

            // Send acknowledgement
            char ack[256];
            snprintf(ack, sizeof(ack), "{\"status\":\"received\",\"ssid\":\"%s\"}\n", ssid);
            write(sock, ack, strlen(ack));

            fclose(fp);
            return NULL;
        }

        printf("[CRED] Invalid response, retrying...\n");
        fclose(fp);
        sleep_or_interrupted(1);
    }

    printf("[CRED] Exiting (state changed or shutdown)\n");
    return NULL;
}

// --- State machine functions ---

void state_try_real(void)
{
    printf("\n=== STATE: TRY_REAL ===\n");

    if (!conf_exists(REAL_CONF)) {
        printf("[REAL] No real config yet, skipping to default...\n");
        set_state(STATE_TRY_DEFAULT);
        return;
    }

    printf("[REAL] Starting wpa_supplicant with real config...\n");
    wifi_start(REAL_CONF);
    start_wpa_monitor();

    printf("[REAL] Waiting up to %dms for connection...\n", TIMEOUT_REAL_MS);
    int ok = wait_for_connect(TIMEOUT_REAL_MS);

    if (ok && get_state() == STATE_ON_REAL) {
        printf("[REAL] Connected to real WiFi!\n");
        led_set(1);
    } else {
        printf("[REAL] Failed to connect to real WiFi\n");
        wifi_stop();
        set_state(STATE_TRY_DEFAULT);
    }
}

void state_on_real(void)
{
    printf("\n=== STATE: ON_REAL ===\n");

    pthread_mutex_lock(&g_mtx);
    while (g_state == STATE_ON_REAL && g_running)
        pthread_cond_wait(&g_cv, &g_mtx);
    pthread_mutex_unlock(&g_mtx);

    led_set(0);
    printf("[REAL] Connection lost, retrying...\n");
    wifi_stop();
}

void state_try_default(void)
{
    printf("\n=== STATE: TRY_DEFAULT ===\n");

    write_default_conf();
    printf("[DEF] Starting wpa_supplicant with default hotspot config...\n");
    wifi_start(DEFAULT_CONF);
    start_wpa_monitor();

    printf("[DEF] Waiting up to %dms for connection...\n", TIMEOUT_DEFAULT_MS);
    int ok = wait_for_connect(TIMEOUT_DEFAULT_MS);

    if (ok && get_state() == STATE_ON_DEFAULT) {
        printf("[DEF] Connected to phone hotspot!\n");
    } else {
        printf("[DEF] Failed to connect to hotspot\n");
        wifi_stop();
        sleep_or_interrupted(10);
        set_state(STATE_TRY_REAL);
    }
}

void state_on_default(void)
{
    printf("\n=== STATE: ON_DEFAULT ===\n");

    // Detect phone IP — poll for up to 15s waiting for gateway/IP
    char phone_ip_buf[64] = {0};
    int found = 0;
    int tries;

    for (tries = 0; tries < 6 && g_running; tries++) {
        if (get_gateway_ip(phone_ip_buf, sizeof(phone_ip_buf)) == 1) {
            printf("[DEF] Phone IP from gateway: %s\n", phone_ip_buf);
            found = 1;
            break;
        }
        // Fallback: parse our own IP from ifconfig
        FILE *fi = popen("ifconfig " WIFI_IFACE " 2>/dev/null | grep 'inet '", "r");
        if (fi) {
            char line[128];
            if (fgets(line, sizeof(line), fi)) {
                char *p = strstr(line, "inet ");
                if (p) {
                    p += 5;
                    while (*p == ' ') p++;
                    char *ip = p;
                    while (*p && *p != ' ') p++;
                    *p = '\0';
                    if (guess_gateway_from_ip(ip, phone_ip_buf, sizeof(phone_ip_buf)) == 0) {
                        printf("[DEF] Phone IP guessed from %s (try %d)\n", ip, tries + 1);
                        found = 1;
                        break;
                    }
                }
            }
            pclose(fi);
        }
        printf("[DEF] Waiting for IP (try %d/6)...\n", tries + 1);
        sleep_or_interrupted(1);
    }

    if (!found) {
        printf("[DEF] Cannot determine phone IP after %d tries\n", tries);
        sleep_or_interrupted(10);
        set_state(STATE_TRY_DEFAULT);
        return;
    }

    // Start credential receiver thread
    char *phone_ip = strdup(phone_ip_buf);
    pthread_t t;
    pthread_create(&t, NULL, credential_receiver, phone_ip);
    pthread_detach(t);

    // Wait for either credentials received or state change (disconnect)
    pthread_mutex_lock(&g_mtx);
    while (g_state == STATE_ON_DEFAULT && g_running && !g_creds_received)
        pthread_cond_wait(&g_cv, &g_mtx);
    int creds = g_creds_received;
    if (creds) {
        g_creds_received = 0;
    }
    pthread_mutex_unlock(&g_mtx);

    if (creds) {
        printf("[DEF] Credentials received, switching to real WiFi...\n");
        write_real_conf(g_new_ssid, g_new_pass);
        memset(g_new_ssid, 0, sizeof(g_new_ssid));
        memset(g_new_pass, 0, sizeof(g_new_pass));
        wifi_stop();
        set_state(STATE_TRY_REAL);
    } else {
        printf("[DEF] Leaving default (state changed)\n");
        wifi_stop();
    }
}

// --- Signal handler ---

void sighandler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[SRV] Shutting down...\n");
        g_running = 0;
        pthread_cond_broadcast(&g_cv);
    }
}

// --- Main ---

int main(void)
{
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    write_default_conf();

    printf("=====================================================\n");
    printf("  QNX WiFi Auto-Config Service\n");
    printf("  WIFI_IFACE: %s\n", WIFI_IFACE);
    printf("  Default hotspot: %s / %s\n", DEFAULT_SSID, DEFAULT_PASS);
    printf("  Config files:\n");
    printf("    Real:    %s\n", REAL_CONF);
    printf("    Default: %s\n", DEFAULT_CONF);
    printf("  Phone port: %d\n", PHONE_PORT);
    printf("=====================================================\n");

    while (g_running) {
        switch (get_state()) {
            case STATE_INIT:
                printf("[MAIN] Starting up - will try real WiFi first...\n");
                set_state(STATE_TRY_REAL);
                break;
            case STATE_TRY_REAL:
                state_try_real();
                break;
            case STATE_ON_REAL:
                state_on_real();
                break;
            case STATE_TRY_DEFAULT:
                state_try_default();
                break;
            case STATE_ON_DEFAULT:
                state_on_default();
                break;
            case STATE_EXIT:
                g_running = 0;
                break;
        }
    }

    wifi_stop();
    printf("[SRV] Exited.\n");
    return 0;
}
