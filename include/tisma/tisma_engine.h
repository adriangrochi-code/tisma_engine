/* Tisma engine C API (docs/C_API.md; mobile_app.md, rule 2): the engine for
 * apps that cannot link C++ directly (the Android app through the NDK).
 * Plain C, no platform dependency. Status: interface agreed on 2026-11-04;
 * since 2026-11-12 the context, opening, settings, the memory estimate, the
 * slice with progress and cancel, the summary and the G-code are
 * implemented. The preview by layers and the analysis answer TISMA_ERROR
 * ("not implemented yet").
 *
 * Threads: a context is used from one thread at a time. Slicing and
 * analysing run on the engine's own worker; the caller polls. No callback
 * ever runs on the caller's thread.
 *
 * Strings: UTF-8, owned by the context, valid until the next call on the
 * same context that returns a string of the same kind (copy them). */

#ifndef TISMA_ENGINE_H
#define TISMA_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tisma_ctx tisma_ctx;

/* Stable error codes: apps translate by code (cloud.md, rule 19a). */
typedef enum tisma_code {
    TISMA_OK = 0,
    TISMA_ERROR = 1,           /* anything else; see the message */
    TISMA_BAD_ARGUMENT = 2,    /* a null pointer, a wrong size or index */
    TISMA_CANNOT_READ = 3,     /* a path that does not open */
    TISMA_BAD_MODEL = 4,       /* not a model the engine reads, or empty */
    TISMA_BAD_SETTINGS = 5,    /* settings JSON that does not parse or is out of range */
    TISMA_NOTHING_TO_SLICE = 6,
    TISMA_BUSY = 7,            /* a job is running on this context */
    TISMA_CANCELLED = 8,
    TISMA_NOT_READY = 9,       /* no finished result to read yet */
    TISMA_TOO_LARGE = 10,      /* more memory than the limit set */
    TISMA_NEEDS_CONDITIONS = 11, /* analyze: no fixed zone or no force */
    TISMA_CANNOT_WRITE = 12
} tisma_code;

typedef struct tisma_status {
    tisma_code code;
    const char* message; /* English, for logs; null when TISMA_OK */
} tisma_status;

/* The context. */
tisma_ctx* tisma_create(void);
void tisma_destroy(tisma_ctx* ctx);
const char* tisma_version(void); /* "0.10.0-alpha.3", static */

/* The plate: a model (STL, OBJ, 3MF) or a Tisma project (3MF with
 * settings), replacing what was open. `name` gives the format for bytes
 * ("part.stl"). */
tisma_status tisma_open_path(tisma_ctx* ctx, const char* path);
tisma_status tisma_open_bytes(tisma_ctx* ctx, const void* data, size_t size, const char* name);

/* Settings as JSON with docs/CONFIG.md's keys: set replaces the whole set
 * (missing keys take their defaults); get returns the effective set. */
tisma_status tisma_set_settings_json(tisma_ctx* ctx, const char* json);
const char* tisma_get_settings_json(tisma_ctx* ctx);

/* Before slicing (mobile_app.md, rule 19): the peak memory the slice would
 * take, in bytes, and a limit above which slicing refuses with
 * TISMA_TOO_LARGE (0: none). */
uint64_t tisma_estimate_memory(tisma_ctx* ctx);
void tisma_set_memory_limit(tisma_ctx* ctx, uint64_t bytes);

/* Jobs: one at a time per context. */
typedef enum tisma_state {
    TISMA_IDLE = 0,
    TISMA_RUNNING = 1,
    TISMA_DONE = 2,
    TISMA_FAILED = 3,
    TISMA_STOPPED = 4 /* cancelled */
} tisma_state;

typedef struct tisma_progress {
    tisma_state state;
    int percent;          /* 0 to 100 */
    const char* stage;    /* "slicing", "gcode", "voxels", "solving"... */
    tisma_status error;   /* when TISMA_FAILED */
} tisma_progress;

tisma_status tisma_slice_start(tisma_ctx* ctx);
tisma_progress tisma_poll(tisma_ctx* ctx);
void tisma_cancel(tisma_ctx* ctx);

/* The finished slice. The summary JSON: {"print_time_s", "filament_mm",
 * "filament_g" (null without the filament's density), "layers", "cost"
 * (filament_g times extrusion.cost per kg, rounded to cents; null when
 * either is unknown), "warnings": [...]}. */
const char* tisma_summary_json(tisma_ctx* ctx);
tisma_status tisma_write_gcode(tisma_ctx* ctx, const char* path);
/* The G-code in pieces: up to `capacity` bytes from `offset`; *written gets
 * how many (0 at the end). */
tisma_status tisma_read_gcode(tisma_ctx* ctx, uint64_t offset, char* buffer, size_t capacity, size_t* written);

/* The preview, layer by layer so a phone never holds it all. */
typedef enum tisma_path_type {
    TISMA_PATH_OUTER_WALL = 0,
    TISMA_PATH_INNER_WALL = 1,
    TISMA_PATH_SOLID_INFILL = 2,
    TISMA_PATH_TOP = 3,
    TISMA_PATH_BOTTOM = 4,
    TISMA_PATH_SPARSE_INFILL = 5,
    TISMA_PATH_SUPPORT = 6,
    TISMA_PATH_BRIDGE = 7,
    TISMA_PATH_SKIRT = 8,
    TISMA_PATH_TRAVEL = 9,
    TISMA_PATH_OTHER = 10
} tisma_path_type;

typedef struct tisma_segment {
    float x0, y0, z0, x1, y1, z1; /* mm */
    float width, height;         /* mm */
    float speed;                 /* mm/s */
    uint8_t type;                /* tisma_path_type */
} tisma_segment;

int tisma_layer_count(tisma_ctx* ctx);
double tisma_layer_z(tisma_ctx* ctx, int layer);
/* Up to `capacity` segments of layers [first, first + count), starting at
 * the `skip`-th; *written gets how many. Call again with skip += written
 * until it is 0. */
tisma_status tisma_preview_segments(tisma_ctx* ctx, int first, int count, size_t skip, tisma_segment* out,
                                    size_t capacity, size_t* written);

/* Engineering (engineering_panel.md): the conditions come with a project;
 * the quality is "fast", "normal", "high" or "extreme". Progress by
 * tisma_poll; the result is the structural report JSON (verdict, safety
 * factor and its likely range, displacement, mass). */
tisma_status tisma_analyze_start(tisma_ctx* ctx, int object, const char* quality);
const char* tisma_analysis_json(tisma_ctx* ctx);

/* The command line in the same process, for platforms that cannot start a
 * program (iOS: apps/ios/PLAN.md). argv as main gets it (argv[0] is the
 * program's name and is skipped); the return value is the program's exit
 * code (docs/CLI.md). What it would print goes to *out and *err, each a
 * NUL-terminated buffer the caller frees with tisma_free (sizes without the
 * NUL); out, err and the sizes may be null when not wanted. Implemented.
 * Safe to call from several threads at once. */
int tisma_run_cli(int argc, const char* const* argv, char** out, size_t* out_size, char** err, size_t* err_size);
void tisma_free(void* buffer);

#ifdef __cplusplus
}
#endif

#endif /* TISMA_ENGINE_H */
