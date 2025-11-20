// dmap_v2_generator.c - Generador de mapas DMAP v2 con texturas empaquetadas  
#include <stdio.h>  
#include <stdint.h>  
#include <string.h>  
#include <math.h>  
#include <stdlib.h>  
  
// Forzar empaquetado sin padding para compatibilidad binaria  
#pragma pack(push, 1)  
  
// ============================================================================  
// ESTRUCTURAS DMAP V2  
// ============================================================================  
  
typedef struct {  
    char magic[4];              // "DMAP"  
    uint32_t version;           // 2  
    uint32_t num_sectors;  
    uint32_t num_walls;  
    uint32_t num_textures;  
    uint32_t num_vertices;  
    uint32_t num_sidedefs;  
    uint32_t num_things;  
    uint32_t num_bsp_nodes;  
    uint32_t num_subsectors;  
    uint32_t num_segs;  
    uint32_t blockmap_width;  
    uint32_t blockmap_height;  
    uint32_t blockmap_offset;  
    uint32_t blockmap_cell_size;  
} DMAP_HEADER_V2;  
  
typedef struct {  
    float x, y;  
} VERTEX;  
  
typedef struct {  
    uint32_t id;  
    float floor_height;  
    float ceiling_height;  
    float floor_slope_x;  
    float floor_slope_y;  
    float ceiling_slope_x;  
    float ceiling_slope_y;  
    uint8_t is_dynamic;  
    float target_floor_height;  
    float target_ceiling_height;  
    float move_speed;  
    uint8_t move_state;  
    uint8_t sector_type;  
    uint32_t floor_texture_id;  
    uint32_t ceiling_texture_id;  
    uint32_t wall_texture_id;  
    uint32_t num_vertices;  
    uint8_t light_level;  
} DMAP_SECTOR_V2;  
  
typedef struct {  
    uint32_t sector1_id;  
    uint32_t sector2_id;  
    uint32_t vertex1_index;  
    uint32_t vertex2_index;  
    float x1, y1, x2, y2;  
    uint32_t sidedef1_id;  
    uint32_t sidedef2_id;  
    uint8_t flags;  
    uint8_t action_type;  
} DMAP_WALL_V2;  
  
typedef struct {  
    uint32_t texture_id;  
    float offset_x;  
    float offset_y;  
    uint8_t flags;  
} DMAP_SIDEDEF;  
  
typedef struct {  
    float x, y, z;  
    float angle;  
    uint32_t type;  
    uint32_t flags;  
} DMAP_THING;  
  
typedef struct {  
    char name[64];  
    uint32_t width;  
    uint32_t height;  
    uint32_t data_size;  
    uint32_t format;  // 0=RGB, 1=RGBA  
} TEXTURE_ENTRY_V2;  
  
typedef struct {  
    float partition_x, partition_y;  
    float partition_dx, partition_dy;  
    int32_t front_child, back_child;  
    float front_bbox_top, front_bbox_bottom, front_bbox_left, front_bbox_right;  
    float back_bbox_top, back_bbox_bottom, back_bbox_left, back_bbox_right;  
} BSP_NODE;  
  
typedef struct {  
    uint32_t first_seg;  
    uint32_t num_segs;  
} SUBSECTOR;  
  
typedef struct {  
    uint32_t vertex1_index, vertex2_index;  
    uint32_t sector_id, sidedef_id;  
    float offset;  
} SEG;  
  
#pragma pack(pop)  
  
// ============================================================================  
// FUNCIÓN PARA GENERAR TEXTURAS PROCEDURALES  
// ============================================================================  
  
