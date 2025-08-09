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

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <libdrm/drm_fourcc.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

#include "mdgui.h"

#define	WIDTH	1920
#define	HEIGHT	1080

static struct {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
} egl;

static struct {
	struct gbm_device *dev;
	struct gbm_surface *surface;
} gbm;

static struct {
	int fd;
	drmModeModeInfo *mode;
	uint32_t crtc_id;
	uint32_t connector_id;
} drm;

struct drm_fb {
	struct gbm_bo *bo;
	uint32_t fb_id;
};

static uint32_t
find_crtc_for_encoder(const drmModeRes *resources,
    const drmModeEncoder *encoder)
{
	uint32_t crtc_mask;
	uint32_t crtc_id;
	int i;

	for (i = 0; i < resources->count_crtcs; i++) {
		crtc_mask = 1 << i;
		crtc_id = resources->crtcs[i];
		if (encoder->possible_crtcs & crtc_mask)
			return (crtc_id);
	}

	return (-1);
}

static uint32_t
find_crtc_for_connector(const drmModeRes *resources,
    const drmModeConnector *connector)
{
	drmModeEncoder *encoder;
	uint32_t encoder_id;
	uint32_t crtc_id;
	int i;

	for (i = 0; i < connector->count_encoders; i++) {
		encoder_id = connector->encoders[i];
		encoder = drmModeGetEncoder(drm.fd, encoder_id);
		if (encoder) {
			crtc_id = find_crtc_for_encoder(resources, encoder);
			drmModeFreeEncoder(encoder);
			if (crtc_id != 0)
				return (crtc_id);
		}
	}

	return (-1);
}

static int
init_drm(void)
{
	drmModeRes *resources;
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	uint32_t crtc_id;
	int current_area;
	int area;
	int i;

	drm.fd = open("/dev/dri/card0", O_RDWR);
	if (drm.fd < 0) {
		printf("could not open drm device\n");
		return (-1);
	}

	resources = drmModeGetResources(drm.fd);
	if (!resources) {
		printf("drmModeGetResources failed: %s\n", strerror(errno));
		return (-1);
	}

	connector = NULL;
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(drm.fd,
		    resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED)
			break;
		drmModeFreeConnector(connector);
		connector = NULL;
	}

	printf(" Connectors count %d\n", resources->count_connectors);

	if (!connector)
		return (-1);

	printf(" Connector mode count %d\n", connector->count_modes);

	/* find prefered mode or the highest resolution mode: */
	for (i = 0, area = 0; i < connector->count_modes; i++) {
		drmModeModeInfo *current_mode = &connector->modes[i];

		if (current_mode->type & DRM_MODE_TYPE_PREFERRED)
			drm.mode = current_mode;

		current_area = current_mode->hdisplay * current_mode->vdisplay;
		if (current_area > area) {
			drm.mode = current_mode;
			area = current_area;
		}
	}

	if (!drm.mode)
		return (-1);

	printf(" Encoder count %d\n", resources->count_encoders);

	encoder = NULL;
	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(drm.fd, resources->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id)
			break;
		drmModeFreeEncoder(encoder);
		encoder = NULL;
	}

	if (encoder)
		drm.crtc_id = encoder->crtc_id;
	else {
		crtc_id = find_crtc_for_connector(resources, connector);
		if (crtc_id == -1)
			return (-1);

		drm.crtc_id = crtc_id;
	}

	drm.connector_id = connector->connector_id;

	return (0);
}

static int
init_gbm(void)
{
	uint32_t format;

	format = DRM_FORMAT_ARGB8888;

	gbm.dev = gbm_create_device(drm.fd);

	printf("drm.mode->hdisplay %d, vdisplay %d\n", drm.mode->hdisplay,
	    drm.mode->vdisplay);

	gbm.surface = gbm_surface_create(gbm.dev, drm.mode->hdisplay,
	    drm.mode->vdisplay, format, GBM_BO_USE_SCANOUT |
	    GBM_BO_USE_RENDERING);
	if (!gbm.surface)
		return (-1);

	return (0);
}

static int
match_config_to_visual(EGLDisplay egl_display, EGLint visual_id,
    EGLConfig *configs, int count)
{
	EGLint id;
	int i;

	for (i = 0; i < count; ++i) {
		if (!eglGetConfigAttrib(egl_display, configs[i],
		    EGL_NATIVE_VISUAL_ID, &id))
			continue;
		if (id == visual_id)
			return (i);
	}

	return (-1);
}

