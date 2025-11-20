#ifndef __LIBMOD_HEIGHTMAP_DMAP_H    
#define __LIBMOD_HEIGHTMAP_DMAP_H    
    
#include <stdint.h>    
#pragma pack(push, 1)  
  
// Constantes (mantener las existentes)  
#define SECTOR_TYPE_NORMAL 0    
#define SECTOR_TYPE_STAIRS 1    
#define SECTOR_TYPE_ELEVATOR 2    
#define SECTOR_TYPE_RAMP 3    
  
#define ACTION_NONE 0    
#define ACTION_OPEN_DOOR 1    
#define ACTION_ACTIVATE_ELEVATOR 2    
#define ACTION_TRIGGER_SCRIPT 3    
  
#define WALL_FLAG_BLOCKS_MOVEMENT 0x01    
#define WALL_FLAG_TRIGGER 0x02    
#define WALL_FLAG_TRANSPARENT 0x04    
  
// ============================================================================  
// FORMATO DMAP v3 ÚNICO - Sistema simplificado estilo VPE  
// ============================================================================  
  
// Header simplificado  
typedef struct {  
    char magic[4];              // "DMAP"  
    uint32_t version;           // 3  
    uint32_t num_sectors;  
    uint32_t num_walls;  
    uint32_t num_textures;  
    uint32_t num_things;  
} DMAP_HEADER_V3;  
  
// Vértice 2D  
typedef struct {  
    float x, y;  
} VERTEX;  
  
// Textura  
typedef struct {      
    char filename[256];  
} TEXTURE_ENTRY_V2;  
  
// Sector simplificado con vértices propios  
typedef struct {  
    uint32_t id;  
    float floor_height;  
    float ceiling_height;  
      
    // Rampas  
    float floor_slope_x;  
    float floor_slope_y;  
      
    // Ascensores  
    uint8_t is_dynamic;  
    float target_floor_height;  
    float target_ceiling_height;  // ← AÑADIR ESTE CAMPO 
    float move_speed;  
    uint8_t move_state;  
      
    // Texturas  
    uint32_t floor_texture_id;  
    uint32_t ceiling_texture_id;  
    uint32_t wall_texture_id;  
      
    // Tipo  
    uint32_t sector_type;  
      
    // Iluminación  
    uint8_t light_level;  
      
    // Geometría (vértices embebidos)  
    uint32_t num_vertices;  
    VERTEX *vertices;  // Array dinámico  
} SECTOR_V3;  
  
// Pared simplificada  
typedef struct {  
    uint32_t sector1_id;  
    uint32_t sector2_id;  
    float x1, y1, x2, y2;  
    uint32_t texture_id;  
    uint8_t flags;  
    uint8_t action_type;  
} WALL_V3;  
  
// Thing (entidad)  
typedef struct {    
    float x, y, z;    
    float angle;    
    uint32_t type;    
    uint32_t flags;    
} THING;  
  
#pragma pack(pop)  
#endif