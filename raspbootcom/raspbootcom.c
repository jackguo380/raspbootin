/* raspbootcom.cc - upload kernel.img via serial port to the RPi */
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

#define _BSD_SOURCE             /* See feature_test_macros(7) */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <stdint.h>
#include <termios.h>

#define BUF_SIZE 65536

struct termios old_tio, new_tio;

void do_exit(int fd, int res) {
    // close FD
    if (fd != -1) close(fd);
    // restore settings for STDIN_FILENO
    if (isatty(STDIN_FILENO)) {
	tcsetattr(STDIN_FILENO,TCSANOW,&old_tio);
    }
    exit(res);
}

// open serial connection
int open_serial(const char *dev) {
    // The termios structure, to be configured for serial interface.
    struct termios termios;

    // Open the device, read/write, not the controlling tty, and non-blocking I/O
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
	// failed to open
	return -1;
    }
    // must be a tty
    if (!isatty(fd)) {
        fprintf(stderr, "%s is not a tty\n", dev);
	do_exit(fd, EXIT_FAILURE);
    }

    // Get the attributes.
    if(tcgetattr(fd, &termios) == -1)
    {
        perror("Failed to get attributes of device");
	do_exit(fd, EXIT_FAILURE);
    }

    // So, we poll.
    termios.c_cc[VTIME] = 0;
    termios.c_cc[VMIN] = 0;

    // 8N1 mode, no input/output/line processing masks.
    termios.c_iflag = 0;
    termios.c_oflag = 0;
    termios.c_cflag = CS8 | CREAD | CLOCAL;
    termios.c_lflag = 0;

    // Set the baud rate.
    if((cfsetispeed(&termios, B115200) < 0) ||
       (cfsetospeed(&termios, B115200) < 0))
    {
        perror("Failed to set baud-rate");
	do_exit(fd, EXIT_FAILURE);
    }

    // Write the attributes.
    if (tcsetattr(fd, TCSAFLUSH, &termios) == -1) {
	perror("tcsetattr()");
	do_exit(fd, EXIT_FAILURE);
    }
    return fd;
}

// send kernel to rpi
void send_kernel(int fd, const char *file) {
    int file_fd;
    off_t off;
    uint32_t size;
    ssize_t pos;
    char *p;
    bool done = false;
    
    // Set fd blocking
    if (fcntl(fd, F_SETFL, 0) == -1) {
	perror("fcntl()");
	do_exit(fd, EXIT_FAILURE);
    }

    // Open file
    if ((file_fd = open(file, O_RDONLY)) == -1) {
	perror(file);
	do_exit(fd, EXIT_FAILURE);
    }

    // Get kernel size
    off = lseek(file_fd, 0L, SEEK_END);
    if (off > 0x200000) {
	fprintf(stderr, "kernel too big\n");
	do_exit(fd, EXIT_FAILURE);
    }
    size = htole32(off);
    lseek(file_fd, 0L, SEEK_SET);

    fprintf(stderr, "### sending kernel %s [%zu byte]\n", file, (size_t)off);

    // send kernel size to RPi
    p = (char*)&size;
    pos = 0;
    while(pos < 4) {
	ssize_t len = write(fd, &p[pos], 4 - pos);
	if (len == -1) {
	    perror("write()");
	    do_exit(fd, EXIT_FAILURE);
	}
	pos += len;
    }
    // wait for OK
    char ok_buf[2];
    p = ok_buf;
    pos = 0;
    while(pos < 2) {
	ssize_t len = read(fd, &p[pos], 2 - pos);
	if (len == -1) {
	    perror("read()");
	    do_exit(fd, EXIT_FAILURE);
	}
	pos += len;
    }
    if (ok_buf[0] != 'O' || ok_buf[1] != 'K') {
	fprintf(stderr, "error after sending size\n");
	do_exit(fd, EXIT_FAILURE);
    }

    while(!done) {
	char buf[BUF_SIZE];
	ssize_t pos = 0;
	ssize_t len = read(file_fd, buf, BUF_SIZE);
	switch(len) {
	case -1:
	    perror("read()");
	    do_exit(fd, EXIT_FAILURE);
	case 0:
	    done = true;
	}
	while(len > 0) {
	    ssize_t len2 = write(fd, &buf[pos], len);
	    if (len2 == -1) {
		perror("write()");
		do_exit(fd, EXIT_FAILURE);
	    }
	    len -= len2;
	    pos += len2;
	}
    }
    
    // Set fd non-blocking
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
	perror("fcntl()");
	do_exit(fd, EXIT_FAILURE);
    }

    fprintf(stderr, "### finished sending\n");

    return;
}

