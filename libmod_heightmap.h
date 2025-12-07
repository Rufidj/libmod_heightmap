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


    
#define MAX_POINTS  8192  
#define MAX_WALLS   8192  
#define MAX_REGIONS 4096  
#define MAX_FLAGS   1000
    
    
// ============================================================================      
// FORMATO TEX - Sistema de texturas simple      
// ============================================================================    
    
// Header para formato TEX    
typedef struct {    
    char magic[4];    
    uint32_t version;    
    uint16_t num_images;    
    uint8_t reserved[6];    
} TEX_HEADER;    
    
typedef struct {    
    uint16_t index;    
    uint16_t width;    
    uint16_t height;    
    uint16_t format;    
    uint8_t reserved[250];    
} TEX_ENTRY;    
    
      
// Estructura de textura para SECTOR      
typedef struct {      
    char filename[256];      
    int graph_id;      
} SECTOR_TEXTURE_ENTRY;      

// Estructuras mínimas para WLD (añadir a libmod_heightmap.h)  
typedef struct {  
    int code;  
    int Width, Height;  
    int Width2, Height2;  
    char *Raw;  
    int Used;  
} WLD_PicInfo;  
  
typedef struct {  
    WLD_PicInfo *pPic;  
    int code;  
} WLD_TexCon;  

#pragma pack(push, 1)  
  
typedef struct {  
    int active;  
    int x, y;  
    int number;  
} WLD_Flag;  // Total: 12 bytes  
  
typedef struct {  
    int32_t active;  
    int32_t x, y;  
    int32_t links;  
} WLD_Point;  // Compatible con tpoint  
  
typedef struct {  
    int32_t active;  
    int32_t type;  
    int32_t p1, p2;  
    int32_t front_region, back_region;  
    int32_t texture;  
    int32_t texture_top;  
    int32_t texture_bot;  
    int32_t fade;  
} WLD_Wall;  // Compatible con twall
  
typedef struct {  
    int active;  
    int type;  
    int floor_height, ceil_height;  
    int floor_tex;  
    int ceil_tex;  
    int fade;  
} WLD_Region;  // Total: 32 bytes  

typedef struct {  
    WLD_Region *original_region;  
    WLD_Wall **wall_ptrs;  
    int num_wall_ptrs;  
} WLD_Region_Optimized;
  
#pragma pack(pop)
  
typedef struct {  
    int num_points;  
    WLD_Point **points;     // Array de punteros  
    int num_walls;  
    WLD_Wall **walls;       // Array de punteros  
    int num_regions;  
    WLD_Region **regions;   // Array de punteros  
    int num_flags;  
    WLD_Flag **flags;       // Array de punteros  
    int loaded;  
    int skybox_angle;  
    char skybox_texture[32]; 
} WLD_Map;

typedef struct {  
    int floor_height, ceil_height;  
    int floor_tex, ceil_tex;  
    int num_walls;  
    WLD_Wall **walls;  
    int *neighbors;  // Regiones adyacentes (portales)  
} WLD_Sector;  

typedef struct {  
    int region_idx;  
    float distance_offset;  
    int depth;  
} RaySegment;

static WLD_Sector *sectors = NULL;
  
// Variables globales para el sistema WLD  
static WLD_PicInfo *wld_pics[1000];  
static int wld_num_pics = 0;
      
// Enumeración para tipos de mapa         
typedef enum {                  
    MAP_TYPE_HEIGHTMAP = 0,  // Terreno exterior voxelspace               
} MAP_TYPE;      
               
typedef struct {                        
    int64_t id;                  
    MAP_TYPE type;  // MAP_TYPE_SECTOR para DMP2      
                      
    // Datos para modo heightmap (exteriores)                  
    GRAPH *heightmap;                  
    GRAPH *texturemap;                  
    int64_t width;                  
    int64_t height;                  
    float *height_cache;                  
    int cache_valid;                      
                      
} HEIGHTMAP;        
    
        

// Mover estas líneas al principio del archivo (después de los #include)  
#define MAXSECTORS 1024  
#define MAXWALLS 8192  
#define MAXSPRITES 4096  
#define MAXTILES 9216  
  
// Variables globales del Build Engine (AÑADIDAS)  

// Constantes de conversión de coordenadas                
#define WORLD_TO_SPRITE_SCALE 10.0f                
#define SPRITE_TO_WORLD_SCALE 0.1f                
                
// Constantes de renderizado                
#define DEFAULT_MAX_RENDER_DISTANCE 8000.0f                
#define DEFAULT_CHUNK_SIZE 512                
#define DEFAULT_CHUNK_RADIUS 15                
  
// Constantes de fog - MOVIDAS AQUÍ PARA DISPONIBILIDAD TEMPRANA                
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

// Añadir antes de las funciones existentes  


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
      
// Funciones para mapas DMP2        
       
          
   
      
// Funciones para sistema TEX    
extern int64_t load_tex_file(INSTANCE *my, int64_t *params);    
extern GRAPH *get_tex_image(int index);      
      
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