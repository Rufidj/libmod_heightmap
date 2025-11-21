// dmap.c - Generador de mapas DMAP tile-based  
#include <stdio.h>  
#include <stdint.h>  
#include <string.h>  
#include <math.h>  
#include <stdlib.h>  
  
// IMPORTANTE: Incluir el header oficial para usar las mismas estructuras  
#include "libmod_heightmap_dmap.h"  
  
// ============================================================================  
// FUNCIÓN PRINCIPAL  
// ============================================================================  
  
int main() {  
    // Verificar tamaños de estructuras  
    printf("sizeof(DMAP_HEADER) = %zu bytes\n", sizeof(DMAP_HEADER));  
    printf("sizeof(TILE_CELL) = %zu bytes\n", sizeof(TILE_CELL));  
    printf("sizeof(TEXTURE_ENTRY) = %zu bytes\n", sizeof(TEXTURE_ENTRY));  
    printf("sizeof(THING) = %zu bytes\n\n", sizeof(THING));  
      
    FILE *f = fopen("test_level.dmap", "wb");  
    if (!f) {  
        fprintf(stderr, "Error: No se pudo crear archivo\n");  
        return 1;  
    }  
      
    // ========================================  
    // HEADER DMAP  
    // ========================================  
      
    DMAP_HEADER header;  
    memcpy(header.magic, "DMAP", 4);  
    header.grid_width = 32;  
    header.grid_height = 32;  
    header.tile_size = 64.0f;  
    header.num_textures = 3;  
    header.num_things = 2;  
      
    fwrite(&header, sizeof(DMAP_HEADER), 1, f);  
    printf("Header DMAP escrito (grid: %ux%u, tile_size: %.1f)\n",   
           header.grid_width, header.grid_height, header.tile_size);  
      
    // ========================================  
    // TEXTURAS (REFERENCIAS A ARCHIVOS)  
    // ========================================  
      
    TEXTURE_ENTRY textures[3];  
    memset(textures, 0, sizeof(textures)); // Inicializar todo a 0  
      
    strcpy(textures[0].filename, "textures/wall_bricks.png");  
    textures[0].graph_id = 0; // Se cargará en runtime  
      
    strcpy(textures[1].filename, "textures/floor_tiles.png");  
    textures[1].graph_id = 0;  
      
    strcpy(textures[2].filename, "textures/ceiling_solid.png");  
    textures[2].graph_id = 0;  
      
    for (int i = 0; i < 3; i++) {  
        fwrite(&textures[i], sizeof(TEXTURE_ENTRY), 1, f);  
        printf("Textura %d: %s\n", i, textures[i].filename);  
    }  
     printf("Texturas escritas al archivo:\n");  
for (int i = 0; i < 3; i++) {  
    printf("  Textura %d: '%s'\n", i, textures[i].filename);  
} 
    // ========================================  
    // GRID DE TILES (32x32 = 1024 celdas)  
    // ========================================  
      
    uint32_t total_cells = header.grid_width * header.grid_height;  
    TILE_CELL *grid = calloc(total_cells, sizeof(TILE_CELL));  
      
    if (!grid) {  
        fprintf(stderr, "Error: No se pudo asignar memoria para grid\n");  
        fclose(f);  
        return 1;  
    }  
      
    // Inicializar TODOS los tiles explícitamente  
    for (uint32_t y = 0; y < header.grid_height; y++) {  
        for (uint32_t x = 0; x < header.grid_width; x++) {  
            TILE_CELL *cell = &grid[y * header.grid_width + x];  
              
            // Valores por defecto (espacio vacío)  
            cell->wall_texture_id = 0;  
            cell->floor_texture_id = 2;  
            cell->ceiling_texture_id = 3;  
            cell->floor_height = 0.0f;  
            cell->ceiling_height = 100.0f;  
            cell->flags = 0x00;  
              
            // Paredes en bordes  
            if (x == 0 || x == 31 || y == 0 || y == 31) {  
                cell->wall_texture_id = 1;  
                cell->floor_texture_id = 0;  
                cell->ceiling_texture_id = 0;  
                cell->flags = 0x01; // Bloquea movimiento  
            }  
        }  
    }  
      
    // Crear pasillo en el centro (tiles 15-16 en X)  
    for (uint32_t y = 1; y < 31; y++) {  
        for (uint32_t x = 15; x <= 16; x++) {  
            TILE_CELL *cell = &grid[y * header.grid_width + x];  
            cell->wall_texture_id = 0;  
            cell->floor_texture_id = 2;  
            cell->ceiling_texture_id = 3;  
            cell->floor_height = 0.0f;  
            cell->ceiling_height = 80.0f; // Techo más bajo  
            cell->flags = 0x00;  
        }  
    }  
      
    // Crear rampa en la esquina superior derecha (tiles 25-30, 5-10)  
    for (uint32_t y = 5; y <= 10; y++) {  
        for (uint32_t x = 25; x <= 30; x++) {  
            TILE_CELL *cell = &grid[y * header.grid_width + x];  
            cell->wall_texture_id = 0;  
            cell->floor_texture_id = 2;  
            cell->ceiling_texture_id = 3;  
              
            // Rampa: altura aumenta con X  
            float ramp_progress = (float)(x - 25) / 5.0f;  
            cell->floor_height = ramp_progress * 50.0f; // De 0 a 50  
            cell->ceiling_height = 100.0f + ramp_progress * 50.0f;  
            cell->flags = 0x00;  
        }  
    }  
      
    // Crear habitación elevada conectada a la rampa (tiles 25-30, 11-20)  
    for (uint32_t y = 11; y <= 20; y++) {  
        for (uint32_t x = 25; x <= 30; x++) {  
            TILE_CELL *cell = &grid[y * header.grid_width + x];  
            cell->wall_texture_id = 0;  
            cell->floor_texture_id = 2;  
            cell->ceiling_texture_id = 3;  
            cell->floor_height = 50.0f; // Piso elevado  
            cell->ceiling_height = 150.0f;  
            cell->flags = 0x00;  
        }  
    }  
      
    // Añadir algunas columnas interiores (obstáculos)  
    uint32_t columns[][2] = {  
        {10, 10}, {10, 20}, {20, 10}, {20, 20}  
    };  
      
    for (int i = 0; i < 4; i++) {  
        uint32_t cx = columns[i][0];  
        uint32_t cy = columns[i][1];  
        TILE_CELL *cell = &grid[cy * header.grid_width + cx];  
        cell->wall_texture_id = 1;  
        cell->floor_height = 0.0f;  
        cell->ceiling_height = 100.0f;  
        cell->flags = 0x01;  
    }  
      
    fwrite(grid, sizeof(TILE_CELL), total_cells, f);  
    printf("Grid de tiles escrito: %u celdas (%zu bytes cada una)\n",   
           total_cells, sizeof(TILE_CELL));  
      
    free(grid);  
      
    // ========================================  
    // THINGS (2 entidades)  
    // ========================================  
      
    THING things[2];  
    memset(things, 0, sizeof(things)); // Inicializar todo a 0  
      
    // Thing 0: Jugador en el centro de la habitación  
    things[0].x = 16.0f * header.tile_size; // Centro X  
    things[0].y = 16.0f * header.tile_size; // Centro Y  
    things[0].z = 10.0f;  
    things[0].angle = 0.0f;  
    things[0].type = 1; // PLAYER  
    things[0].flags = 0;  
      
    // Thing 1: Enemigo en la habitación elevada  
    things[1].x = 27.0f * header.tile_size;  
    things[1].y = 15.0f * header.tile_size;  
    things[1].z = 60.0f; // Altura ajustada al piso elevado  
    things[1].angle = 3.14159f; // 180 grados  
    things[1].type = 2; // ENEMY  
    things[1].flags = 0;  
      
    fwrite(things, sizeof(THING), 2, f);  
    printf("Things escritos: 2\n");  
      
    // ========================================  
    // CIERRE Y RESUMEN  
    // ========================================  
      
    fclose(f);  
      
    // Calcular tamaño esperado del archivo  
    size_t expected_size = sizeof(DMAP_HEADER) +   
                          (3 * sizeof(TEXTURE_ENTRY)) +   
                          (1024 * sizeof(TILE_CELL)) +   
                          (2 * sizeof(THING));  
      
    printf("\n===========================================\n");  
    printf("Archivo test_level.dmap creado exitosamente\n");  
    printf("===========================================\n");  
    printf("Tamaño esperado: %zu bytes\n", expected_size);  
    printf("Formato: DMAP tile-based (sin versión)\n");  
    printf("Características:\n");  
    printf("  - Grid de %ux%u tiles\n", header.grid_width, header.grid_height);  
    printf("  - Tamaño de tile: %.1f unidades\n", header.tile_size);  
    printf("  - 3 texturas externas\n");  
    printf("  - 2 things (jugador, enemigo)\n");  
    printf("  - Raycasting DDA estilo Wolfenstein 3D\n\n");  
      
    printf("Elementos del mapa:\n");  
    printf("  - Habitación principal (1-30, 1-30)\n");  
    printf("  - Pasillo central con techo bajo (15-16, 1-30)\n");  
    printf("  - Rampa en esquina superior derecha (25-30, 5-10)\n");  
    printf("  - Habitación elevada (25-30, 11-20)\n");  
    printf("  - 4 columnas como obstáculos\n\n");  
      
    printf("Posición inicial recomendada:\n");  
    printf("  - Cámara: (%.1f, %.1f, 10.0)\n",   
           16.0f * header.tile_size, 16.0f * header.tile_size);  
    printf("  - Ángulo: 0 radianes (mirando al este)\n\n");  
      
    printf("Compilar con: gcc -I../modules/libmod_heightmap dmap.c -o dmap_gen -lm\n");  
    printf("Ejecutar: ./dmap_gen\n");  
      
    return 0;  
}