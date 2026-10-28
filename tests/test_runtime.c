#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime.h"

static int ran, failed;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++ran;                                                               \
        if (!(cond)) { ++failed; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    char *s = malloc((size_t)n + 1);
    if (fread(s, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(s); return NULL; }
    s[n] = '\0';
    fclose(f);
    return s;
}

static int count(const char *hay, const char *needle) {
    int n = 0;
    for (const char *p = strstr(hay, needle); p; p = strstr(p + 1, needle)) n++;
    return n;
}

#define OUT "build/test_runtime.svg"

static char *draw_and_read(void (*body)(void)) {
    turtle_init(10, 90, 0, 0, 0, OUT);
    body();
    turtle_finish();
    return slurp(OUT);
}

static void line_only(void) { turtle_forward(10); turtle_turn(90); turtle_forward(10); }
static void move_gap(void)  { turtle_forward(10); turtle_move(10); turtle_forward(10); }
static void branch(void) {
    turtle_forward(10);
    turtle_push(); turtle_turn(90); turtle_forward(10); turtle_pop();
    turtle_turn(-90); turtle_forward(10);
}
static void restore(void) {
    turtle_push(); turtle_turn(90); turtle_forward(50); turtle_pop();
    turtle_forward(10);
}
static void move_first(void) { turtle_move(100); turtle_forward(10); }
static void nothing(void) { turtle_move(10); turtle_push(); turtle_pop(); }
static void deep(void) {
    for (int i = 0; i < 5000; i++) { turtle_push(); turtle_forward(1); }
    for (int i = 0; i < 5000; i++) turtle_pop();
    turtle_turn(90); turtle_forward(1);
}

#define OUT3 "build/test_runtime.f3d"

static void three_d(void) {
    printf("runtime 3d\n");
    turtle3_init(OUT3);
    turtle3_begin(0, 0, 0, 0, 1, 1, 0.5, "A", "#112233,#445566");
    turtle3_forward(2);
    turtle3_yaw(90);
    turtle3_forward(1);
    turtle3_push(); turtle3_pitch(90); turtle3_forward(1); turtle3_pop();
    turtle3_forward(1);
    turtle3_thin(0.5); turtle3_color(); turtle3_blade(1);
    turtle3_move(5); turtle3_roll(45);
    turtle3_end();
    turtle3_begin(10, 0, 5, 90, 2, 1, 0.5, "B", "");
    turtle3_forward(1);
    turtle3_pitch(90); turtle3_forward(1);
    turtle3_end();
    turtle3_finish();

    char *s = slurp(OUT3);
    CHECK(s && strncmp(s, "# FRACTL 3D segments v1\n", 24) == 0);
    CHECK(s && strstr(s, "o A 0 #112233,#445566\n"));
    CHECK(s && strstr(s, "\nt 0.000 0.000 0.000 0.000 2.000 0.000 0.500 0\n"));
    CHECK(s && strstr(s, "\nt 0.000 2.000 0.000 -1.000 2.000 0.000 0.500 0\n"));
    CHECK(s && strstr(s, "\nt -1.000 2.000 0.000 -1.000 2.000 -1.000 0.500 0\n"));
    CHECK(s && strstr(s, "\nt -1.000 2.000 0.000 -2.000 2.000 0.000 0.500 0\n"));
    CHECK(s && strstr(s, "\nb -2.000 2.000 0.000 -3.000 2.000 0.000 0.000 -1.000 0.000 0.500 0.250 1\n"));
    CHECK(s && strstr(s, "\no B 1 -\n"));
    CHECK(s && strstr(s, "\nt 10.000 0.000 5.000 10.000 2.000 5.000 1.000 0\n"));
    CHECK(s && strstr(s, "\nt 10.000 2.000 5.000 8.000 2.000 5.000 1.000 0\n"));
    CHECK(s && count(s, "\nt ") == 6);
    CHECK(s && count(s, "\nb ") == 1);
    free(s);

    fractl_seed(5);
    double a = fractl_rand(0), b = fractl_rand(0);
    fractl_seed(5);
    CHECK(fractl_rand(0) == a);
    CHECK(a != b && a >= 0.0 && a < 1.0 && b >= 0.0 && b < 1.0);
    fractl_seed(5);
    double l1 = fractl_rand(1);
    fractl_seed(5);
    fractl_rand(0);
    fractl_rand(0);
    CHECK(fractl_rand(1) == l1);
    fractl_seed(6);
    CHECK(fractl_rand(0) != a);
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "underflow") == 0) {
        turtle_init(1, 90, 0, 0, 0, OUT);
        turtle_pop();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "underflow3") == 0) {
        turtle3_init(OUT3);
        turtle3_begin(0, 0, 0, 0, 1, 1, 0.5, "A", "");
        turtle3_pop();
        return 0;
    }

    printf("runtime\n");
    char *s;

    s = draw_and_read(line_only);
    CHECK(s && count(s, "<polyline") == 1);
    CHECK(s && strstr(s, "points=\"10.00,20.00 20.00,20.00 20.00,10.00\""));
    CHECK(s && strstr(s, "width=\"30.00\" height=\"30.00\""));
    free(s);

    s = draw_and_read(move_gap);
    CHECK(s && count(s, "<polyline") == 2);
    CHECK(s && strstr(s, "points=\"10.00,10.00 20.00,10.00\""));
    CHECK(s && strstr(s, "points=\"30.00,10.00 40.00,10.00\""));
    free(s);

    s = draw_and_read(branch);
    CHECK(s && count(s, "<polyline") == 2);
    CHECK(s && strstr(s, "points=\"10.00,20.00 20.00,20.00 20.00,10.00\""));
    CHECK(s && strstr(s, "points=\"20.00,20.00 20.00,30.00\""));
    CHECK(s && strstr(s, "width=\"30.00\" height=\"40.00\""));
    free(s);

    s = draw_and_read(restore);
    CHECK(s && count(s, "<polyline") == 2);
    CHECK(s && strstr(s, "points=\"10.00,60.00 20.00,60.00\""));
    free(s);

    s = draw_and_read(move_first);
    CHECK(s && count(s, "<polyline") == 1);
    CHECK(s && strstr(s, "width=\"30.00\" height=\"20.00\""));
    free(s);

    s = draw_and_read(nothing);
    CHECK(s && count(s, "<polyline") == 0);
    free(s);

    s = draw_and_read(deep);
    CHECK(s && count(s, "<polyline") == 2);
    free(s);

    char cmd[512];
    snprintf(cmd, sizeof cmd, "%s underflow 2>build/test_runtime.err", argv[0]);
    int rc = system(cmd);
    char *err = slurp("build/test_runtime.err");
    CHECK(rc != 0);
    CHECK(err && strstr(err, "state stack underflow"));
    free(err);

    three_d();
    snprintf(cmd, sizeof cmd, "%s underflow3 2>build/test_runtime.err", argv[0]);
    rc = system(cmd);
    err = slurp("build/test_runtime.err");
    CHECK(rc != 0);
    CHECK(err && strstr(err, "state stack underflow"));
    free(err);

    printf("%d runtime checks, %d failed\n", ran, failed);
    return failed ? 1 : 0;
}
