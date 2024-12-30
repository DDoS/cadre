#include <pico/stdio_uart.h>
#include <pico/cyw43_arch.h>

#include <picow_http/http.h>

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

void server_forever() {
    struct server *srv;
    struct server_cfg cfg;
    err_t err;

    /*
     * Start with the default configuration for the HTTP server.
     *
     * If a list of NTP servers was named at compile time, set it
     * here.  We set a longer idle timeout than specified in the
     * default configuration, since Javascript code runs updates every
     * few seconds for /temp and /rssi (to update the temperature and
     * signal strength in the UI). Most browsers leave connections
     * established after a page is loaded, so keep them open to be
     * re-used for the updates.
     *
     * See: https://slimhazard.gitlab.io/picow_http/group__server.html#ga3380001925d9eb091370db86d85f7349
     */
    cfg = http_default_cfg();
    cfg.idle_tmo_s = 30;

    /*
     * Start the server, and turn on the onboard LED when it's
     * running.
     *
     * See: https://slimhazard.gitlab.io/picow_http/group__server.html#gae2b8bdf44100f13cd2c5e18969208ff5
     */
    while ((err = http_srv_init(&srv, &cfg)) != ERR_OK)
        HTTP_LOG_ERROR("http_init: %d\n", err);
    HTTP_LOG_INFO("http started");
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);

    while (true) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1));
    }
}
