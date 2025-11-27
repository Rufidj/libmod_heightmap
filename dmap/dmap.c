/*  
 * Creador de mapas DMP2 - Versión final que escribe archivos válidos  
 * Genera una sala cuadrada de 1024x1024 con texturas desde TEX  
 */  
  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <stdint.h>  
  
#pragma pack(push, 1)  
  
// Estructuras DMP2 (coincidentes con libmod_heightmap.h)  
typedef struct {  
    char magic[4];              // "DMP2"  
    uint32_t version;           // 1  
    uint32_t num_points;  
    uint32_t num_regions;  
    uint32_t num_walls;  
    uint32_t num_textures;  
} DMP2_HEADER;  
  
typedef struct {  
    int16_t x, y;  
    int16_t Type;    // Mayúscula igual que SECTOR_Point  
    int32_t link;  
} DMP2_POINT;  
  
typedef struct {  
    int16_t floor_height;  
    int16_t ceiling_height;  
    int16_t floor_texture;  
    int16_t ceiling_texture;  
    int16_t light_level;  
    int16_t flags;  
} DMP2_REGION;  
  
typedef struct {  
    int16_t point1, point2;      // 4 bytes  
    int16_t region1, region2;     // 4 bytes    
    int16_t texture;              // 2 bytes  
    int16_t flags;                // 2 bytes  
    uint16_t x_offset, y_offset;  // 4 bytes  
} __attribute__((packed)) DMP2_WALL;  // Total: 16 bytes  
  
#pragma pack(pop)  
  
void create_square_room_dmp2_tex(const char *filename) {  
    FILE *file = fopen(filename, "wb");  
    if (!file) {  
        printf("Error: No se pudo crear archivo DMP2: %s\n", filename);  
        return;  
    }  
      
    printf("DEBUG: Creando DMP2 con texturas TEX\n");  
      
    // Header DMP2  
    DMP2_HEADER header;  
    memset(&header, 0, sizeof(DMP2_HEADER));  
    memcpy(header.magic, "DMP2", 4);  
    header.version = 1;  
    header.num_points = 4;  
    header.num_regions = 1;  
    header.num_walls = 4;  
    header.num_textures = 3;  
      
    printf("DEBUG: Escribiendo header (%zu bytes)\n", sizeof(DMP2_HEADER));  
    fwrite(&header, sizeof(DMP2_HEADER), 1, file);  
      
    // Puntos de la sala (1024x1024)  
    DMP2_POINT points[4] = {  
        {-512, -512, 0, -1},  // Esquina inferior izquierda  
        { 512, -512, 0, -1},  // Esquina inferior derecha  
        { 512,  512, 0, -1},  // Esquina superior derecha  
        {-512,  512, 0, -1}   // Esquina superior izquierda  
    };  
      
    printf("DEBUG: Escribiendo %d puntos (%zu bytes)\n", header.num_points, sizeof(DMP2_POINT) * header.num_points);  
    fwrite(points, sizeof(DMP2_POINT), header.num_points, file);  
      
    // Región (sala)  
    DMP2_REGION region = {  
        .floor_height = 0,  
        .ceiling_height = 150,  
        .floor_texture = 2,      // Suelo = textura 2  
        .ceiling_texture = 3,    // Techo = textura 3  
        .light_level = 200,  
        .flags = 0  
    };  
      
    printf("DEBUG: Escribiendo %d regiones (%zu bytes)\n", header.num_regions, sizeof(DMP2_REGION) * header.num_regions);  
    fwrite(&region, sizeof(DMP2_REGION), header.num_regions, file);  
      
    // Paredes con texturas  
    DMP2_WALL walls[4] = {  
        {0, 1, 0, -1, 1, 0, 0, 0},    // Pared sur (textura 1)  
        {1, 2, 0, -1, 1, 0, 0, 0},    // Pared este (textura 1)  
        {2, 3, 0, -1, 1, 0, 0, 0},    // Pared norte (textura 1)  
        {3, 0, 0, -1, 1, 0, 0, 0}     // Pared oeste (textura 1)  
    };  
      
    printf("DEBUG: Escribiendo %d paredes (%zu bytes)\n", header.num_walls, sizeof(DMP2_WALL) * header.num_walls);  
    size_t walls_written = fwrite(walls, sizeof(DMP2_WALL), header.num_walls, file);  
    printf("DEBUG: Paredes escritas: %zu\n", walls_written);  
      
    // Índices de texturas para DMP2  
    int16_t texture_indices[3] = {1, 2, 3};  
      
    printf("DEBUG: Escribiendo %d texturas (%zu bytes)\n", header.num_textures, sizeof(int16_t) * header.num_textures);  
    fwrite(texture_indices, sizeof(int16_t), header.num_textures, file);  
      
    fclose(file);  
      
    // Verificación final  
    file = fopen(filename, "rb");  
    fseek(file, 0, SEEK_END);  
    long file_size = ftell(file);  
    fclose(file);  
      
    printf("DEBUG: DMP2 creado: %s (%ld bytes)\n", filename, file_size);  
    printf("DEBUG: Esperado: header=%zu + puntos=%zu + regiones=%zu + paredes=%zu + texturas=%zu = %zu bytes\n",  
           sizeof(DMP2_HEADER),  
           sizeof(DMP2_POINT) * 4,  
           sizeof(DMP2_REGION) * 1,  
           sizeof(DMP2_WALL) * 4,  
           sizeof(int16_t) * 3,  
           sizeof(DMP2_HEADER) + sizeof(DMP2_POINT) * 4 + sizeof(DMP2_REGION) * 1 + sizeof(DMP2_WALL) * 4 + sizeof(int16_t) * 3);  
}  
  
int main() {  
    create_square_room_dmp2_tex("test_room_1024.dmp2");  
    return 0;  
}