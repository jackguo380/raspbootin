/* main.cc - the entry point for the kernel */
/* Copyright (C) 2013 Goswin von Brederlow <goswin-v-b@web.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdint.h>
#include <archinfo.h>
#include <uart.h>
#include <printnumk.h>
#include <atag.h>

extern "C" {
    // kernel_main gets called from boot.S. Declaring it extern "C" avoid
    // having to deal with the C++ name mangling.
    void kernel_main(uint32_t r0, uint32_t r1, const Header *atags);
}


#define LOADER_ADDR 0x1000000


typedef void (*entry_fn)(uint32_t r0, uint32_t r1, const Header *atags);

const ArchInfo *arch_info;

const char *find(const char *str, const char *token) {
    while(*str) {
	const char *p = str;
	const char *q = token;
	// as long as str matches token
	while(*p && *p == *q) {
	    // keep comparing
	    ++p;
	    ++q;
	}
	if (*q == 0) return str; // found token
	// token not found, try again
	++str;
    }

    // end of string, nothing found
    return NULL;
}

// kernel main function, it all begins here
void kernel_main(uint32_t r0, uint32_t r1, const Header *atags) {
    // Only Raspberry Pi 2 is supported
    const char model_name[] = "Raspberry Pi b 2\0";
    const char hello[] = "\n\nRaspbootin (Modded for CPEN-432) V1.1\n\n\0";
    const char done_str[] = "booting your kernel...\n\n\0";
    const char size_err[] = "The kernel is too big: \0";

    ArchInfo info(model_name, 0x3F000000, 47, 0);
    arch_info = &info;

    UART::init();
    UART::puts(hello);

    // request kernel by sending 3 breaks
    UART::puts("\x03\x03\x03");

    // get kernel size
    uint32_t size = UART::getc();
    size |= UART::getc() << 8;
    size |= UART::getc() << 16;
    size |= UART::getc() << 24;

    if (0x8000 + size > LOADER_ADDR) {
	UART::putc('S');
	UART::putc('E');

	UART::puts(size_err);
	printnumk(10, size);
	UART::putc('\n');

	while(1) {
	    for(volatile int i = 0; i < 1000000; ++i) { }
	}
    } else {
	UART::putc('O');
	UART::putc('K');
    }
    
    // get kernel
    uint8_t *kernel = (uint8_t*)0x8000;
    while(size-- > 0) {
	*kernel++ = UART::getc();
    }

    // Print done
    UART::puts(done_str);

    // Disable uart to prevent stray characters
    UART::deinit();

    entry_fn fn = (entry_fn)0x8000;
    fn(r0, r1, atags);
}

