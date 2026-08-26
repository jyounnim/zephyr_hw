#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <string.h>

#define LOOPBACK_NODE DT_NODELABEL(loopback_dev)

static const struct spi_dt_spec loopback_spi = SPI_DT_SPEC_GET(
    LOOPBACK_NODE, SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER, 0);

static bool spi_loopback_test(void) {
    uint8_t tx_data[8] = {0x01, 0x02, 0x03, 0x04, 0xAA, 0x55, 0xFF, 0x00};
    uint8_t rx_data[8] = {0};

    struct spi_buf tx_buf = { .buf = tx_data, .len = sizeof(tx_data) };
    struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };

    struct spi_buf rx_buf = { .buf = rx_data, .len = sizeof(rx_data) };
    struct spi_buf_set rx_bufs = { .buffers = &rx_buf, .count = 1 };

    int ret = spi_transceive_dt(&loopback_spi, &tx_bufs, &rx_bufs);
    if (ret != 0) {
        printk("spi_transceive_dt failed: %d\n", ret);
        return false;
    }

    printk("Sent:     ");
    for (int i = 0; i < (int)sizeof(tx_data); i++) printk("%02X ", tx_data[i]);
    printk("\n");

    printk("Received: ");
    for (int i = 0; i < (int)sizeof(rx_data); i++) printk("%02X ", rx_data[i]);
    printk("\n");

    return memcmp(tx_data, rx_data, sizeof(tx_data)) == 0;
}

#define TEST_STACK_SIZE 2048
#define TEST_PRIORITY   5

static void test_thread_entry(void *p1, void *p2, void *p3) {
    printk("\n=== SPI Basics: bus loopback self-test ===\n");

    if (!spi_is_ready_dt(&loopback_spi)) {
        printk("SPI device not ready - check devicetree status/overlay\n");
        return;
    }

    bool ok = spi_loopback_test();

    if (ok) {
        printk("PASS: received bytes match sent bytes - SPI peripheral, "
               "clock, and pin routing are all working.\n");
    } else {
        printk("FAIL: received bytes do NOT match sent bytes - check "
               "the MISO/MOSI pinmux, or SCLK wiring.\n");
    }
}

K_THREAD_DEFINE(test_tid, TEST_STACK_SIZE, test_thread_entry,
                NULL, NULL, NULL, TEST_PRIORITY, 0, 0);

int main(void) {
    return 0;
}
