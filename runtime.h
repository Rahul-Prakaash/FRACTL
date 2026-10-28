#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void turtle_init(double step, double angle, double x, double y, double heading, const char *path);
void turtle_forward(double dist);
void turtle_move(double dist);
void turtle_turn(double degrees);
void turtle_push(void);
void turtle_pop(void);
void turtle_finish(void);

void turtle3_init(const char *path);
void turtle3_begin(double x, double y, double z, double yaw, double scale,
                   double width, double taper, const char *name, const char *colors);
void turtle3_forward(double len);
void turtle3_blade(double len);
void turtle3_move(double len);
void turtle3_yaw(double degrees);
void turtle3_pitch(double degrees);
void turtle3_roll(double degrees);
void turtle3_push(void);
void turtle3_pop(void);
void turtle3_thin(double factor);
void turtle3_color(void);
void turtle3_end(void);
void turtle3_finish(void);

void   fractl_seed(uint64_t seed);
double fractl_rand(int32_t level);

#ifdef __cplusplus
}
#endif
