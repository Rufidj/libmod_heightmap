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
#include <GL/glew.h>      
    
// Incluir estructuras DMAP v3  
#include "libmod_heightmap_dmap.h"    
      
// ESTRUCTURAS PARA MAPAS DEL MODULO      
// Enumeración para tipos de mapa          
typedef enum {          
    MAP_TYPE_HEIGHTMAP = 0,  // Terreno exterior voxelspace  
    MAP_TYPE_SECTOR = 1       // Interior estilo VPE (DMAP v3)  
} MAP_TYPE;          
          
// Entrada en tabla de texturas          
typedef struct {          
    char filename[256];          
    int graph_id;          
} TEXTURE_ENTRY;        
        
/* Estructura principal del módulo de heightmap */          
typedef struct {                
    int64_t id;          
    MAP_TYPE type;  // Tipo de mapa  
              
    // Datos para modo heightmap (exteriores)          
    GRAPH *heightmap;          
    GRAPH *texturemap;          
    int64_t width;          
    int64_t height;          
    float *height_cache;          
    int cache_valid;          
              
    // Datos para modo sector (interiores DMAP v3)  
    SECTOR_V3 *sectors;  
    int num_sectors;          
    WALL_V3 *walls;  
    int num_walls;          
    THING *things;  
    int num_things;    
    TEXTURE_ENTRY *textures;  
    int num_textures;          
} HEIGHTMAP;      

// Estructura de clip buffer por columna (equivalente a VLine de VPE)  
typedef struct {  
    int top;     // Límite superior de píxel visible  
    int bottom;  // Límite inferior de píxel visible  
} COLUMN_CLIP;  
      
// Constantes de conversión de coordenadas        
#define WORLD_TO_SPRITE_SCALE 10.0f        
#define SPRITE_TO_WORLD_SCALE 0.1f        
        
// Constantes de renderizado        
#define DEFAULT_MAX_RENDER_DISTANCE 8000.0f        
#define DEFAULT_CHUNK_SIZE 512        
#define DEFAULT_CHUNK_RADIUS 15        
        
// Constantes de fog        
#define FOG_MIN_VISIBILITY 0.6f        
#define FOG_MAX_DISTANCE 800.0f      
      
// Constantes de proyección        
#define PROJECTION_HEIGHT_SCALE 300.0f        
#define PROJECTION_CENTER_Y 120.0f        
        
// Constantes de calidad de renderizado        
#define QUALITY_STEP_NEAR 0.2f        
#define QUALITY_STEP_MID 0.5f        
#define QUALITY_STEP_FAR 1.0f        
#define QUALITY_DISTANCE_NEAR 50.0f        
#define QUALITY_DISTANCE_MID 200.0f        
        
// Constantes de agua        
#define WATER_WAVE_FREQUENCY 0.05f        
#define WATER_UV_SCALE 0.01f        
#define WATER_TIME_SCALE_U 0.1f        
#define WATER_TIME_SCALE_V 0.05f      
      
        
typedef struct {        
    float x, y, z;        
    float angle, pitch;        
    float fov;        
    float near, far;    
} CAMERA_3D;       
      
typedef struct {          
    int screen_x, screen_y;          
    float distance;    
    float distance_scale;    
    int scaled_width, scaled_height;          
    uint8_t alpha;          
    int valid;          
    float fog_tint_factor;      
} BILLBOARD_PROJECTION;      
      
/* Constantes */        
#define MAX_HEIGHTMAPS 512        
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
extern int64_t libmod_heightmap_load_texture(INSTANCE *my, int64_t *params);    
extern int64_t libmod_heightmap_update_sprite_3d(INSTANCE *my, int64_t *params);      
extern int64_t libmod_heightmap_load_overlay_mask(INSTANCE *my, int64_t *params);        
extern int64_t libmod_heightmap_load_bridge_texture(INSTANCE *my, int64_t *params);        
extern int64_t libmod_heightmap_set_bridge_height(INSTANCE *my, int64_t *params);      
extern int64_t libmod_heightmap_update_billboard_graph(INSTANCE *my, int64_t *params);      
    
// Declaración para renderizado GPU        
extern int64_t libmod_heightmap_render_voxelspace_gpu(INSTANCE *my, int64_t *params);      
    
// Funciones para mapas sector DMAP v3  
extern int64_t libmod_heightmap_load_dmap(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_get_map_type(INSTANCE *my, int64_t *params);      
extern int64_t libmod_heightmap_render_sector_cpu(INSTANCE *my, int64_t *params);    
    
// Funciones DMAP v3 para sectores dinámicos    
extern int64_t libmod_heightmap_activate_elevator(INSTANCE *my, int64_t *params);    
extern int64_t libmod_heightmap_set_sector_target_height(INSTANCE *my, int64_t *params);    
extern int64_t libmod_heightmap_check_wall_collision(INSTANCE *my, int64_t *params);    
extern int64_t libmod_heightmap_get_sector_at_position(INSTANCE *my, int64_t *params);    
extern int64_t libmod_heightmap_activate_wall_action(INSTANCE *my, int64_t *params);    
extern int64_t libmod_heightmap_update_dynamic_sectors(INSTANCE *my, int64_t *params);    
extern int64_t libmod_heightmap_get_floor_height_at_point(INSTANCE *my, int64_t *params);    
    
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
    
/* Funciones internas para tiles */        
extern void build_tile_height_cache(HEIGHTMAP *hm, int tile_index);        
extern void load_tile_from_folder_on_demand(HEIGHTMAP *hm, int tile_x, int tile_y);        
extern void load_tile_from_fpg_on_demand(HEIGHTMAP *hm, int tile_x, int tile_y);        
extern float get_height_from_tile_cache(HEIGHTMAP *hm, int tile_index, float local_x, float local_y);        
extern int64_t libmod_heightmap_load_tiles_from_folder(INSTANCE *my, int64_t *params);      
    
/* Funciones internas para DMAP v3 */    
extern void update_dynamic_sectors(HEIGHTMAP *hm, float delta_time);    
extern int check_wall_collision(float x, float y, HEIGHTMAP *hm);    
extern int get_sector_at_position(HEIGHTMAP *hm, float x, float y);    
extern float floor_height_at_point(SECTOR_V3 *sector, float world_x, float world_y);  
    
#endif