#ifndef __LIBMOD_HEIGHTMAP_DMAP_H      
#define __LIBMOD_HEIGHTMAP_DMAP_H      
      
#include <stdint.h>      
#pragma pack(push, 1)    
  
// Header DMAP (tile-based)  
typedef struct {  
    char magic[4];              // "DMAP"  
    uint32_t grid_width;        // Ancho en tiles  
    uint32_t grid_height;       // Alto en tiles  
    float tile_size;            // Tamaño de cada tile (ej: 64.0)  
    uint32_t num_textures;  
    uint32_t num_things;  
} DMAP_HEADER;  
  
// Celda de tile  
typedef struct {  
    uint16_t wall_texture_id;   // 0 = vacío, >0 = textura de pared  
    uint16_t floor_texture_id;  
    uint16_t ceiling_texture_id;  
    float floor_height;  
    float ceiling_height;  
    uint8_t flags;              // Portal, transparente, etc.  
} TILE_CELL;  
  
// Textura  
typedef struct {  
    char filename[256];  
    int graph_id;               // ID del gráfico cargado  
} TEXTURE_ENTRY;  
  
// Thing (entidad)  
typedef struct {  
    float x, y, z;  
    float angle;  
    uint32_t type;  
    uint32_t flags;  
} THING;  
  
#pragma pack(pop)    
#endif