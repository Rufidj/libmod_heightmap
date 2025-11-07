// test_dmap_generator_with_walls.c  
#include <stdio.h>  
#include <stdint.h>  
#include <string.h>  
  
typedef struct {  
    char magic[4];  
    uint32_t version;  
    uint32_t num_sectors;  
    uint32_t num_walls;  
    uint32_t num_textures;  
} DMAP_HEADER;  
  
typedef struct {  
    uint32_t id;  
    float floor_height;  
    float ceiling_height;  
    uint32_t floor_texture_id;  
    uint32_t ceiling_texture_id;  
    uint32_t wall_texture_id;  
    uint32_t num_vertices;  
} DMAP_SECTOR;  
  
typedef struct {  
    uint32_t sector1_id;  
    uint32_t sector2_id;  
    uint32_t texture_id;  
    float x1, y1, x2, y2;  
} DMAP_WALL;  
  
int main() {  
    FILE *f = fopen("test_level.dmap", "wb");  
    if (!f) {  
        fprintf(stderr, "Error: No se pudo crear archivo\n");  
        return 1;  
    }  
      
    // Header - 1 sector, 4 paredes, 1 textura  
    DMAP_HEADER header;  
    memcpy(header.magic, "DMAP", 4);  
    header.version = 1;  
    header.num_sectors = 1;  
    header.num_walls = 4;  // 4 paredes formando habitación  
    header.num_textures = 1;  
    fwrite(&header, sizeof(DMAP_HEADER), 1, f);  
      
    // Tabla de texturas  
    char texture_name[256] = "floor.png";  
    fwrite(texture_name, 256, 1, f);  
      
    // Sector: habitación 200x200 con altura de techo 150  
    DMAP_SECTOR sector;  
    sector.id = 0;  
    sector.floor_height = 0.0f;  
    sector.ceiling_height = 150.0f;  
    sector.floor_texture_id = 0;  
    sector.ceiling_texture_id = 0;  
    sector.wall_texture_id = 0;  
    sector.num_vertices = 4;  
    fwrite(&sector, sizeof(DMAP_SECTOR), 1, f);  
      
    // Vértices del sector (cuadrado 200x200)  
    float vertices[] = {  
        0.0f, 0.0f,      // Esquina inferior izquierda  
        200.0f, 0.0f,    // Esquina inferior derecha  
        200.0f, 200.0f,  // Esquina superior derecha  
        0.0f, 200.0f     // Esquina superior izquierda  
    };  
    fwrite(vertices, sizeof(float), 8, f);  
      
    // Pared 1: Pared sur (abajo)  
    DMAP_WALL wall1;  
    wall1.sector1_id = 0;  
    wall1.sector2_id = 0;  // Mismo sector = pared sólida  
    wall1.texture_id = 0;  
    wall1.x1 = 0.0f;  
    wall1.y1 = 0.0f;  
    wall1.x2 = 200.0f;  
    wall1.y2 = 0.0f;  
    fwrite(&wall1, sizeof(DMAP_WALL), 1, f);  
      
    // Pared 2: Pared este (derecha)  
    DMAP_WALL wall2;  
    wall2.sector1_id = 0;  
    wall2.sector2_id = 0;  
    wall2.texture_id = 0;  
    wall2.x1 = 200.0f;  
    wall2.y1 = 0.0f;  
    wall2.x2 = 200.0f;  
    wall2.y2 = 200.0f;  
    fwrite(&wall2, sizeof(DMAP_WALL), 1, f);  
      
    // Pared 3: Pared norte (arriba)  
    DMAP_WALL wall3;  
    wall3.sector1_id = 0;  
    wall3.sector2_id = 0;  
    wall3.texture_id = 0;  
    wall3.x1 = 200.0f;  
    wall3.y1 = 200.0f;  
    wall3.x2 = 0.0f;  
    wall3.y2 = 200.0f;  
    fwrite(&wall3, sizeof(DMAP_WALL), 1, f);  
      
    // Pared 4: Pared oeste (izquierda)  
    DMAP_WALL wall4;  
    wall4.sector1_id = 0;  
    wall4.sector2_id = 0;  
    wall4.texture_id = 0;  
    wall4.x1 = 0.0f;  
    wall4.y1 = 200.0f;  
    wall4.x2 = 0.0f;  
    wall4.y2 = 0.0f;  
    fwrite(&wall4, sizeof(DMAP_WALL), 1, f);  
      
    fclose(f);  
    printf("Archivo test_level.dmap creado exitosamente\n");  
    printf("Habitación: 200x200 unidades\n");  
    printf("Altura techo: 150 unidades\n");  
    printf("4 paredes definidas\n");  
    printf("\nPosiciona la cámara en (100, 100, 75) para estar en el centro\n");  
    return 0;  
}