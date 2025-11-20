#ifndef __LIBMOD_HEIGHTMAP_DMAP_H  
#define __LIBMOD_HEIGHTMAP_DMAP_H  
  
#include <stdint.h>  
#pragma pack(push, 1)  // ← AGREGAR AQUÍ    
// ============================================================================  
// FORMATO DMAP v2 - Sistema de mapas por sectores estilo DOOM  
// ============================================================================  
  
// Constantes para tipos de sectores  
#define SECTOR_TYPE_NORMAL 0  
#define SECTOR_TYPE_STAIRS 1  
#define SECTOR_TYPE_ELEVATOR 2  
#define SECTOR_TYPE_RAMP 3  
  
// Constantes para acciones de paredes  
#define ACTION_NONE 0  
#define ACTION_OPEN_DOOR 1  
#define ACTION_ACTIVATE_ELEVATOR 2  
#define ACTION_TRIGGER_SCRIPT 3  
  
// Constantes para flags de paredes  
#define WALL_FLAG_BLOCKS_MOVEMENT 0x01  
#define WALL_FLAG_TRIGGER 0x02  
#define WALL_FLAG_TRANSPARENT 0x04  
  
// Constantes para flags de sidedefs  
#define SIDEDEF_FLAG_TRANSPARENT 0x01  
#define SIDEDEF_FLAG_PORTAL 0x02  
  
// ============================================================================  
// ESTRUCTURAS DEL FORMATO DMAP v2  
// ============================================================================  
  
// Header del archivo .dmap  
typedef struct {  
    char magic[4];              // "DMAP"  
    uint32_t version;           // 2  
    uint32_t num_sectors;  
    uint32_t num_walls;  
    uint32_t num_textures;  
    uint32_t num_vertices;      // Vértices compartidos  
    uint32_t num_sidedefs;      // Atributos por lado de pared  
    uint32_t num_things;        // Entidades del mapa  
    uint32_t num_bsp_nodes;     // Nodos del árbol BSP  
    uint32_t num_subsectors;    // Subsectores (hojas del BSP)  
    uint32_t num_segs;          // Segmentos de paredes  
    uint32_t blockmap_width;    // Ancho del blockmap  
    uint32_t blockmap_height;   // Alto del blockmap  
    uint32_t blockmap_offset;   // Offset en archivo  
    uint32_t blockmap_cell_size; // Tamaño de celda  
} DMAP_HEADER_V2;  
  
// Vértice compartido (2D)  
typedef struct {  
    float x, y;  
} VERTEX; 
 
//TEXTURAS  
typedef struct {    
    char filename[256];      // Ruta al archivo de textura    
} TEXTURE_ENTRY_V2;
  
// Sector con soporte para rampas y movimiento dinámico  
typedef struct {  
    uint32_t id;  
    float floor_height;  
    float ceiling_height;  
      
    // Soporte para planos inclinados (rampas)  
    float floor_slope_x;  
    float floor_slope_y;  
    float ceiling_slope_x;  
    float ceiling_slope_y;  
      
    // Soporte para sectores dinámicos (ascensores)  
    uint8_t is_dynamic;  
    float target_floor_height;  
    float target_ceiling_height;  
    float move_speed;  
    uint8_t move_state; // 0=detenido, 1=subiendo, 2=bajando  
      
    // Texturas  
    uint32_t floor_texture_id;  
    uint32_t ceiling_texture_id;  
    uint32_t wall_texture_id;  
      
    // Geometría (índices a array global de vértices)  
    uint32_t num_vertices;  
    uint32_t *vertex_indices;  // AGREGADO: Array de índices a vértices globales  
      
    // Iluminación  
    uint8_t light_level; // 0-255  
      
    // Tipo de sector  
    uint8_t sector_type; // SECTOR_TYPE_*  
} SECTOR_V2;  
  
// Pared con índices de vértices y sidedefs  
typedef struct {  
    uint32_t sector1_id;  
    uint32_t sector2_id;  
      
    // Índices a array global de vértices  
    uint32_t vertex1_index;  
    uint32_t vertex2_index;  
      
    // Coordenadas directas (para compatibilidad legacy)  
    float x1, y1, x2, y2;  
      
    // Referencias a sidedefs  
    uint32_t sidedef1_id; // Cara frontal  
    uint32_t sidedef2_id; // Cara trasera  
      
    // Flags y acciones  
    uint8_t flags;        // WALL_FLAG_*  
    uint8_t action_type;  // ACTION_*  
} WALL_V2;  
  
// Atributos visuales por lado de pared  
typedef struct {  
    uint32_t texture_id;  
    float offset_x;  
    float offset_y;  
    uint8_t flags; // SIDEDEF_FLAG_*  
} SIDEDEF;  
  
// Entidad del mapa (jugador, enemigos, items)  
typedef struct {  
    float x, y, z;  
    float angle;  
    uint32_t type;  
    uint32_t flags;  
} THING;  
  
// Nodo del árbol BSP  
typedef struct {    
    float partition_x, partition_y;       // 8 bytes  
    float partition_dx, partition_dy;     // 8 bytes  
    int32_t front_child, back_child;      // 8 bytes ← AQUÍ ESTÁN LOS VALORES  
    float front_bbox_top, front_bbox_bottom, front_bbox_left, front_bbox_right;  // 16 bytes  
    float back_bbox_top, back_bbox_bottom, back_bbox_left, back_bbox_right;      // 16 bytes  
} BSP_NODE;  // Total: 56 bytes
  
// Subsector (hoja del árbol BSP)  
typedef struct {  
    uint32_t first_seg;  
    uint32_t num_segs;  
} SUBSECTOR;  
  
// Segmento de pared  
typedef struct {    
    uint32_t vertex1_index;  // Índice al array global de vértices  
    uint32_t vertex2_index;  // Índice al array global de vértices  
    uint32_t sector_id;       // Sector al que pertenece  
    uint32_t sidedef_id;      // Sidedef para textura  
    float offset;             // Offset de textura  
} SEG;
  
// AGREGADO: Celda del blockmap para colisiones  
typedef struct {  
    uint32_t num_walls;  
    uint32_t *wall_indices;  
} BLOCKMAP_CELL;  
  
// AGREGADO: Proyección de pared para renderizado  
typedef struct {  
    int x1_screen, x2_screen;  
    float z1, z2;  
    float wall_height;  
    int valid;  
} WALL_PROJECTION;  

#pragma pack(pop)  // ← DEBE ESTAR AQUÍ 
  
#endif // __LIBMOD_HEIGHTMAP_DMAP_H