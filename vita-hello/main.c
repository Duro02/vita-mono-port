#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

int main(void) {
    vita2d_init();
    vita2d_pvf *font = vita2d_load_default_pvf();

    while (1) {
        vita2d_start_drawing();
        vita2d_clear_screen();

        vita2d_pvf_draw_text(font, 120, 200, RGBA8(0, 255, 0, 255), 2.0f,
                             "Hello Vita! Pipeline OK");

        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    vita2d_free_pvf(font);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
