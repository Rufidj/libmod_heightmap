/*  
 * Copyright (C) 2025 - Heightmap Module for BennuGD2  
 * This file is part of Bennu Game Development  
 */  
  
#ifndef __LIBMOD_HEIGHTMAP_H  
#define __LIBMOD_HEIGHTMAP_H  
  
#include <stdint.h>  
#include <math.h>  
  
/* Inclusiones necesarias de BennuGD2 */  
#include "bgddl.h"  
#include "libbggfx.h"  
#include "g_bitmap.h"  
#include "g_blit.h"  
#include "g_pixel.h"  
#include "g_clear.h"  
#include "g_grlib.h"  
#include "xstrings.h"  
#include "m_map.h"  

  
/* Estructuras para el módulo de heightmap */  
typedef struct {  
    int64_t id;  
    GRAPH *heightmap;  
    GRAPH *texturemap; // Nuevo: Mapa de texturas  
    int64_t width;  
    int64_t height;  
    float *height_cache;  
    int cache_valid;  
} HEIGHTMAP;  
  
typedef struct {  
    float x, y, z;  
    float angle, pitch;  
    float fov;  
    float near, far;  // Agregar estos campos  
} CAMERA_3D; 

typedef struct {    
    int screen_x, screen_y;    
    float distance;          // NUEVO: distancia real para depth buffer  
    float distance_scale;    // Mantener para compatibilidad  
    int scaled_width, scaled_height;    
    uint8_t alpha;    
    int valid;    
    float fog_tint_factor;
} BILLBOARD_PROJECTION;


/* Constantes */  
#define MAX_HEIGHTMAPS 256  
#define DEFAULT_FOV 60.0f  
#define DEFAULT_NEAR 1.0f  
#define DEFAULT_FAR 1000.0f  
  
/* Variables globales del módulo */  
extern HEIGHTMAP heightmaps[MAX_HEIGHTMAPS];  
extern CAMERA_3D camera;  
extern int64_t next_heightmap_id;  
  
/* Funciones principales */  
extern int64_t libmod_heightmap_load(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_create(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_unload(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_get_height(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_set_camera(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_render_3d(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_load_texture(INSTANCE *my, int64_t *params); // Nueva función  
extern int64_t libmod_heightmap_update_sprite_3d(INSTANCE *my, int64_t *params);
extern int64_t libmod_heightmap_load_overlay_mask(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_load_bridge_texture(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_set_bridge_height(INSTANCE *my, int64_t *params);
extern int64_t libmod_heightmap_update_billboard_graph(INSTANCE *my, int64_t *params);
// Declaraciones forward para skybox  
static uint32_t sample_sky_texture(float screen_x, float screen_y, float camera_angle, float camera_pitch, float time);  
static void render_skybox(float camera_angle, float camera_pitch, float time, int quality_step);

// Declaraciones forward para efectos atmosféricos  
static void render_atmospheric_particles(float time, int quality_step, HEIGHTMAP *hm); 
static float calculate_atmospheric_lighting(float distance, float height);  
static float calculate_volumetric_fog(float world_z, float distance);


/* Funciones internas */  
extern float get_height_at(HEIGHTMAP *hm, float x, float y);  
extern void build_height_cache(HEIGHTMAP *hm);  
  
#endif