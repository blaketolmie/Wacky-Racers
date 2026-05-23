#ifndef BUMPER_HIT_H
#define BUMPER_HIT_H

#include <stdbool.h>
#include "ledbuffer.h"

void bumper_hit_start(void);
void bumper_hit_stop(void);
bool bumper_hit_is_active(void);

// call every tick — handles jingle and LED strip
void bumper_hit_update(ledbuffer_t *leds);

#endif /* BUMPER_HIT_H */
