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
  
// ESTRUCTURAS PARA FORMATO WLD (basadas en Div/load.c)  
struct WLD_Point {  
    int16_t x, y;  
    int16_t Type;  
    int32_t link;  
};  
  
struct WLD_Region {  
    int16_t floor_height;  
    int16_t ceiling_height;  
    int16_t floor_texture;  
    int16_t ceiling_texture;  
    int16_t light_level;  
    int16_t flags;  
};  
  
struct WLD_Wall {  
    int16_t point1, point2;  
    int16_t region1, region2;  
    int16_t texture;  
    int16_t flags;  
    uint16_t x_offset, y_offset;  
};  
  
struct WLD_Thing {  
    float x, y, z;  
    float angle;  
    uint32_t type;  
    uint32_t flags;  
};  
  
struct WLD_Header {  
    int32_t NumPoints;  
    int32_t NumRegions;  
    int32_t NumWalls;  
    int32_t NumThings;  
    int32_t NumTextures;  
};  
  
// Estructura de textura para WLD (sin dependencias de DMAP)  
typedef struct {  
    char filename[256];  
    int graph_id;  
} WLD_TEXTURE_ENTRY;  
  
// Enumeración para tipos de mapa     
typedef enum {              
    MAP_TYPE_HEIGHTMAP = 0,  // Terreno exterior voxelspace      
    MAP_TYPE_SECTOR = 1,     // Interior estilo VPE (WLD)    
} MAP_TYPE;  
           
typedef struct {                    
    int64_t id;              
    MAP_TYPE type;  // MAP_TYPE_SECTOR para WLD  
                  
    // Datos para modo heightmap (exteriores)              
    GRAPH *heightmap;              
    GRAPH *texturemap;              
    int64_t width;              
    int64_t height;              
    float *height_cache;              
    int cache_valid;              
                  
    // Datos para modo WLD (interiores)  
    struct WLD_Point *wld_points;  
    struct WLD_Region *wld_regions;  
    struct WLD_Wall *wld_walls;  
    int num_wld_points;  
    int num_wld_regions;  
    int num_wld_walls;  
      
    WLD_TEXTURE_ENTRY *textures;  
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
    float plane_x, plane_y;    
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
        
// Funciones para mapas WLD    
extern int64_t libmod_heightmap_load_wld(INSTANCE *my, int64_t *params);      
extern int64_t libmod_heightmap_get_map_type(INSTANCE *my, int64_t *params);          
extern int64_t libmod_heightmap_render_wld_cpu(INSTANCE *my, int64_t *params);        
        
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