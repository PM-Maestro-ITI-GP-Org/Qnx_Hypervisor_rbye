#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <devctl.h>
#include <errno.h>
#include <hw/io-spi.h>

#define SPI_DEV_PATH "/dev/io-spi/spi0/dev0"
#define DATA_LEN 16

int main(int argc, char *argv[])
{
    int fd, ret, match;
    spi_cfg_t cfg;
    spi_drvinfo_t drvinfo;
    spi_devinfo_t devinfo;
    uint32_t xchng_size;
    spi_xchng_t *xchng;

    fd = open(SPI_DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    ret = devctl(fd, DCMD_SPI_GET_DRVINFO, &drvinfo, sizeof(drvinfo), NULL);
    if (ret != EOK) {
        fprintf(stderr, "devctl GET_DRVINFO: %s\n", strerror(ret));
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Driver: %s v%u.%u\n", drvinfo.name,
           drvinfo.version >> 16, (drvinfo.version >> 8) & 0xFF);

    ret = devctl(fd, DCMD_SPI_GET_DEVINFO, &devinfo, sizeof(devinfo), NULL);
    if (ret != EOK) {
        fprintf(stderr, "devctl GET_DEVINFO: %s\n", strerror(ret));
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Device: %s, devno=%d, clock=%u Hz\n",
           devinfo.name, devinfo.devno, devinfo.current_clkrate);

    cfg.mode = SPI_MODE_WORD_WIDTH_8 | SPI_MODE_CPHA_0
             | SPI_MODE_CPOL_0 | SPI_MODE_BODER_MSB;
    cfg.clock_rate = 5000000;
    ret = devctl(fd, DCMD_SPI_SET_CONFIG, &cfg, sizeof(cfg), NULL);
    if (ret != EOK) {
        fprintf(stderr, "devctl SET_CONFIG: %s\n", strerror(ret));
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Config: 5MHz, mode 0, 8-bit, MSB\n");

    xchng_size = sizeof(spi_xchng_t) + DATA_LEN;
    xchng = malloc(xchng_size);
    if (!xchng) {
        perror("malloc");
        close(fd);
        return EXIT_FAILURE;
    }

    xchng->nbytes = DATA_LEN;
    for (int i = 0; i < DATA_LEN; i++)
        xchng->data[i] = 0xA0 + i;

    printf("\nTX: ");
    for (int i = 0; i < DATA_LEN; i++)
        printf("0x%02X ", xchng->data[i]);
    printf("\n");

    ret = devctl(fd, DCMD_SPI_DATA_XCHNG, xchng, xchng_size, NULL);
    if (ret != EOK) {
        fprintf(stderr, "devctl DATA_XCHNG: %s\n", strerror(ret));
        free(xchng);
        close(fd);
        return EXIT_FAILURE;
    }

    printf("RX: ");
    for (int i = 0; i < DATA_LEN; i++)
        printf("0x%02X ", xchng->data[i]);
    printf("\n");

    match = 1;
    for (int i = 0; i < DATA_LEN; i++) {
        if (xchng->data[i] != (uint8_t)(0xA0 + i)) {
            match = 0;
            break;
        }
    }

    if (match)
        printf("\nLOOPBACK TEST PASSED\n");
    else
        printf("\nLOOPBACK TEST FAILED (check MISO-MOSI jumper)\n");

    free(xchng);
    close(fd);
    return match ? EXIT_SUCCESS : EXIT_FAILURE;
}
