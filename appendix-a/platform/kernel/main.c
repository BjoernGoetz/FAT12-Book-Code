#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "vga.h"
#include "ata_block_device.h"
#include "block_device.h"
#include "fat12.h"

void kernel_main(void)
{
    vga_clear();
    vga_print("FAT12 kernel booted\n\n");
    vga_print("Opening block device ..... ");
    BlockDevice *device = ata_block_device_open();
    if (device == NULL)
        return;
    vga_print("OK\n");

    vga_print("Formatting volume ........ ");
    FormatParams params = {"MYKERNEL"};
    if (fat12_format(device, params) != 0)
        return;
    vga_print("OK\n");

    vga_print("Mounting volume .......... ");
    FAT12FS *fs = fat12_mount(device);
    if (fs == NULL)
        return;
    vga_print("OK\n");

    vga_print("Writing /HELLO.TXT ....... ");
    File *f = fat12_open(fs, "/HELLO.TXT", 'w');
    if (f == NULL)
        return;
    const char *text = "Hello from the kernel!\n";
    uint32_t written = fat12_write(f, text, strlen(text));
    if (fat12_close(f) != 0 || written != strlen(text))
        return;
    vga_print("OK\n");

    vga_print("Listing / ................ ");
    Directory *dir = fat12_opendir(fs, "/");
    if (dir == NULL)
        return;
    vga_print("OK\n\n");
    DirEntry entry;
    while (fat12_readdir(dir, &entry) != -1) {
        vga_print("    ");
        vga_print(entry.name);
        vga_print("\n");
    }
    vga_print("\n");
    fat12_closedir(dir);

    vga_print("Reading /HELLO.TXT ....... ");
    f = fat12_open(fs, "/HELLO.TXT", 'r');
    if (f == NULL)
        return;
    vga_print("OK\n\n    ");
    char buf[256];
    uint32_t bytes;
    while ((bytes = fat12_read(f, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes] = '\0';
        vga_print(buf);
    }
    vga_print("\n");
    fat12_close(f);

    vga_print("Unmounting volume ........ ");
    fat12_umount(fs);
    vga_print("OK\n");

    vga_print("Closing block device ..... ");
    if (block_device_close(device) != 0)
        return;
    vga_print("OK\n");
}
