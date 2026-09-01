#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>

bool renderer_init();
void renderer_shutdown();
void renderer_frame();

// Set avatar position by geographic coords (lat, lon). The renderer will transform to world coords.
void renderer_set_avatar_geoposition(double lat, double lon);

#endif
