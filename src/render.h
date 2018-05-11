#ifndef RENDER_H
#define RENDER_H 1

/* Experimental UI default */
extern const int default_ui_color;

typedef struct {int y, x;} vec2;

vec2 inline yx(int y, int x) {
        vec2 ret = {y, x};
        return ret; /* Passing by value, as syntax+type sugar. */
}

/* Render panels */

void render_col_addrs(vec2 base, const int dim_x, 
        size_t bytes_per_row, view v);

void render_row_addrs(vec2 base, vec2 dim, 
        const size_t buf_sz, const size_t cursor, 
        view v, size_t bytes_per_row);

void render_grid(vec2 base, vec2 dim, 
        const size_t buf_sz, const size_t cursor, 
        unsigned char* buf, view v, size_t bytes_per_full_row);

/* Utility */

uint32_t dlog2(uint32_t v);
#endif
