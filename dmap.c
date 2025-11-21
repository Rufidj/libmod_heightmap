// dmap.c - Generador de mapas DMAP multi-nivel  
#include <stdio.h>  
#include <stdint.h>  
#include <string.h>  
#include <math.h>  
#include <stdlib.h>  
  
// IMPORTANTE: Incluir el header oficial para usar las mismas estructuras  
#include "libmod_heightmap_dmap.h"  
  
// Macro para acceder al grid 3D  
#define GRID_CELL(grid, width, height, x, y, level) \
    grid[(level) * (width) * (height) + (y) * (width) + (x)]  
  
int main() {  
    FILE *f = fopen("test_level.dmap", "wb");  
    if (!f) {  
        fprintf(stderr, "Error: No se pudo crear archivo\n");  
        return 1;  
    }  
      
    // ========================================  
    // HEADER DMAP MULTI-NIVEL  
    // ========================================  
      
    DMAP_HEADER header;  
    memcpy(header.magic, "DMAP", 4);  
    header.grid_width = 32;  
    header.grid_height = 32;  
    header.tile_size = 64.0f;  
    header.num_levels = 3;  // 3 pisos verticales  
    header.num_textures = 4;  
    header.num_thin_walls = 8;  
    header.num_things = 3;  
      
    fwrite(&header, sizeof(DMAP_HEADER), 1, f);  
    printf("Header DMAP multi-nivel escrito\n");  
    printf("  - Grid: %ux%ux%u tiles\n", header.grid_width, header.grid_height, header.num_levels);  
    printf("  - Tile size: %.1f\n", header.tile_size);  
      
    // ========================================  
    // TEXTURAS  
    // ========================================  
      
    TEXTURE_ENTRY textures[4];  
    strcpy(textures[0].filename, "/home/ruben/libmod_heightmap/textures/wall_bricks.png");  
    strcpy(textures[1].filename, "/home/ruben/libmod_heightmap/textures/floor_tiles.png");  
    strcpy(textures[2].filename, "/home/ruben/libmod_heightmap/textures/ceiling_solid.png");  
    strcpy(textures[3].filename, "/home/ruben/libmod_heightmap/textures/ramp_texture.png");  
      
    for (int i = 0; i < 4; i++) {  
        textures[i].graph_id = 0;  // Se carga en runtime  
        fwrite(&textures[i], sizeof(TEXTURE_ENTRY), 1, f);  
        printf("Textura %d: %s\n", i, textures[i].filename);  
    }  
      
    // ========================================  
    // GRID 3D DE TILES  
    // ========================================  
      
    uint32_t total_cells = header.grid_width * header.grid_height * header.num_levels;  
    TILE_CELL *grid = calloc(total_cells, sizeof(TILE_CELL));  
      
    if (!grid) {  
        fprintf(stderr, "Error: No se pudo asignar memoria para grid\n");  
        fclose(f);  
        return 1;  
    }  
      
    // Inicializar todo el grid como vacío  
    for (uint32_t i = 0; i < total_cells; i++) {  
        grid[i].wall_texture_id = 0;  
        grid[i].floor_texture_id = 2;  
        grid[i].ceiling_texture_id = 3;  
        grid[i].floor_height = 0.0f;  
        grid[i].ceiling_height = 100.0f;  
        grid[i].light_level = 200;  
        grid[i].flags = 0;  
    }  
      
    // ========================================  
    // NIVEL 0: HABITACIÓN PRINCIPAL CON COLUMNAS  
    // ========================================  
      
    printf("\nGenerando Nivel 0 (planta baja)...\n");  
      
    // Paredes exteriores de la habitación principal (10x10 tiles, centrada)  
    for (int x = 10; x < 22; x++) {  
        for (int y = 10; y < 22; y++) {  
            TILE_CELL *cell = &GRID_CELL(grid, header.grid_width, header.grid_height, x, y, 0);  
              
            // Bordes son paredes  
            if (x == 10 || x == 21 || y == 10 || y == 21) {  
                cell->wall_texture_id = 1;  
                cell->floor_texture_id = 2;  
                cell->ceiling_texture_id = 3;  
                cell->floor_height = 0.0f;  
                cell->ceiling_height = 100.0f;  
                cell->light_level = 200;  
            } else {  
                // Interior vacío  
                cell->wall_texture_id = 0;  
                cell->floor_texture_id = 2;  
                cell->ceiling_texture_id = 3;  
                cell->floor_height = 0.0f;  
                cell->ceiling_height = 100.0f;  
                cell->light_level = 220;  
            }  
        }  
    }  
      
    // Puerta en la pared este (x=21, y=15-16)  
    GRID_CELL(grid, header.grid_width, header.grid_height, 21, 15, 0).wall_texture_id = 0;  
    GRID_CELL(grid, header.grid_width, header.grid_height, 21, 15, 0).flags = TILE_FLAG_DOOR;  
    GRID_CELL(grid, header.grid_width, header.grid_height, 21, 16, 0).wall_texture_id = 0;  
    GRID_CELL(grid, header.grid_width, header.grid_height, 21, 16, 0).flags = TILE_FLAG_DOOR;  
      
    printf("  - Habitación principal: 12x12 tiles con puerta\n");  
      
    // ========================================  
    // NIVEL 0: PASILLO CONECTANDO A SEGUNDA HABITACIÓN  
    // ========================================  
      
    // Pasillo (22-27, 14-17)  
    for (int x = 22; x < 28; x++) {  
        for (int y = 14; y < 18; y++) {  
            TILE_CELL *cell = &GRID_CELL(grid, header.grid_width, header.grid_height, x, y, 0);  
              
            if (y == 14 || y == 17) {  
                cell->wall_texture_id = 1;  
            } else {  
                cell->wall_texture_id = 0;  
            }  
              
            cell->floor_texture_id = 2;  
            cell->ceiling_texture_id = 3;  
            cell->floor_height = 0.0f;  
            cell->ceiling_height = 80.0f;  // Techo más bajo  
            cell->light_level = 150;  
        }  
    }  
      
    printf("  - Pasillo: 6x4 tiles (techo bajo a 80 unidades)\n");  
      
    // ========================================  
    // NIVEL 0: SEGUNDA HABITACIÓN  
    // ========================================  
      
    // Segunda habitación (28-35, 12-20)  
    for (int x = 28; x < 36; x++) {  
        for (int y = 12; y < 20; y++) {  
            TILE_CELL *cell = &GRID_CELL(grid, header.grid_width, header.grid_height, x, y, 0);  
              
            if (x == 28 || x == 35 || y == 12 || y == 19) {  
                cell->wall_texture_id = 1;  
            } else {  
                cell->wall_texture_id = 0;  
            }  
              
            cell->floor_texture_id = 2;  
            cell->ceiling_texture_id = 3;  
            cell->floor_height = 0.0f;  
            cell->ceiling_height = 100.0f;  
            cell->light_level = 180;  
        }  
    }  
      
    // Conexión con pasillo  
    GRID_CELL(grid, header.grid_width, header.grid_height, 28, 15, 0).wall_texture_id = 0;  
    GRID_CELL(grid, header.grid_width, header.grid_height, 28, 16, 0).wall_texture_id = 0;  
      
    printf("  - Segunda habitación: 8x8 tiles\n");  
      
    // ========================================  
    // NIVEL 1: PLATAFORMA ELEVADA  
    // ========================================  
      
    printf("\nGenerando Nivel 1 (primer piso)...\n");  
      
    // Plataforma elevada sobre parte de la habitación principal  
    for (int x = 12; x < 20; x++) {  
        for (int y = 12; y < 20; y++) {  
            TILE_CELL *cell = &GRID_CELL(grid, header.grid_width, header.grid_height, x, y, 1);  
              
            if (x == 12 || x == 19 || y == 12 || y == 19) {  
                cell->wall_texture_id = 1;  
            } else {  
                cell->wall_texture_id = 0;  
            }  
              
            cell->floor_texture_id = 2;  
            cell->ceiling_texture_id = 3;  
            cell->floor_height = 150.0f;  // Elevado  
            cell->ceiling_height = 250.0f;  
            cell->light_level = 200;  
        }  
    }  
      
    printf("  - Plataforma elevada: 8x8 tiles a altura 150\n");  
      
    // ========================================  
    // NIVEL 2: TORRE  
    // ========================================  
      
    printf("\nGenerando Nivel 2 (segundo piso)...\n");  
      
    // Torre pequeña en el centro  
    for (int x = 14; x < 18; x++) {  
        for (int y = 14; y < 18; y++) {  
            TILE_CELL *cell = &GRID_CELL(grid, header.grid_width, header.grid_height, x, y, 2);  
              
            if (x == 14 || x == 17 || y == 14 || y == 17) {  
                cell->wall_texture_id = 1;  
            } else {  
                cell->wall_texture_id = 0;  
            }  
              
            cell->floor_texture_id = 2;  
            cell->ceiling_texture_id = 3;  
            cell->floor_height = 300.0f;  // Muy elevado  
            cell->ceiling_height = 400.0f;  
            cell->light_level = 180;  
        }  
    }  
      
    printf("  - Torre: 4x4 tiles a altura 300\n");  
      
    // Escribir grid completo al archivo  
    fwrite(grid, sizeof(TILE_CELL), total_cells, f);  
    printf("\nGrid 3D escrito: %u tiles totales\n", total_cells);  
      
    free(grid);  
      
    // ========================================  
    // THIN WALLS (COLUMNAS Y RAMPAS)  
    // ========================================  
      
    THIN_WALL thin_walls[8];  
      
    // Columnas en la habitación principal (nivel 0)  
    // Columna 1  
    thin_walls[0].x1 = 13.0f * header.tile_size;  
    thin_walls[0].y1 = 13.0f * header.tile_size;  
    thin_walls[0].x2 = 13.5f * header.tile_size;  
    thin_walls[0].y2 = 13.0f * header.tile_size;  
    thin_walls[0].z = 0.0f;  
    thin_walls[0].height = 100.0f;  
    thin_walls[0].texture_id = 1;  
    thin_walls[0].level = 0;  
    thin_walls[0].flags = 0;  
    thin_walls[0].slope = 0.0f;  
      
    // Columna 2  
    thin_walls[1].x1 = 18.0f * header.tile_size;  
    thin_walls[1].y1 = 13.0f * header.tile_size;  
    thin_walls[1].x2 = 18.5f * header.tile_size;  
    thin_walls[1].y2 = 13.0f * header.tile_size;  
    thin_walls[1].z = 0.0f;  
    thin_walls[1].height = 100.0f;  
    thin_walls[1].texture_id = 1;  
    thin_walls[1].level = 0;  
    thin_walls[1].flags = 0;  
    thin_walls[1].slope = 0.0f;  
      
    // Columna 3  
    thin_walls[2].x1 = 13.0f * header.tile_size;  
    thin_walls[2].y1 = 18.0f * header.tile_size;  
    thin_walls[2].x2 = 13.5f * header.tile_size;  
    thin_walls[2].y2 = 18.0f * header.tile_size;  
    thin_walls[2].z = 0.0f;  
    thin_walls[2].height = 100.0f;  
    thin_walls[2].texture_id = 1;  
    thin_walls[2].level = 0;  
    thin_walls[2].flags = 0;  
    thin_walls[2].slope = 0.0f;  
      
    // Columna 4  
    thin_walls[3].x1 = 18.0f * header.tile_size;  
    thin_walls[3].y1 = 18.0f * header.tile_size;  
    thin_walls[3].x2 = 18.5f * header.tile_size;  
    thin_walls[3].y2 = 18.0f * header.tile_size;  
    thin_walls[3].z = 0.0f;  
    thin_walls[3].height = 100.0f;  
    thin_walls[3].texture_id = 1;  
    thin_walls[3].level = 0;  
    thin_walls[3].flags = 0;  
    thin_walls[3].slope = 0.0f;  
          // Rampa diagonal conectando nivel 0 a nivel 1  
    thin_walls[4].x1 = 20.0f * header.tile_size;  
    thin_walls[4].y1 = 15.0f * header.tile_size;  
    thin_walls[4].x2 = 22.0f * header.tile_size;  
    thin_walls[4].y2 = 17.0f * header.tile_size;  
    thin_walls[4].z = 0.0f;  
    thin_walls[4].height = 150.0f;  // Altura que conecta nivel 0 con nivel 1  
    thin_walls[4].texture_id = 4;  
    thin_walls[4].level = 0;  
    thin_walls[4].flags = THIN_WALL_FLAG_RAMP;  
    thin_walls[4].slope = 1.0f;  // Pendiente de la rampa  
      
    // Pared delgada en nivel 1 (segundo piso)  
    thin_walls[5].x1 = 10.0f * header.tile_size;  
    thin_walls[5].y1 = 20.0f * header.tile_size;  
    thin_walls[5].x2 = 15.0f * header.tile_size;  
    thin_walls[5].y2 = 20.0f * header.tile_size;  
    thin_walls[5].z = 150.0f;  // Altura del nivel 1  
    thin_walls[5].height = 100.0f;  
    thin_walls[5].texture_id = 1;  
    thin_walls[5].level = 1;  
    thin_walls[5].flags = 0;  
    thin_walls[5].slope = 0.0f;  
      
    // Columna decorativa en nivel 1  
    thin_walls[6].x1 = 18.0f * header.tile_size;  
    thin_walls[6].y1 = 18.0f * header.tile_size;  
    thin_walls[6].x2 = 18.5f * header.tile_size;  
    thin_walls[6].y2 = 18.0f * header.tile_size;  
    thin_walls[6].z = 150.0f;  
    thin_walls[6].height = 100.0f;  
    thin_walls[6].texture_id = 1;  
    thin_walls[6].level = 1;  
    thin_walls[6].flags = 0;  
    thin_walls[6].slope = 0.0f;  
      
    // Escalera conectando nivel 1 a nivel 2  
    thin_walls[7].x1 = 25.0f * header.tile_size;  
    thin_walls[7].y1 = 20.0f * header.tile_size;  
    thin_walls[7].x2 = 27.0f * header.tile_size;  
    thin_walls[7].y2 = 22.0f * header.tile_size;  
    thin_walls[7].z = 150.0f;  
    thin_walls[7].height = 150.0f;  // Conecta nivel 1 con nivel 2  
    thin_walls[7].texture_id = 4;  
    thin_walls[7].level = 1;  
    thin_walls[7].flags = THIN_WALL_FLAG_RAMP;  
    thin_walls[7].slope = 1.5f;  // Pendiente más pronunciada para escalera  
      
    fwrite(thin_walls, sizeof(THIN_WALL), header.num_thin_walls, f);  
    printf("Thin walls escritos: %u\n", header.num_thin_walls);  
      
    // ========================================  
    // THINGS (ENTIDADES)  
    // ========================================  
      
    THING things[3];  
      
    // Thing 0: Jugador en nivel 0  
    things[0].x = 16.0f * header.tile_size;  
    things[0].y = 16.0f * header.tile_size;  
    things[0].z = 10.0f;  
    things[0].angle = 0.0f;  
    things[0].type = 1;  // PLAYER  
    things[0].flags = 0;  
      
    // Thing 1: Enemigo en nivel 1  
    things[1].x = 12.0f * header.tile_size;  
    things[1].y = 20.0f * header.tile_size;  
    things[1].z = 160.0f;  // Altura del nivel 1 + offset  
    things[1].angle = 3.14159f;  // 180 grados  
    things[1].type = 2;  // ENEMY  
    things[1].flags = 0;  
      
    // Thing 2: Item en nivel 2  
    things[2].x = 26.0f * header.tile_size;  
    things[2].y = 21.0f * header.tile_size;  
    things[2].z = 310.0f;  // Altura del nivel 2 + offset  
    things[2].angle = 0.0f;  
    things[2].type = 3;  // ITEM  
    things[2].flags = 0;  
      
    fwrite(things, sizeof(THING), 3, f);  
    printf("Things escritos: 3\n");  
      
    // ========================================  
    // CIERRE Y RESUMEN  
    // ========================================  
      
    fclose(f);  
      
    printf("\n===========================================\n");  
    printf("Archivo test_level.dmap creado exitosamente\n");  
    printf("===========================================\n");  
    printf("Formato: DMAP multi-nivel\n");  
    printf("Características:\n");  
    printf("  - Grid: %ux%ux%u tiles (width x height x levels)\n",   
           header.grid_width, header.grid_height, header.num_levels);  
    printf("  - Tile size: %.1f unidades\n", header.tile_size);  
    printf("  - %u texturas\n", header.num_textures);  
    printf("  - %u thin walls (columnas, rampas, escaleras)\n", header.num_thin_walls);  
    printf("  - %u things (jugador, enemigos, items)\n", header.num_things);  
    printf("\nNiveles verticales:\n");  
    printf("  - Nivel 0: Habitación principal con columnas (z=0)\n");  
    printf("  - Nivel 1: Segundo piso (z=150)\n");  
    printf("  - Nivel 2: Tercer piso (z=300)\n");  
    printf("\nConexiones:\n");  
    printf("  - Rampa diagonal: nivel 0 -> nivel 1 (thin_wall 4)\n");  
    printf("  - Escalera: nivel 1 -> nivel 2 (thin_wall 7)\n");  
    printf("\nPosición inicial recomendada:\n");  
    printf("  - Cámara: (%.1f, %.1f, 10.0)\n",   
           16.0f * header.tile_size, 16.0f * header.tile_size);  
    printf("  - Ángulo: 0 radianes (mirando al este)\n\n");  
      
    printf("Compilar con: gcc dmap.c -o dmap_gen -lm\n");  
    printf("Ejecutar: ./dmap_gen\n");  
      
    return 0;  
}