uint8_t* generate_procedural_texture(int width, int height, int type, size_t *out_size) {  
    int channels = 4; // RGBA  
    size_t size = width * height * channels;  
    uint8_t *data = (uint8_t*)malloc(size);  
      
    if (!data) return NULL;  
      
    for (int y = 0; y < height; y++) {  
        for (int x = 0; x < width; x++) {  
            int idx = (y * width + x) * channels;  
              
            switch (type) {  
                case 0: // Textura de ladrillos (pared)  
                {  
                    int brick_w = 16;  
                    int brick_h = 8;  
                    int bx = x % brick_w;  
                    int by = y % brick_h;  
                    int is_mortar = (bx == 0 || by == 0);  
                      
                    if (is_mortar) {  
                        data[idx + 0] = 100; // R  
                        data[idx + 1] = 100; // G  
                        data[idx + 2] = 100; // B  
                    } else {  
                        data[idx + 0] = 150 + (x * y) % 50; // R  
                        data[idx + 1] = 80 + (x * y) % 30;  // G  
                        data[idx + 2] = 60 + (x * y) % 20;  // B  
                    }  
                    data[idx + 3] = 255; // A  
                    break;  
                }  
                  
                case 1: // Textura de baldosas (suelo)  
                {  
                    int tile_size = 16;  
                    int tx = (x / tile_size) % 2;  
                    int ty = (y / tile_size) % 2;  
                    int is_dark = (tx + ty) % 2;  
                      
                    if (is_dark) {  
                        data[idx + 0] = 80;  // R  
                        data[idx + 1] = 80;  // G  
                        data[idx + 2] = 80;  // B  
                    } else {  
                        data[idx + 0] = 120; // R  
                        data[idx + 1] = 120; // G  
                        data[idx + 2] = 120; // B  
                    }  
                    data[idx + 3] = 255; // A  
                    break;  
                }  
                  
                case 2: // Textura sólida (techo)  
                {  
                    data[idx + 0] = 60;  // R  
                    data[idx + 1] = 60;  // G  
                    data[idx + 2] = 80;  // B  
                    data[idx + 3] = 255; // A  
                    break;  
                }  
                  
                default:  
                    data[idx + 0] = 255;  
                    data[idx + 1] = 0;  
                    data[idx + 2] = 255;  
                    data[idx + 3] = 255;  
                    break;  
            }  
        }  
    }  
      
    *out_size = size;  
    return data;  
}  
  
// ============================================================================  
// FUNCIÓN PRINCIPAL  
// ============================================================================  
  
