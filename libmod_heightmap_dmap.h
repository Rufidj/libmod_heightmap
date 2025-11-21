#ifndef __LIBMOD_HEIGHTMAP_DMAP_H      
#define __LIBMOD_HEIGHTMAP_DMAP_H      
      
#include <stdint.h>      
#pragma pack(push, 1)    
  
// ============================================================================  
// FORMATO DMAP MULTI-NIVEL (basado en sdl2-raycast)  
// ============================================================================  
  
// Header DMAP multi-nivel  
typedef struct {  
    char magic[4];              // "DMAP"  
    uint32_t grid_width;        // Ancho en tiles  
    uint32_t grid_height;       // Alto en tiles  
    uint32_t num_levels;        // Número de niveles verticales (ej: 3)  
    float tile_size;            // Tamaño de cada tile (ej: 64.0)  
    uint32_t num_textures;  
    uint32_t num_thin_walls;    // Paredes delgadas para geometría compleja  
    uint32_t num_things;  
} DMAP_HEADER;  
  

// Celda de tile  
typedef struct {  
    uint16_t wall_texture_id;   // 0 = vacío, >0 = textura de pared  
    uint16_t floor_texture_id;  
    uint16_t ceiling_texture_id;  
    float floor_height;  
    float ceiling_height;  
    uint8_t light_level;        // Nivel de iluminación 0-255  
    uint8_t flags;              // Flags de comportamiento del tile  
} TILE_CELL;
  
// Thin Wall (pared delgada para columnas, rampas diagonales)  
typedef struct {  
    float x1, y1;               // Punto inicial  
    float x2, y2;               // Punto final  
    float z;                    // Altura base  
    float height;               // Altura de la pared  
    uint16_t texture_id;        // Textura  
    uint8_t level;              // Nivel vertical (0, 1, 2...)  
    uint8_t flags;              // Flags especiales  
    float slope;                // Pendiente para rampas (0.0 = plano)  
} THIN_WALL;  
  
// Textura  
typedef struct {  
    char filename[256];  
    int graph_id;               // Se carga en runtime  
} TEXTURE_ENTRY;  
  
// Thing (entidad)  
typedef struct {      
    float x, y, z;      
    float angle;      
    uint32_t type;      
    uint32_t flags;      
} THING;  
  
// Flags para tiles  
#define TILE_FLAG_DOOR          0x01  
#define TILE_FLAG_TRANSPARENT   0x02  
#define TILE_FLAG_ELEVATOR      0x04  
  
// Flags para thin walls  
#define THIN_WALL_FLAG_BLOCKS   0x01  
#define THIN_WALL_FLAG_PORTAL   0x02  
#define THIN_WALL_FLAG_RAMP     0x02  
#pragma pack(pop)    
#endif