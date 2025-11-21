// dmap_v3_generator.c - Generador completo con habitaciones gigantes  
#include <stdio.h>    
#include <stdint.h>    
#include <string.h>    
#include <math.h>    
#include <stdlib.h>    
    
#pragma pack(push, 1)    
    
// ============================================================================    
// ESTRUCTURAS DMAP V3    
// ============================================================================    
    
typedef struct {    
    char magic[4];              // "DMAP"    
    uint32_t version;           // 3    
    uint32_t num_sectors;    
    uint32_t num_walls;    
    uint32_t num_textures;    
    uint32_t num_things;    
} DMAP_HEADER_V3;    
    
typedef struct {    
    float x, y;    
} VERTEX;    
    
typedef struct {    
    char filename[256];    
} TEXTURE_ENTRY_V2;    
    
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
    float target_ceiling_height;    
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
} SECTOR_V3;    
    
typedef struct {    
    uint32_t sector1_id;    
    uint32_t sector2_id;    
    float x1, y1, x2, y2;    
    uint32_t texture_id;    
    uint8_t flags;    
    uint8_t action_type;    
} WALL_V3;    
    
typedef struct {    
    float x, y, z;    
    float angle;    
    uint32_t type;    
    uint32_t flags;    
} THING;    
    
#pragma pack(pop)    
    
// ============================================================================    
// FUNCIÓN PRINCIPAL    
// ============================================================================    
    
