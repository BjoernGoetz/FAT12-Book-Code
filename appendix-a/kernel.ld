/* _start, defined in startup.s, is the entry point */
ENTRY(_start)

/* Lays out where each section goes in the final kernel binary */
SECTIONS
{
    /* Load above the BIOS and VGA memory in the first 1 MiB */
    . = 1M;

    .text : {
        /* Keeps the .multiboot section from startup.s at the start of
           .text, so the loader finds the header */
        KEEP(*(.multiboot))
        *(.text*)
    } :text
    .rodata : { *(.rodata*) } :rodata
    .data   : { *(.data*) } :data
    .bss    : { *(COMMON) *(.bss*) } :data
}

/* Optional: splits the kernel into loadable segments, one per
   permission set, instead of one segment that allows everything */
PHDRS
{
    text   PT_LOAD FLAGS(5); /* R+X */
    rodata PT_LOAD FLAGS(4); /* R   */
    data   PT_LOAD FLAGS(6); /* R+W */
}