int main() {  
    FILE *f = fopen("test_level_v2.dmap", "wb");  
    if (!f) {  
        fprintf(stderr, "Error: No se pudo crear archivo\n");  
        return 1;  
    }  
      
    // ========================================  
    // HEADER DMAP V2  
    // ========================================  
      
    DMAP_HEADER_V2 header;  
    memcpy(header.magic, "DMAP", 4);  
    header.version = 2;  
    header.num_sectors = 3;  
    header.num_walls = 12;  
    header.num_textures = 3;  
    header.num_vertices = 12;  
    header.num_sidedefs = 24;  
    header.num_things = 2;  
    header.num_bsp_nodes = 1;  
    header.num_subsectors = 2;  
    header.num_segs = 12;  
    header.blockmap_width = 10;  
    header.blockmap_height = 10;  
    header.blockmap_cell_size = 50;  
    header.blockmap_offset = 0;  
      
    fwrite(&header, sizeof(DMAP_HEADER_V2), 1, f);  
    printf("Header DMAP v2 escrito\n");  
      
    // ========================================  
    // TEXTURAS EMPAQUETADAS  
    // ========================================  
      
    TEXTURE_ENTRY_V2 textures[3];  
    uint8_t *texture_data[3];  
    size_t texture_sizes[3];  
      
    // Textura 0: Pared (ladrillos)  
    strcpy(textures[0].name, "wall_bricks");  
    textures[0].width = 64;  
    textures[0].height = 64;  
    textures[0].format = 1; // RGBA  
    texture_data[0] = generate_procedural_texture(64, 64, 0, &texture_sizes[0]);  
    textures[0].data_size = texture_sizes[0];  
      
    // Textura 1: Suelo (baldosas)  
    strcpy(textures[1].name, "floor_tiles");  
    textures[1].width = 64;  
    textures[1].height = 64;  
    textures[1].format = 1; // RGBA  
    texture_data[1] = generate_procedural_texture(64, 64, 1, &texture_sizes[1]);  
    textures[1].data_size = texture_sizes[1];  
      
    // Textura 2: Techo (color sólido)  
    strcpy(textures[2].name, "ceiling_solid");  
    textures[2].width = 64;  
    textures[2].height = 64;  
    textures[2].format = 1; // RGBA  
    texture_data[2] = generate_procedural_texture(64, 64, 2, &texture_sizes[2]);  
    textures[2].data_size = texture_sizes[2];  
      
    // Escribir headers de texturas  
    for (int i = 0; i < 3; i++) {  
        fwrite(&textures[i], sizeof(TEXTURE_ENTRY_V2), 1, f);  
    }  
      
    // Escribir datos de píxeles de cada textura  
    for (int i = 0; i < 3; i++) {  
        fwrite(texture_data[i], texture_sizes[i], 1, f);  
        printf("Textura %d: %s (%ux%u) - %zu bytes escritos\n",  
               i, textures[i].name, textures[i].width, textures[i].height, texture_sizes[i]);  
    }
      // ========================================  
    // SECTORES (3 sectores)  
    // ========================================  
      
    DMAP_SECTOR_V2 sectors[3];  
      
    // Sector 0: Habitación principal con ascensor  
    sectors[0].id = 0;  
    sectors[0].floor_height = 0.0f;  
    sectors[0].ceiling_height = 100.0f;  
    sectors[0].floor_slope_x = 0.0f;  
    sectors[0].floor_slope_y = 0.0f;  
    sectors[0].ceiling_slope_x = 0.0f;  
    sectors[0].ceiling_slope_y = 0.0f;  
    sectors[0].is_dynamic = 1;  // Ascensor dinámico  
    sectors[0].target_floor_height = 50.0f;  
    sectors[0].target_ceiling_height = 150.0f;  
    sectors[0].move_speed = 20.0f;  
    sectors[0].move_state = 0;  // Inicialmente detenido  
    sectors[0].sector_type = 2; // SECTOR_TYPE_ELEVATOR  
    sectors[0].floor_texture_id = 1;  
    sectors[0].ceiling_texture_id = 2;  
    sectors[0].wall_texture_id = 0;  
    sectors[0].num_vertices = 4;  
    sectors[0].light_level = 200;  
      
    // Sector 1: Pasillo  
    sectors[1].id = 1;  
    sectors[1].floor_height = 0.0f;  
    sectors[1].ceiling_height = 80.0f;  
    sectors[1].floor_slope_x = 0.0f;  
    sectors[1].floor_slope_y = 0.0f;  
    sectors[1].ceiling_slope_x = 0.0f;  
    sectors[1].ceiling_slope_y = 0.0f;  
    sectors[1].is_dynamic = 0;  
    sectors[1].target_floor_height = 0.0f;  
    sectors[1].target_ceiling_height = 80.0f;  
    sectors[1].move_speed = 0.0f;  
    sectors[1].move_state = 0;  
    sectors[1].sector_type = 0; // SECTOR_TYPE_NORMAL  
    sectors[1].floor_texture_id = 1;  
    sectors[1].ceiling_texture_id = 2;  
    sectors[1].wall_texture_id = 0;  
    sectors[1].num_vertices = 4;  
    sectors[1].light_level = 150;  
      
    // Sector 2: Rampa con pendiente  
    sectors[2].id = 2;  
    sectors[2].floor_height = 0.0f;  
    sectors[2].ceiling_height = 120.0f;  
    sectors[2].floor_slope_x = 0.5f;  // Rampa en X  
    sectors[2].floor_slope_y = 0.0f;  
    sectors[2].ceiling_slope_x = 0.0f;  
    sectors[2].ceiling_slope_y = 0.0f;  
    sectors[2].is_dynamic = 0;  
    sectors[2].target_floor_height = 0.0f;  
    sectors[2].target_ceiling_height = 120.0f;  
    sectors[2].move_speed = 0.0f;  
    sectors[2].move_state = 0;  
    sectors[2].sector_type = 3; // SECTOR_TYPE_RAMP  
    sectors[2].floor_texture_id = 1;  
    sectors[2].ceiling_texture_id = 2;  
    sectors[2].wall_texture_id = 0;  
    sectors[2].num_vertices = 4;  
    sectors[2].light_level = 180;  
      
     fwrite(sectors, sizeof(DMAP_SECTOR_V2), 3, f);    
    printf("Sectores v2 escritos: %d\n", header.num_sectors);    
        
    // Escribir índices de vértices para cada sector    
    uint32_t sector0_indices[4] = {0, 1, 2, 3};    
    uint32_t sector1_indices[4] = {4, 5, 6, 7};    
    uint32_t sector2_indices[4] = {8, 9, 10, 11};    
        
    fwrite(sector0_indices, sizeof(uint32_t), 4, f);    
    fwrite(sector1_indices, sizeof(uint32_t), 4, f);    
    fwrite(sector2_indices, sizeof(uint32_t), 4, f);    
    printf("Índices de vértices escritos\n");  
      
    // ========================================    
    // VÉRTICES COMPARTIDOS (12 vértices total)  
    // ========================================    
      
    VERTEX vertices[12];  // ← DECLARAR AQUÍ  
      
    // Habitación principal (0-3)  
    vertices[0].x = 0.0f;    vertices[0].y = 0.0f;  
    vertices[1].x = 200.0f;  vertices[1].y = 0.0f;  
    vertices[2].x = 200.0f;  vertices[2].y = 200.0f;  
    vertices[3].x = 0.0f;    vertices[3].y = 200.0f;  
      
    // Pasillo (4-7)  
    vertices[4].x = 200.0f;  vertices[4].y = 0.0f;  
    vertices[5].x = 300.0f;  vertices[5].y = 0.0f;  
    vertices[6].x = 300.0f;  vertices[6].y = 200.0f;  
    vertices[7].x = 200.0f;  vertices[7].y = 200.0f;  
      
    // Rampa (8-11)  
    vertices[8].x = 300.0f;   vertices[8].y = 0.0f;  
    vertices[9].x = 500.0f;   vertices[9].y = 0.0f;  
    vertices[10].x = 500.0f;  vertices[10].y = 200.0f;  
    vertices[11].x = 300.0f;  vertices[11].y = 200.0f;  
      
    fwrite(vertices, sizeof(VERTEX), 12, f);    
    printf("Vértices escritos: %d\n", header.num_vertices);
      
      // ========================================  
    // PAREDES (12 paredes total)  
    // ========================================  
      
    DMAP_WALL_V2 walls[12];  
    int wall_idx = 0;  
      
    // Paredes de habitación principal (4 paredes)  
    for (int i = 0; i < 4; i++) {  
        walls[wall_idx].sector1_id = 0;  
        walls[wall_idx].sector2_id = (i == 1) ? 1 : 0; // Pared este conecta con pasillo  
        walls[wall_idx].vertex1_index = i;  
        walls[wall_idx].vertex2_index = (i + 1) % 4;  
        walls[wall_idx].x1 = vertices[i].x;  
        walls[wall_idx].y1 = vertices[i].y;  
        walls[wall_idx].x2 = vertices[(i + 1) % 4].x;  
        walls[wall_idx].y2 = vertices[(i + 1) % 4].y;  
        walls[wall_idx].sidedef1_id = wall_idx * 2;  
        walls[wall_idx].sidedef2_id = wall_idx * 2 + 1;  
        walls[wall_idx].flags = 0x01; // WALL_FLAG_BLOCKS_MOVEMENT  
        walls[wall_idx].action_type = 0; // ACTION_NONE  
        wall_idx++;  
    }  
      
    // Paredes de pasillo (4 paredes)  
    for (int i = 0; i < 4; i++) {  
        walls[wall_idx].sector1_id = 1;  
        walls[wall_idx].sector2_id = (i == 2) ? 2 : 1; // Pared este conecta con rampa  
        walls[wall_idx].vertex1_index = 4 + i;  
        walls[wall_idx].vertex2_index = 4 + ((i + 1) % 4);  
        walls[wall_idx].x1 = vertices[4 + i].x;  
        walls[wall_idx].y1 = vertices[4 + i].y;  
        walls[wall_idx].x2 = vertices[4 + ((i + 1) % 4)].x;  
        walls[wall_idx].y2 = vertices[4 + ((i + 1) % 4)].y;  
        walls[wall_idx].sidedef1_id = wall_idx * 2;  
        walls[wall_idx].sidedef2_id = wall_idx * 2 + 1;  
        walls[wall_idx].flags = 0x01;  
        walls[wall_idx].action_type = 0;  
        wall_idx++;  
    }  
      
    // Paredes de rampa (4 paredes)  
    for (int i = 0; i < 4; i++) {  
        walls[wall_idx].sector1_id = 2;  
        walls[wall_idx].sector2_id = (i == 0) ? 1 : 2; // Primera pared conecta con pasillo  
        walls[wall_idx].vertex1_index = 8 + i;  
        walls[wall_idx].vertex2_index = 8 + ((i + 1) % 4);  
        walls[wall_idx].x1 = vertices[8 + i].x;  
        walls[wall_idx].y1 = vertices[8 + i].y;  
        walls[wall_idx].x2 = vertices[8 + ((i + 1) % 4)].x;  
        walls[wall_idx].y2 = vertices[8 + ((i + 1) % 4)].y;  
        walls[wall_idx].sidedef1_id = wall_idx * 2;  
        walls[wall_idx].sidedef2_id = wall_idx * 2 + 1;  
        walls[wall_idx].flags = 0x01;  
        walls[wall_idx].action_type = 0;  
        wall_idx++;  
    }  
      
    fwrite(walls, sizeof(DMAP_WALL_V2), 12, f);  
    printf("Paredes escritas: %d\n", header.num_walls);  
      
    // ========================================  
    // SIDEDEFS (24 sidedefs: 2 por pared)  
    // ========================================  
      
    DMAP_SIDEDEF sidedefs[24];  
    for (int i = 0; i < 24; i++) {  
        sidedefs[i].texture_id = i / 8; // Distribuir texturas: 0-7 tex0, 8-15 tex1, 16-23 tex2  
        sidedefs[i].offset_x = 0.0f;  
        sidedefs[i].offset_y = 0.0f;  
        sidedefs[i].flags = 0;  
    }  
      
    fwrite(sidedefs, sizeof(DMAP_SIDEDEF), 24, f);  
    printf("Sidedefs escritos: %d\n", header.num_sidedefs);  
      
    // ========================================  
    // THINGS (2 entidades)  
    // ========================================  
      
    DMAP_THING things[2];  
      
    // Thing 0: Jugador  
    things[0].x = 100.0f;  
    things[0].y = 100.0f;  
    things[0].z = 75.0f;  
    things[0].angle = 0.0f;  
    things[0].type = 1; // PLAYER  
    things[0].flags = 0;  
      
    // Thing 1: Enemigo  
    things[1].x = 300.0f;  
    things[1].y = 100.0f;  
    things[1].z = 50.0f;  
    things[1].angle = 3.14159f; // 180 grados  
    things[1].type = 2; // ENEMY  
    things[1].flags = 0;  
      
    fwrite(things, sizeof(DMAP_THING), 2, f);  
    printf("Things escritos: %d\n", header.num_things);  
      
    // ========================================  
    // BSP NODES (1 nodo raíz simplificado)  
    // ========================================  
      
    BSP_NODE root_node;  
    root_node.partition_x = 200.0f;  
    root_node.partition_y = 0.0f;  
    root_node.partition_dx = 0.0f;  
    root_node.partition_dy = 200.0f;  
    root_node.front_child = -1; // Subsector 0 (habitación)  
    root_node.back_child = -2;  // Subsector 1 (pasillo + rampa)  
      
    root_node.front_bbox_top = 200.0f;  
    root_node.front_bbox_bottom = 0.0f;  
    root_node.front_bbox_left = 0.0f;  
    root_node.front_bbox_right = 200.0f;  
      
    root_node.back_bbox_top = 200.0f;  
    root_node.back_bbox_bottom = 0.0f;  
    root_node.back_bbox_left = 200.0f;  
    root_node.back_bbox_right = 500.0f;  
      
    fwrite(&root_node, sizeof(BSP_NODE), 1, f);  
    printf("BSP nodes escritos: %d\n", header.num_bsp_nodes);  
      
    // ========================================  
    // SUBSECTORS (2 subsectores)  
    // ========================================  
      
    SUBSECTOR subsectors[2];  
    subsectors[0].first_seg = 0;  
    subsectors[0].num_segs = 4; // Habitación  
    subsectors[1].first_seg = 4;  
    subsectors[1].num_segs = 8; // Pasillo + rampa  
      
    fwrite(subsectors, sizeof(SUBSECTOR), 2, f);  
    printf("Subsectores escritos: 2\n");  
      
    // ========================================  
    // SEGS (12 segmentos: 1 por pared)  
    // ========================================  
      
    SEG segs[12];  
    for (int i = 0; i < 12; i++) {  
        segs[i].vertex1_index = walls[i].vertex1_index;  
        segs[i].vertex2_index = walls[i].vertex2_index;  
        segs[i].sector_id = walls[i].sector1_id;  
        segs[i].sidedef_id = walls[i].sidedef1_id;  
        segs[i].offset = 0.0f;  
    }  
      
    fwrite(segs, sizeof(SEG), 12, f);  
    printf("Segs escritos: 12\n");  
      
    // ========================================  
    // BLOCKMAP (10x10 grid)  
    // ========================================  
      
    uint16_t blockmap[100];  
    for (int i = 0; i < 100; i++) {  
        // Asignar paredes por región simplificada  
        if (i < 40) blockmap[i] = 0x000F; // Habitación: paredes 0-3  
        else if (i < 60) blockmap[i] = 0x00F0; // Pasillo: paredes 4-7  
        else blockmap[i] = 0x0F00; // Rampa: paredes 8-11  
    }  
      
    fwrite(blockmap, sizeof(uint16_t), 100, f);  
    printf("Blockmap escrito: 10x10 celdas\n");  
      
    // ========================================  
    // CIERRE Y RESUMEN  
    // ========================================  
      
    fclose(f);  
      
    // Liberar memoria de texturas  
    for (int i = 0; i < 3; i++) {  
        free(texture_data[i]);  
    }  
      
    printf("\n===========================================\n");  
    printf("Archivo test_level_v2.dmap creado exitosamente\n");  
    printf("===========================================\n");  
    printf("Formato: DMAP v2\n");  
    printf("- 3 sectores (habitación, pasillo, rampa)\n");  
    printf("- 12 vértices compartidos\n");  
    printf("- 12 paredes con sidedefs\n");  
    printf("- 24 sidedefs (2 por pared)\n");  
    printf("- 2 things (jugador, enemigo)\n");  
    printf("- 1 nodo BSP raíz\n");  
    printf("- 2 subsectores\n");  
    printf("- 12 segs\n");  
    printf("- Blockmap 10x10\n\n");  
    printf("Características especiales:\n");  
    printf("- Sector 0: Ascensor dinámico (altura inicial 0)\n");  
    printf("- Sector 2: Rampa con pendiente floor_slope_x=0.5\n\n");  
    printf("Posiciona la cámara en (100, 100, 75) para empezar\n");  
    printf("en la habitación principal\n");  
      
    return 0;  
}