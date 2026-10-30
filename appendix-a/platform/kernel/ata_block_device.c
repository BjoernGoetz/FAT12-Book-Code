#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "block_device.h"
#include "ata_block_device.h"

#define ATA_DATA         0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW      0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HIGH     0x1F5
#define ATA_DRIVE        0x1F6
#define ATA_STATUS       0x1F7
#define ATA_COMMAND      0x1F7

#define ATA_STATUS_BUSY         0x80
#define ATA_STATUS_ERROR        0x01
#define ATA_STATUS_DATA_REQUEST 0x08
#define ATA_STATUS_DRIVE_FAULT  0x20

#define ATA_COMMAND_IDENTIFY    0xEC
#define ATA_COMMAND_READ        0x20
#define ATA_COMMAND_WRITE       0x30
#define ATA_COMMAND_CACHE_FLUSH 0xE7

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;

    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));

    return ret;
}

static inline void insw(uint16_t port, void *addr, uint32_t byte_count)
{
    uint16_t *buf = (uint16_t *)addr;
    uint32_t word_count = byte_count / 2;

    __asm__ volatile("cld\n\t"
                     "rep insw"
                     : "+D"(buf), "+c"(word_count)
                     : "d"(port)
                     : "memory");
}

static inline void outsw(uint16_t port, const void *addr,
                         uint32_t byte_count)
{
    const uint16_t *buf = (const uint16_t *)addr;
    uint32_t word_count = byte_count / 2;

    __asm__ volatile("cld\n\t"
                     "rep outsw"
                     : "+S"(buf), "+c"(word_count)
                     : "d"(port)
                     : "memory");
}

/* four dummy status reads take roughly 400ns, the time a drive needs
   before its status register is trustworthy */
static inline void ata_delay_400ns(void)
{
    for (int i = 0; i < 4; i++)
        inb(ATA_STATUS);
}

static inline void ata_issue_command(uint8_t command, uint32_t lba,
                                     uint8_t sector_count)
{
    /* wait until the selected drive is done,
       leaving the channel idle */
    while (inb(ATA_STATUS) & ATA_STATUS_BUSY)
        ;

    /* master drive, LBA mode, plus the LBA's top 4 bits (27:24) */
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    ata_delay_400ns();

    outb(ATA_SECTOR_COUNT, sector_count);
    outb(ATA_LBA_LOW, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));

    /* triggers execution, so it must be last */
    outb(ATA_COMMAND, command);
    ata_delay_400ns();

    /* wait until the drive has accepted this command */
    while (inb(ATA_STATUS) & ATA_STATUS_BUSY)
        ;
}

static inline int ata_wait_data_request(void)
{
    while (1) {
        uint8_t status = inb(ATA_STATUS);

        /* while BSY is set, every other status bit is meaningless */
        if (status & ATA_STATUS_BUSY)
            continue;

        /* a failed command never raises DRQ, so stop waiting for it */
        if (status & (ATA_STATUS_ERROR | ATA_STATUS_DRIVE_FAULT))
            return -1;

        if (status & ATA_STATUS_DATA_REQUEST)
            return 0;
    }
}

struct BlockDevice {
    uint32_t sector_size;
    uint64_t sector_count;
};

BlockDevice *ata_block_device_open(void)
{
    ata_issue_command(ATA_COMMAND_IDENTIFY, 0, 0);

    if (ata_wait_data_request() != 0)
        return NULL;

    /* IDENTIFY answers with one 512-byte block: 256 16-bit words */
    uint16_t identify[256];
    insw(ATA_DATA, identify, sizeof(identify));

    uint32_t sector_count = (uint32_t)identify[60] |
                            ((uint32_t)identify[61] << 16);

    BlockDevice *device = (BlockDevice *)malloc(sizeof(BlockDevice));
    device->sector_count = sector_count;

    /* 512 is the ATA default logical sector size; IDENTIFY only reports
       an actual size when a drive's is larger. Classic ATA/IDE drives,
       and QEMU's IDE emulation, never are, so we hardcode it */
    device->sector_size = 512;

    return device;
}

int block_device_read(BlockDevice *device, uint64_t lba,
                      uint32_t sector_count, void *buffer)
{
    ata_issue_command(ATA_COMMAND_READ, (uint32_t)lba,
                      (uint8_t)sector_count);

    for (uint32_t i = 0; i < sector_count; i++) {
        if (ata_wait_data_request() != 0)
            return -1;

        insw(ATA_DATA, (uint8_t *)buffer + (i * device->sector_size),
             device->sector_size);

        /* give the drive time to raise BSY before we check it again */
        ata_delay_400ns();
    }

    return 0;
}

int block_device_write(BlockDevice *device, uint64_t lba,
                       uint32_t sector_count, const void *buffer)
{
    ata_issue_command(ATA_COMMAND_WRITE, (uint32_t)lba,
                      (uint8_t)sector_count);

    for (uint32_t i = 0; i < sector_count; i++) {
        if (ata_wait_data_request() != 0)
            return -1;

        outsw(ATA_DATA,
              (const uint8_t *)buffer + (i * device->sector_size),
              device->sector_size);

        /* give the drive time to raise BSY before we check it again */
        ata_delay_400ns();
    }

    /* the drive may only have buffered these sectors in its
       write cache; CACHE_FLUSH forces them out to persistent
       media before we return */
    ata_issue_command(ATA_COMMAND_CACHE_FLUSH, 0, 0);

    return 0;
}

uint32_t block_device_sector_size(BlockDevice *device)
{
    return device->sector_size;
}

uint64_t block_device_sector_count(BlockDevice *device)
{
    return device->sector_count;
}

int block_device_close(BlockDevice *device)
{
    free(device);
    return 0;
}
