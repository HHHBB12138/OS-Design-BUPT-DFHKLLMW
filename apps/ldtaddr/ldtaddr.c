#include "apilib.h"
#include "stdlib.h"
#include <stdio.h>
#include <string.h>

static void print_addr(const char *name, void *ptr, unsigned int ds_base)
{
    unsigned int offset = (unsigned int) ptr;
    unsigned int phys = ds_base + offset;
    unsigned int phys_kernel = api_virt2phys(ptr);
    int ok = (phys == phys_kernel);

    printf("%s\n", name);
    printf("  logical  DS:%08X\n", offset);
    printf("  ds_base  %08X\n", ds_base);
    printf("  physical %08X\n", phys);
    printf("  kernel   %08X\n", phys_kernel);
    printf("  check    %s\n\n", ok ? "physical == base + offset" : "mismatch");
}

static void wait_enter(void)
{
    api_putstr0("press Enter to continue\n");
    for (;;) {
        if (api_getkey(1) == 0x0a) {
            break;
        }
    }
}

int _main(void)
{
    unsigned int ds_base;
    int local_var = 42;
    char local_buf[8];
    memset(local_buf, 0, sizeof(local_buf));

    ds_base = api_getdsbase();
    api_putstr0("LDT address translation demo\n");
    api_putstr0("physical = ds_base + offset (no paging)\n\n");
    wait_enter();

    print_addr("stack int", &local_var, ds_base);
    wait_enter();
    print_addr("stack buf", local_buf, ds_base);
    wait_enter();
    
    api_end();
    return 0;
}
