#ifdef __cplusplus
extern "C" {
#endif

int mdgui_init(void);
int mdgui_render(int i);

int input_init(void);
int input_poll_once(void);

bool mdgui_input_event(int type, double x, double y);

#ifdef __cplusplus
}
#endif