int main(int argc, char *argv[]) {
    int fd, max_fd = STDIN_FILENO;
    fd_set rfds, wfds, efds;
    char buf[BUF_SIZE];
    size_t start = 0;
    size_t end = 0;
    bool done = false, leave = false;

    printf("Raspbootcom V1.0\n");

    if (argc != 3) {
	printf("USAGE: %s <dev> <file>\n", argv[0]);
	printf("Example: %s /dev/ttyUSB0 kernel/kernel.img\n", argv[0]);
	exit(EXIT_FAILURE);
    }

    // Set STDIN non-blocking and unbuffered
    if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1) {
	perror("fcntl()");
	exit(EXIT_FAILURE);
    }
    if (isatty(STDIN_FILENO)) {
	// get the terminal settings for stdin
	if (tcgetattr(STDIN_FILENO, &old_tio) == -1) {
	    perror("tcgetattr");
	    exit(EXIT_FAILURE);
	}
	
	// we want to keep the old setting to restore them a the end
	new_tio=old_tio;

	// disable canonical mode (buffered i/o) and local echo
	new_tio.c_lflag &= (~ICANON & ~ECHO);

	// set the new settings immediately
	if (tcsetattr(STDIN_FILENO, TCSANOW, &new_tio) == -1) {
	    perror("tcsetattr()");
	    do_exit(-1, EXIT_FAILURE);
	}
    }
    
    while(!leave) {
	// Open device
	if ((fd = open_serial(argv[1])) == -1) {
	    // udev takes a while to change ownership
	    // so sometimes one gets EPERM
	    if (errno == ENOENT || errno == ENODEV || errno == EACCES) {
		fprintf(stderr, "\r### Waiting for %s...\r", argv[1]);
		sleep(1);
		continue;
	    }
	    perror(argv[1]);
	    do_exit(fd, EXIT_FAILURE);
	}
	fprintf(stderr, "### Listening on %s     \n", argv[1]);

	// select needs the largeds FD + 1
	if (fd > STDIN_FILENO) {
	    max_fd = fd + 1;
	} else {
	    max_fd = STDIN_FILENO + 1;
	}

	done = false;
	start = end = 0;
	while(!done || start != end) {	
	    // Watch stdin and dev for input.
	    FD_ZERO(&rfds);
	    if (!done && end < BUF_SIZE) FD_SET(STDIN_FILENO, &rfds);
	    FD_SET(fd, &rfds);
	    
	    // Watch fd for output if needed.
	    FD_ZERO(&wfds);
	    if (start != end) FD_SET(fd, &wfds);

	    // Watch stdin and dev for error.
	    FD_ZERO(&efds);
	    FD_SET(STDIN_FILENO, &efds);
	    FD_SET(fd, &efds);

	    // Wait for something to happend
	    if (select(max_fd, &rfds, &wfds, &efds, NULL) == -1) {
		perror("select()");
		do_exit(fd, EXIT_FAILURE);
	    } else {
		// check for errors
		if (FD_ISSET(STDIN_FILENO, &efds)) {
		    fprintf(stderr, "error on STDIN\n");
		    do_exit(fd, EXIT_FAILURE);
		}
		if (FD_ISSET(fd, &efds)) {
		    fprintf(stderr, "error on device\n");
		    do_exit(fd, EXIT_FAILURE);
		}
		// RPi is ready to recieve more data, send more
		if (FD_ISSET(fd, &wfds)) {
		    ssize_t len = write(fd, &buf[start], end - start);
		    if (len == -1) {
			perror("write()");
			do_exit(fd, EXIT_FAILURE);
		    }
		    start += len;
		    if (start == end) start = end = 0;
		    // shift buffer contents
		    if (end == BUF_SIZE) {
			memmove(buf, &buf[start], end - start);
			end -= start;
			start = 0;
		    }
		}
		// input from the user, copy to RPi
		if (FD_ISSET(STDIN_FILENO, &rfds)) {
		    ssize_t len = read(STDIN_FILENO, &buf[end], BUF_SIZE - end);
		    switch(len) {
		    case -1:
			perror("read()");
			do_exit(fd, EXIT_FAILURE);
		    case 0:
			done = true;
			leave = true;
		    }
		    // hacky pseudo echo
		    if (len > 0) {
			if (write(STDOUT_FILENO, &buf[end], len) == -1) {
			    perror("write()");
			    do_exit(fd, EXIT_FAILURE);
			}
		    }
		    end += len;
		}
		// output from the RPi, copy to STDOUT
		if (FD_ISSET(fd, &rfds)) {
		    char buf2[BUF_SIZE+1];
		    ssize_t len = read(fd, buf2, BUF_SIZE);
		    switch(len) {
		    case -1:
			perror("read()");
			do_exit(fd, EXIT_FAILURE);
		    case 0:
			done = true;
		    }
		    // scan output for tripple break (^C^C^C)
		    char *buf2_end = buf2+len;
		    *buf2_end = 0; // ensure theres a null byte
		    const char *break_c = index(buf2, '\x03');

		    // Found 3 breaks in a row, send kernel
		    if (break_c != NULL && (buf2_end - break_c) >= 3
			    && break_c[1] == '\x03' && break_c[2] == '\x03') {
			ssize_t len2 = write(STDOUT_FILENO, buf2, break_c - buf2);
			if (len2 == -1) {
			    perror("write()");
			    do_exit(fd, EXIT_FAILURE);
			}

			send_kernel(fd, argv[2]);
		    } else {
			// Regular output, just pass it through
			ssize_t len2 = write(STDOUT_FILENO, buf2, len);
			if (len2 == -1) {
			    perror("write()");
			    do_exit(fd, EXIT_FAILURE);
			}
		    }
		}
	    }
	}
	close(fd);
    }
		
    do_exit(-1, EXIT_SUCCESS);
}
