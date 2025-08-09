/*-
 * Copyright (c) 2025 Ruslan Bukin <br@bsdpad.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <libinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdbool.h>

#include "mdgui.h"

static double x = 0, y = 0;
static struct libinput *libinput;
static struct udev *udev;

#define	dprintf(fmt, ...)

static void
process_event(struct libinput_event *event)
{
	struct libinput_event_pointer *pointer_event;
	struct libinput_event_touch *touch_event;
	enum libinput_button_state state;
	int type;

	type = libinput_event_get_type(event);
	switch(type) {
	case LIBINPUT_EVENT_POINTER_MOTION:
		pointer_event = libinput_event_get_pointer_event(event);
		x += libinput_event_pointer_get_dx(pointer_event);
		y += libinput_event_pointer_get_dy(pointer_event);
		dprintf("pointer motion event: %d %d\n", (int)x, (int)y);
		mdgui_input_event(2, x, y);
		break;
	case LIBINPUT_EVENT_POINTER_BUTTON:
		pointer_event = libinput_event_get_pointer_event(event);
		dprintf("pointer button event\n");
		state = libinput_event_pointer_get_button_state(pointer_event);
		if (state == LIBINPUT_BUTTON_STATE_PRESSED)
			mdgui_input_event(1, x, y);
		else
			mdgui_input_event(3, 0, 0);
		break;
	case LIBINPUT_EVENT_TOUCH_DOWN:
		touch_event = libinput_event_get_touch_event(event);
		x = libinput_event_touch_get_x(touch_event);
		y = libinput_event_touch_get_y(touch_event);
		dprintf("press down %f %f\n", x, y);
		mdgui_input_event(1, x, y);
		break;
	case LIBINPUT_EVENT_TOUCH_UP:
		dprintf("press up\n");
		mdgui_input_event(3, 0, 0);
		break;
	case LIBINPUT_EVENT_TOUCH_MOTION:
		touch_event = libinput_event_get_touch_event(event);
		x = libinput_event_touch_get_x(touch_event);
		y = libinput_event_touch_get_y(touch_event);
		dprintf("motion %f %f\n", x, y);
		mdgui_input_event(2, x, y);
		break;
	case LIBINPUT_EVENT_TOUCH_CANCEL:
	case LIBINPUT_EVENT_TOUCH_FRAME:
	default:
		break;
	}

	libinput_event_destroy(event);
}

static int
open_restricted(const char *path, int flags, void *user_data)
{

	return open(path, flags);
}

static void
close_restricted(int fd, void *user_data)
{

	close(fd);
}

static struct libinput_interface interface = {
	&open_restricted,
	&close_restricted,
};

int
input_poll_once(void)
{
	struct libinput_event *event;
	struct pollfd fd;

	fd.fd = libinput_get_fd(libinput);
	fd.events = POLLIN;
	fd.revents = 0;
	poll(&fd, 1, -1);
	libinput_dispatch(libinput);

	while (1) {
		event = libinput_get_event(libinput);
		if (!event)
			break;
		process_event(event);
	}

	return (0);
}

int
input_init(void)
{

	udev = udev_new();
	if (!udev)
		return (-1);

	libinput = libinput_udev_create_context(&interface, NULL, udev);
	if (!libinput)
		return (-2);

	if (libinput_udev_assign_seat(libinput, "seat0") == -1)
		return (-3);

	return (0);
}

void
input_deinit(void)
{

	libinput_unref(libinput);
	udev_unref(udev);
}
