#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "errno.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include "esp_camera.h"

// M5STACK Unitcam
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    15
#define XCLK_GPIO_NUM     27
#define SIOD_GPIO_NUM     25
#define SIOC_GPIO_NUM     23
#define Y9_GPIO_NUM       19
#define Y8_GPIO_NUM       36
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       39
#define Y5_GPIO_NUM        5
#define Y4_GPIO_NUM       34
#define Y3_GPIO_NUM       35
#define Y2_GPIO_NUM       32
#define VSYNC_GPIO_NUM    22
#define HREF_GPIO_NUM     26
#define PCLK_GPIO_NUM     21

static const char *TAG = "camera";

esp_err_t init_camera(void)
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 24000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_DRAM;

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed error 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();

    // drop down frame size for higher initial frame rate
    s->set_framesize(s, FRAMESIZE_CIF);

    // CLK X2
    // s->set_reg(s, 0x111, 0x80, 0x80);

    return ESP_OK;
}

static int camera_sock;
static struct sockaddr_in senderinfo;

static void camera_tx(void *param)
{
    size_t udp_packet_max = 65536;
    senderinfo.sin_port = htons(55556);

    uint32_t frame_count = 0;
    uint32_t frame_size_sum = 0;
    int64_t start = esp_timer_get_time();

    while (1)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            esp_camera_fb_return(fb);
            continue;
        }

        uint8_t *buf = fb->buf;
        size_t total = fb->len;
        size_t send = 0;

        // ESP_LOGI(TAG, "send %d", total);

        for (; send < total;)
        {
            size_t txlen = total - send;
            if (txlen > udp_packet_max) {
                txlen = udp_packet_max;
            }
            int err = sendto(camera_sock, &buf[send], txlen, 0, (struct sockaddr *)&senderinfo, sizeof(senderinfo));
            if (err < 0)
            {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                if (errno == ENOMEM)
                {
                    vTaskDelay(pdMS_TO_TICKS(2));
                }
                else
                {
                    send = total;
                }
            }
            else
            {
                send += err;
            }
        }

        esp_camera_fb_return(fb);

        frame_count++;
        frame_size_sum += total;

        int64_t end = esp_timer_get_time();
        int64_t pasttime = end - start;
        if (pasttime > 1000000) {
            start = end;
            float adj = 1000000.0 / (float)pasttime;
            ESP_LOGI(TAG, "%.1f fps %.3f Mbps", frame_count * adj, (frame_size_sum * 8.0 * adj) / 1024.0 / 1024.0);
            frame_count = 0;
            frame_size_sum = 0;
        }
    }
}

void start_camera(void)
{
    camera_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (camera_sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(55555);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(camera_sock, (struct sockaddr *)&addr, sizeof(addr));

    socklen_t addrlen = sizeof(senderinfo);
    char buf[128];
    ESP_LOGI(TAG, "Wait a trigger...");
    while (1) {
        int n = recvfrom(camera_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&senderinfo, &addrlen);
        if (n > 0 && buf[0] == 0x55) {
            break;
        }
        ESP_LOGI(TAG, "recvfrom %d", n);
    }

    xTaskCreatePinnedToCore(&camera_tx, "camera_tx", 4096, NULL, 1, NULL, tskNO_AFFINITY);
}
