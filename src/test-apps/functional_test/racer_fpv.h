#ifndef RACER_FPV_H
#define RACER_FPV_H

#include <stdbool.h>

#include "button.h"

typedef struct
{
    button_t button;
    bool enabled;
} racer_fpv_t;

int racer_fpv_init(racer_fpv_t *fpv);
void racer_fpv_update(racer_fpv_t *fpv);
void racer_fpv_apply(racer_fpv_t *fpv);

#endif
