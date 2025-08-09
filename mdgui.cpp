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

#include <stdio.h>

#include <GL/gl.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include "mdgui.h"

static int counter = 0;
static bool show_demo_window = true;
static bool show_another_window = false;
static float slider_f = 0.0f;

int
mdgui_init(void)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;

	io.ConfigFlags = ImGuiConfigFlags_IsTouchScreen;
	io.DisplaySize.x = DISPLAY_WIDTH;
	io.DisplaySize.y = DISPLAY_HEIGHT;
	io.ConfigInputTrickleEventQueue = false;

	/* Cursor. Disable for touch screen. */
	io.MouseDrawCursor = false;
	io.MouseDrawCursor = true;

	/* Dont save settings. */
	io.IniFilename = NULL;
	io.LogFilename = NULL;

	/* Pick one. */
	ImGui::StyleColorsLight();
	ImGui::StyleColorsDark();

	/* Setup scaling. */
	float main_scale = 4;
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);
	style.FontScaleDpi = main_scale;
	ImGui_ImplOpenGL3_Init(NULL);

	return (0);
}

int
mdgui_render(int i)
{

	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	float ImGuiWidth = DISPLAY_WIDTH;
	float ImGuiHeight = DISPLAY_HEIGHT;

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	ImGui::Begin("Hello, world!");
	ImGui::SetWindowSize(ImVec2((float)ImGuiWidth, (float)ImGuiHeight));
	ImGui::SetWindowPos(ImVec2((float)0, (float)0));
	ImGui::Text("This is some useful text.");
	ImGui::Checkbox("Demo Window", &show_demo_window);
	ImGui::Checkbox("Another Window", &show_another_window);
	ImGui::SliderFloat("float", &slider_f, 0.0f, 1.0f);
	ImGui::ColorEdit3("clear color", (float*)&clear_color);
	if (ImGui::Button("Button")) {
		counter++;
		io.MousePos = ImVec2(-FLT_MAX,-FLT_MAX);
	}
	ImGui::SameLine();
	ImGui::Text("counter = %d", counter);
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
	    1000.0f / io.Framerate, io.Framerate);
	ImGui::End();

	ImGui::Render();
	glViewport(0, 0, (int)DISPLAY_WIDTH, (int)DISPLAY_HEIGHT);
	glClearColor(clear_color.x * clear_color.w,
	    clear_color.y * clear_color.w, clear_color.z * clear_color.w,
	    clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	return (0);
}

int
main_clean(void)
{

	ImGui_ImplOpenGL3_Shutdown();
	ImGui::DestroyContext();

	return 0;
}

bool
main_process_event(int type, double x, double y)
{
	ImGuiIO& io = ImGui::GetIO();

	switch (type) {
	case 1:
		/* Down */
		io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
		io.AddMousePosEvent(x, y);
		io.AddMouseButtonEvent(0, true);
		return true;
	case 2:
		/* Motion */
		io.AddMousePosEvent(x, y);
		return true;
	case 3:
		/* Touch UP */
		io.AddMouseButtonEvent(0, false);
		return true;
	}

	return false;
}