int main() {    
    FILE *f = fopen("test_level_v3.dmap", "wb");    
    if (!f) {    
        fprintf(stderr, "Error: No se pudo crear archivo\n");    
        return 1;    
    }    
        
    // ========================================    
    // HEADER DMAP V3    
    // ========================================    
        
    DMAP_HEADER_V3 header;    
    memcpy(header.magic, "DMAP", 4);    
    header.version = 3;    
    header.num_sectors = 5;  // Habitación + pasillo + rampa + escaleras + ascensor    
    header.num_walls = 20;    
    header.num_textures = 3;    
    header.num_things = 2;    
        
    fwrite(&header, sizeof(DMAP_HEADER_V3), 1, f);    
    printf("Header DMAP v3 escrito\n");    
        
    // ========================================    
    // TEXTURAS (REFERENCIAS A ARCHIVOS)    
    // ========================================    
        
    TEXTURE_ENTRY_V2 textures[3];    
    strcpy(textures[0].filename, "textures/wall_bricks.png");    
    strcpy(textures[1].filename, "textures/floor_tiles.png");    
    strcpy(textures[2].filename, "textures/ceiling_solid.png");    
        
    for (int i = 0; i < 3; i++) {    
        fwrite(&textures[i], sizeof(TEXTURE_ENTRY_V2), 1, f);    
        printf("Textura %d: %s\n", i, textures[i].filename);    
    }
        // ========================================    
    // SECTORES V3 (con vértices embebidos)    
    // ========================================    
        
    // Sector 0: Habitación principal GIGANTE (0,0 a 1000,1000)    
    SECTOR_V3 sector0;    
    sector0.id = 0;    
    sector0.floor_height = 0.0f;    
    sector0.ceiling_height = 200.0f;  // Techo más alto para habitación gigante  
    sector0.floor_slope_x = 0.0f;    
    sector0.floor_slope_y = 0.0f;    
    sector0.is_dynamic = 0;    
    sector0.target_floor_height = 0.0f;    
    sector0.target_ceiling_height = 200.0f;    
    sector0.move_speed = 0.0f;    
    sector0.move_state = 0;    
    sector0.floor_texture_id = 1;    
    sector0.ceiling_texture_id = 2;    
    sector0.wall_texture_id = 0;    
    sector0.sector_type = 0;  // SECTOR_TYPE_NORMAL    
    sector0.light_level = 200;    
    sector0.num_vertices = 4;    
        
    fwrite(&sector0, sizeof(SECTOR_V3), 1, f);    
        
    VERTEX vertices0[4] = {    
        {0.0f, 0.0f},    
        {1000.0f, 0.0f},    
        {1000.0f, 1000.0f},    
        {0.0f, 1000.0f}    
    };    
    fwrite(vertices0, sizeof(VERTEX), 4, f);    
    printf("Sector 0: Habitación principal GIGANTE 1000x1000 (NORMAL)\n");    
        
    // Sector 1: Pasillo GIGANTE (1000,0 a 2000,1000)    
    SECTOR_V3 sector1;    
    sector1.id = 1;    
    sector1.floor_height = 0.0f;    
    sector1.ceiling_height = 160.0f;  // Techo más bajo que habitación principal  
    sector1.floor_slope_x = 0.0f;    
    sector1.floor_slope_y = 0.0f;    
    sector1.is_dynamic = 0;    
    sector1.target_floor_height = 0.0f;    
    sector1.target_ceiling_height = 160.0f;    
    sector1.move_speed = 0.0f;    
    sector1.move_state = 0;    
    sector1.floor_texture_id = 1;    
    sector1.ceiling_texture_id = 2;    
    sector1.wall_texture_id = 0;    
    sector1.sector_type = 0;  // SECTOR_TYPE_NORMAL    
    sector1.light_level = 150;    
    sector1.num_vertices = 4;    
        
    fwrite(&sector1, sizeof(SECTOR_V3), 1, f);    
        
    VERTEX vertices1[4] = {    
        {1000.0f, 0.0f},    
        {2000.0f, 0.0f},    
        {2000.0f, 1000.0f},    
        {1000.0f, 1000.0f}    
    };    
    fwrite(vertices1, sizeof(VERTEX), 4, f);    
    printf("Sector 1: Pasillo GIGANTE 1000x1000 (NORMAL)\n");    
        
    // Sector 2: Rampa GIGANTE con pendiente (2000,0 a 3000,1000)    
    SECTOR_V3 sector2;    
    sector2.id = 2;    
    sector2.floor_height = 0.0f;    
    sector2.ceiling_height = 240.0f;    
    sector2.floor_slope_x = 0.2f;  // Pendiente más suave para habitación gigante  
    sector2.floor_slope_y = 0.0f;    
    sector2.is_dynamic = 0;    
    sector2.target_floor_height = 0.0f;    
    sector2.target_ceiling_height = 240.0f;    
    sector2.move_speed = 0.0f;    
    sector2.move_state = 0;    
    sector2.floor_texture_id = 1;    
    sector2.ceiling_texture_id = 2;    
    sector2.wall_texture_id = 0;    
    sector2.sector_type = 3;  // SECTOR_TYPE_RAMP    
    sector2.light_level = 180;    
    sector2.num_vertices = 4;    
        
    fwrite(&sector2, sizeof(SECTOR_V3), 1, f);    
        
    VERTEX vertices2[4] = {    
        {2000.0f, 0.0f},    
        {3000.0f, 0.0f},    
        {3000.0f, 1000.0f},    
        {2000.0f, 1000.0f}    
    };    
    fwrite(vertices2, sizeof(VERTEX), 4, f);    
    printf("Sector 2: Rampa GIGANTE 1000x1000 con pendiente 0.2 (RAMP)\n");    
        
    // Sector 3: Escaleras GIGANTES (3000,0 a 4000,1000)    
    SECTOR_V3 sector3;    
    sector3.id = 3;    
    sector3.floor_height = 0.0f;    
    sector3.ceiling_height = 200.0f;    
    sector3.floor_slope_x = 0.3f;  // Pendiente ajustada para escaleras gigantes  
    sector3.floor_slope_y = 0.0f;    
    sector3.is_dynamic = 0;    
    sector3.target_floor_height = 0.0f;    
    sector3.target_ceiling_height = 200.0f;    
    sector3.move_speed = 0.0f;    
    sector3.move_state = 0;    
    sector3.floor_texture_id = 1;    
    sector3.ceiling_texture_id = 2;    
    sector3.wall_texture_id = 0;    
    sector3.sector_type = 1;  // SECTOR_TYPE_STAIRS    
    sector3.light_level = 170;    
    sector3.num_vertices = 4;    
        
    fwrite(&sector3, sizeof(SECTOR_V3), 1, f);    
        
    VERTEX vertices3[4] = {    
        {3000.0f, 0.0f},    
        {4000.0f, 0.0f},    
        {4000.0f, 1000.0f},    
        {3000.0f, 1000.0f}    
    };    
    fwrite(vertices3, sizeof(VERTEX), 4, f);    
    printf("Sector 3: Escaleras GIGANTES 1000x1000 con pendiente 0.3 (STAIRS)\n");    
        
    // Sector 4: Ascensor GIGANTE dinámico (4000,0 a 5000,1000)    
    SECTOR_V3 sector4;    
    sector4.id = 4;    
    sector4.floor_height = 0.0f;    
    sector4.ceiling_height = 200.0f;    
    sector4.floor_slope_x = 0.0f;    
    sector4.floor_slope_y = 0.0f;    
    sector4.is_dynamic = 1;  // ¡Dinámico!    
    sector4.target_floor_height = 100.0f;  // Sube 100 unidades  
    sector4.target_ceiling_height = 300.0f;    
    sector4.move_speed = 30.0f;  // Velocidad ajustada para distancias gigantes  
    sector4.move_state = 0;  // Inicialmente detenido    
    sector4.floor_texture_id = 1;    
    sector4.ceiling_texture_id = 2;    
    sector4.wall_texture_id = 0;    
    sector4.sector_type = 2;  // SECTOR_TYPE_ELEVATOR    
    sector4.light_level = 220;    
    sector4.num_vertices = 4;    
        
    fwrite(&sector4, sizeof(SECTOR_V3), 1, f);    
        
    VERTEX vertices4[4] = {    
        {4000.0f, 0.0f},    
        {5000.0f, 0.0f},    
        {5000.0f, 1000.0f},    
        {4000.0f, 1000.0f}    
    };    
    fwrite(vertices4, sizeof(VERTEX), 4, f);    
    printf("Sector 4: Ascensor GIGANTE 1000x1000 dinámico (ELEVATOR)\n");    
        
    printf("Sectores v3 escritos: %d\n", header.num_sectors);
     printf("Sectores v3 escritos: %d\n", header.num_sectors);  
        
    // ========================================    
    // PAREDES V3 (sin sidedefs)    
    // ========================================    
        
    WALL_V3 walls[20];    
    int wall_idx = 0;    
        
    // Paredes de habitación principal GIGANTE (4 paredes)    
    for (int i = 0; i < 4; i++) {    
        walls[wall_idx].sector1_id = 0;    
        walls[wall_idx].sector2_id = (i == 1) ? 1 : 0;  // Pared este (i=1) conecta con pasillo    
        walls[wall_idx].x1 = vertices0[i].x;    
        walls[wall_idx].y1 = vertices0[i].y;    
        walls[wall_idx].x2 = vertices0[(i + 1) % 4].x;    
        walls[wall_idx].y2 = vertices0[(i + 1) % 4].y;    
        walls[wall_idx].texture_id = 0;    
        walls[wall_idx].flags = 0x01;  // WALL_FLAG_BLOCKS_MOVEMENT    
        walls[wall_idx].action_type = 0;  // ACTION_NONE    
        wall_idx++;    
    }    
    printf("Paredes habitación GIGANTE: 4\n");    
        
    // Paredes de pasillo GIGANTE (4 paredes) - Pared oeste tiene puerta    
    for (int i = 0; i < 4; i++) {    
        walls[wall_idx].sector1_id = 1;    
        walls[wall_idx].sector2_id = (i == 0) ? 0 : ((i == 2) ? 2 : 1);    
        walls[wall_idx].x1 = vertices1[i].x;    
        walls[wall_idx].y1 = vertices1[i].y;    
        walls[wall_idx].x2 = vertices1[(i + 1) % 4].x;    
        walls[wall_idx].y2 = vertices1[(i + 1) % 4].y;    
        walls[wall_idx].texture_id = 0;    
        walls[wall_idx].flags = 0x01;    
        walls[wall_idx].action_type = (i == 0) ? 1 : 0;  // ACTION_OPEN_DOOR en pared oeste    
        wall_idx++;    
    }    
    printf("Paredes pasillo GIGANTE: 4 (pared 4 tiene puerta)\n");    
        
    // Paredes de rampa GIGANTE (4 paredes)    
    for (int i = 0; i < 4; i++) {    
        walls[wall_idx].sector1_id = 2;    
        walls[wall_idx].sector2_id = (i == 0) ? 1 : ((i == 2) ? 3 : 2);    
        walls[wall_idx].x1 = vertices2[i].x;    
        walls[wall_idx].y1 = vertices2[i].y;    
        walls[wall_idx].x2 = vertices2[(i + 1) % 4].x;    
        walls[wall_idx].y2 = vertices2[(i + 1) % 4].y;    
        walls[wall_idx].texture_id = 0;    
        walls[wall_idx].flags = 0x01;    
        walls[wall_idx].action_type = 0;    
        wall_idx++;    
    }    
    printf("Paredes rampa GIGANTE: 4\n");    
        
    // Paredes de escaleras GIGANTES (4 paredes)    
    for (int i = 0; i < 4; i++) {    
        walls[wall_idx].sector1_id = 3;    
        walls[wall_idx].sector2_id = (i == 0) ? 2 : ((i == 2) ? 4 : 3);    
        walls[wall_idx].x1 = vertices3[i].x;    
        walls[wall_idx].y1 = vertices3[i].y;    
        walls[wall_idx].x2 = vertices3[(i + 1) % 4].x;    
        walls[wall_idx].y2 = vertices3[(i + 1) % 4].y;    
        walls[wall_idx].texture_id = 0;    
        walls[wall_idx].flags = 0x01;    
        walls[wall_idx].action_type = 0;    
        wall_idx++;    
    }    
    printf("Paredes escaleras GIGANTES: 4\n");    
        
    // Paredes de ascensor GIGANTE (4 paredes) - Pared oeste activa ascensor    
    for (int i = 0; i < 4; i++) {    
        walls[wall_idx].sector1_id = 4;    
        walls[wall_idx].sector2_id = (i == 0) ? 3 : 4;    
        walls[wall_idx].x1 = vertices4[i].x;    
        walls[wall_idx].y1 = vertices4[i].y;    
        walls[wall_idx].x2 = vertices4[(i + 1) % 4].x;    
        walls[wall_idx].y2 = vertices4[(i + 1) % 4].y;    
        walls[wall_idx].texture_id = 0;    
        walls[wall_idx].flags = 0x01;    
        walls[wall_idx].action_type = (i == 0) ? 2 : 0;  // ACTION_ACTIVATE_ELEVATOR en pared oeste    
        wall_idx++;    
    }    
    printf("Paredes ascensor GIGANTE: 4 (pared 16 activa ascensor)\n");    
        
    fwrite(walls, sizeof(WALL_V3), header.num_walls, f);    
    printf("Total paredes v3 escritas: %d\n", header.num_walls);
        fwrite(walls, sizeof(WALL_V3), header.num_walls, f);    
    printf("Total paredes v3 escritas: %d\n", header.num_walls);    
        
    // ========================================    
    // THINGS (2 entidades)    
    // ========================================    
        
    THING things[2];    
        
    // Thing 0: Jugador en habitación principal GIGANTE (centro)  
    things[0].x = 500.0f;  // Centro de habitación 1000x1000  
    things[0].y = 500.0f;    
    things[0].z = 50.0f;   // Altura ajustada para techo de 200  
    things[0].angle = 0.0f;    
    things[0].type = 1;  // PLAYER    
    things[0].flags = 0;    
        
    // Thing 1: Enemigo en rampa GIGANTE  
    things[1].x = 2500.0f;  // Centro de rampa (2000-3000)  
    things[1].y = 500.0f;    
    things[1].z = 50.0f;    // Altura ajustada por rampa  
    things[1].angle = 3.14159f;  // 180 grados    
    things[1].type = 2;  // ENEMY    
    things[1].flags = 0;    
        
    fwrite(things, sizeof(THING), 2, f);    
    printf("Things escritos: 2\n");    
        
    // ========================================    
    // CIERRE Y RESUMEN    
    // ========================================    
        
    fclose(f);    
        
    printf("\n===========================================\n");    
    printf("Archivo test_level_v3.dmap creado exitosamente\n");    
    printf("===========================================\n");    
    printf("Formato: DMAP v3 (VPE simplificado)\n");    
    printf("Características:\n");    
    printf("  - 5 sectores GIGANTES conectados (1000x1000 cada uno)\n");    
    printf("  - 20 paredes (sin sidedefs)\n");    
    printf("  - 3 texturas externas\n");    
    printf("  - 2 things (jugador, enemigo)\n");    
    printf("  - Sin BSP, blockmap ni estructuras auxiliares\n\n");    
        
    printf("Elementos dinámicos:\n");    
    printf("  - Sector 0: Habitación GIGANTE 1000x1000 normal\n");    
    printf("  - Sector 1: Pasillo GIGANTE 1000x1000 con puerta (pared 4)\n");    
    printf("  - Sector 2: Rampa GIGANTE 1000x1000 con pendiente 0.2 en X\n");    
    printf("  - Sector 3: Escaleras GIGANTES 1000x1000 con pendiente 0.3 en X\n");    
    printf("  - Sector 4: Ascensor GIGANTE 1000x1000 dinámico (pared 16 lo activa)\n\n");    
        
    printf("Posición inicial recomendada:\n");    
    printf("  - Cámara: (500, 500, 50) - Centro de habitación principal\n");    
    printf("  - Ángulo: 0 radianes (mirando al este)\n\n");    
        
    printf("Compilar con: gcc dmap.c -o gen -lm\n");    
    printf("Ejecutar: ./gen\n");    
        
    return 0;    
}