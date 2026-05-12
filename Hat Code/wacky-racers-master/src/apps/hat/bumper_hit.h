#ifndef BUMPER_HIT_H
#define BUMPER_HIT_H

// Trigger the bumper hit effect.
void bumper_hit_start(void);

// Call once per pacer tick. Returns 1 while active, 0 when done.
int bumper_hit_update(void);

#endif /* BUMPER_HIT_H */
