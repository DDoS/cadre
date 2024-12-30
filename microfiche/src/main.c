// #include <pico/stdio_uart.h>
#include <pico/stdio_usb.h>
#include <pico/cyw43_arch.h>

#include <lwip/apps/mdns.h>

// #define WIFI_SSID "( ͡° ͜ʖ ͡°) _optout _nomap"
// #define WIFI_PASSWORD ""

void setup_hostname(const char *hostname) {
    cyw43_arch_lwip_begin();
    struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
    netif_set_hostname(n, hostname);
    netif_set_up(n);
    mdns_resp_init();
    mdns_resp_add_netif(n, hostname);
    cyw43_arch_lwip_end();
}

typedef struct {
    uint8_t (*names)[33];
    size_t count;
} network_list;

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    network_list *networks = env;

    networks->names = realloc(networks->names, (networks->count + 1) * 33);
    memcpy(networks->names[networks->count], result->ssid, 32);
    networks->names[networks->count][result->ssid_len] = '\0';
    networks->count += 1;

    // printf("ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
    //     result->ssid, result->rssi, result->channel,
    //     result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
    //     result->auth_mode);
    return 0;
}

int main() {
    // stdio_uart_init();
    stdio_usb_init();

    if (cyw43_arch_init() != 0) {
        printf("Network architecture initialization failed\n");
        exit(-1);
    }

    cyw43_arch_enable_sta_mode();

    setup_hostname("microfiche");

    cyw43_wifi_scan_options_t scan_options = {};
    network_list networks = {};
    if (cyw43_wifi_scan(&cyw43_state, &scan_options, &networks, &scan_result) != 0) {
        printf("Wifi scan failed\n");
        exit(-1);
    }

    while (cyw43_wifi_scan_active(&cyw43_state)) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
    }

    if (networks.count <= 0) {
        printf("No wifi networks found\n");
        exit(-1);
    }

    printf("Wifi networks found:\n");
    for (size_t i = 0; i < networks.count; i++) {
        printf("  %u: %s\n", i, networks.names[i]);
    }

    size_t index = 0;
    printf("Enter network index: ");
    scanf("%u", &index);
    if (index >= networks.count) {
        printf("Invalid network index\n");
        exit(-1);
    }

    char ssid[33] = {};
    memcpy(ssid, networks.names[index], 33);
    free(networks.names);

    char password[128] = {};
    printf("Enter network password: ");
    scanf("%127s", password);

    printf("Connecting to \"%s\" with password \"%s\" ...\n", ssid, password);
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 10000) != 0) {
        printf("Failed to connect\n");
        exit(-1);
    }
    printf("Connected to \"%s\"\n", ssid);

    while (true) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
    }

    return 0;
}