static int
init_gl(void)
{
	EGLint major, minor, n;
	EGLConfig *configs;
	int config_index;
	int visual_id;
	int count;

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 0,
		EGL_DEPTH_SIZE, 24,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SAMPLES, 0,
		EGL_NONE
	};

	PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;
	get_platform_display =
		(void *) eglGetProcAddress("eglGetPlatformDisplayEXT");
	assert(get_platform_display != NULL);

	egl.display = eglGetDisplay((void *)gbm.dev);
	if (!eglInitialize(egl.display, &major, &minor)) {
		printf("failed to initialize\n");
		return (-1);
	}

	printf("EGL Version \"%s\"\n",
	    eglQueryString(egl.display, EGL_VERSION));
	printf("EGL Vendor \"%s\"\n",
	    eglQueryString(egl.display, EGL_VENDOR));
	printf("EGL Extensions \"%s\"\n",
	    eglQueryString(egl.display, EGL_EXTENSIONS));

	if (!eglBindAPI(EGL_OPENGL_API))
		return (-1);

	if (!eglGetConfigs(egl.display, NULL, 0, &count) || count < 1) {
		printf("No EGL configs to choose from.\n");
		return (-1);
	}

	configs = malloc(count * sizeof(*configs));
	if (!configs)
		return (-1);

	n = 0;
	if (!eglChooseConfig(egl.display, config_attribs, configs, count, &n)) {
		printf("failed to choose config: count %d, n %d\n", count, n);
		return (-1);
	}

	visual_id = DRM_FORMAT_ARGB8888;
	config_index = match_config_to_visual(egl.display, visual_id, configs,
	    n);
	if (config_index < 0)
		return (-1);

	egl.config = configs[config_index];
	egl.context = eglCreateContext(egl.display, egl.config, EGL_NO_CONTEXT,
	    context_attribs);
	if (egl.context == NULL) {
		printf("failed to create context\n");
		return (-1);
	}

	egl.surface = eglCreateWindowSurface(egl.display, egl.config,
	    gbm.surface, NULL);
	if (egl.surface == EGL_NO_SURFACE) {
		printf("failed to create egl surface\n");
		return (-1);
	}

	eglMakeCurrent(egl.display, egl.surface, egl.surface, egl.context);

	return (0);
}

static int count = 0;

static void
draw(uint32_t i)
{

	mdgui_render(i);

	/* Skip two frames before listening for input events. */
	if (count < 2) {
		count++;
		return;
	}

	input_poll_once();
}

static void
drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
	struct drm_fb *fb;

	fb = data;
	if (fb->fb_id)
		drmModeRmFB(drm.fd, fb->fb_id);

	free(fb);
}

static struct drm_fb *
drm_fb_get_from_bo(struct gbm_bo *bo)
{
	uint32_t width, height, stride, handle;
	struct drm_fb *fb;
	int ret;

	fb = gbm_bo_get_user_data(bo);
	if (fb)
		return (fb);

	fb = calloc(1, sizeof *fb);
	fb->bo = bo;

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	stride = gbm_bo_get_stride(bo);
	handle = gbm_bo_get_handle(bo).u32;

	ret = drmModeAddFB(drm.fd, width, height, 32, 32, stride, handle,
	    &fb->fb_id);
	if (ret) {
		printf("failed to create fb: %s\n", strerror(errno));
		free(fb);
		return (NULL);
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return (fb);
}

static void
page_flip_handler(int fd, unsigned int frame, unsigned int sec,
    unsigned int usec, void *data)
{
	int *waiting_for_flip;

	waiting_for_flip = (int *)data;

	*waiting_for_flip = 0;
}

int
main(int argc, char *argv[])
{
	drmEventContext evctx;
	struct gbm_bo *next_bo;
	int waiting_for_flip;
	struct gbm_bo *bo;
	struct drm_fb *fb;
	fd_set fds;
	int ret;
	int i;

	bzero(&evctx, sizeof(drmEventContext));
	evctx.version = DRM_EVENT_CONTEXT_VERSION;
	evctx.page_flip_handler = page_flip_handler;

	ret = init_drm();
	if (ret) {
		printf("failed to initialize DRM\n");
		return ret;
	}

	FD_ZERO(&fds);
	FD_SET(0, &fds);
	FD_SET(drm.fd, &fds);

	ret = init_gbm();
	if (ret) {
		printf("failed to initialize GBM\n");
		return ret;
	}

	ret = init_gl();
	if (ret) {
		printf("failed to initialize EGL\n");
		return ret;
	}

	mdgui_init();
	input_init();

	eglSwapBuffers(egl.display, egl.surface);

	/* clear the color buffer */
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	bo = gbm_surface_lock_front_buffer(gbm.surface);
	fb = drm_fb_get_from_bo(bo);

	ret = drmModeSetCrtc(drm.fd, drm.crtc_id, fb->fb_id, 0, 0,
	    &drm.connector_id, 1, drm.mode);
	if (ret) {
		printf("Failed to set mode: %s\n", strerror(errno));
		return ret;
	}

	i = 0;

	while (1) {
		waiting_for_flip = 1;

		if (i > 0xfff)
			i = 0;

		draw(i++);

		eglSwapBuffers(egl.display, egl.surface);
		next_bo = gbm_surface_lock_front_buffer(gbm.surface);
		fb = drm_fb_get_from_bo(next_bo);

		ret = drmModePageFlip(drm.fd, drm.crtc_id, fb->fb_id,
			DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
		if (ret) {
			printf("failed to queue page flip: %s\n",
			    strerror(errno));
			return (-1);
		}

		while (waiting_for_flip) {
			ret = select(drm.fd + 1, &fds, NULL, NULL, NULL);
			if (ret < 0) {
				printf("select err: %s\n", strerror(errno));
				return (ret);
			} else if (ret == 0) {
				printf("select timeout!\n");
				return (-1);
			} else if (FD_ISSET(0, &fds)) {
				printf("user interrupted!\n");
				return (0);
			}
			drmHandleEvent(drm.fd, &evctx);
		}

		gbm_surface_release_buffer(gbm.surface, bo);
		bo = next_bo;
	}

	return (ret);
}
