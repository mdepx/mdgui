IMGUI_DIR = ./imgui

MORELLO_FLAGS =
PREFIX = /usr/local
WIDTH = 1080
HEIGHT = 1240

# Uncomment for ARM Morello
# MORELLO_FLAGS = -march=morello+noa64c -Xclang -morello-vararg=new -mabi=aapcs
# PREFIX = /usr/local64
# WIDTH = 1920
# HEIGHT = 1080

CFLAGS = -I${PREFIX}/include/libdrm -I${PREFIX}/include
CFLAGS += -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
CFLAGS += -DDISPLAY_WIDTH=${WIDTH} -DDISPLAY_HEIGHT=${HEIGHT}
CFLAGS += -Wall -Wformat
CFLAGS += ${MORELLO_FLAGS}

LDFLAGS = -L${PREFIX}/lib -lEGL -lgbm -ldrm -lm -lOpenGL -linput -ludev

EXEC = mdgui

OBJS = mdgui.o input.o drmgl.o

IMGUI_SRC =						\
	$(IMGUI_DIR)/imgui.o				\
	$(IMGUI_DIR)/imgui_demo.o			\
	$(IMGUI_DIR)/imgui_draw.o			\
	$(IMGUI_DIR)/imgui_tables.o			\
	$(IMGUI_DIR)/imgui_widgets.o			\
	$(IMGUI_DIR)/backends/imgui_impl_opengl2.o	\
	$(IMGUI_DIR)/backends/imgui_impl_opengl3.o

IMGUI_OBJS = $(IMGUI_SRC:.cpp=.o)

.cpp.o:
	c++ $(CFLAGS) -g -c -o $@ $<

.c.o:
	cc $(CFLAGS) -g -c -o $@ $<

all:	$(EXEC)

${EXEC}: ${IMGUI_OBJS} ${OBJS}
	c++ -g -o mdgui ${OBJS} ${IMGUI_OBJS} ${LDFLAGS} ${MORELLO_FLAGS}
	strip mdgui
	scp mdgui 10.0.0.1:~/kmscube

clean:
	@rm -f ${EXEC} ${OBJS} ${IMGUI_OBJS}
