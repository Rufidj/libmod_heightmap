/*  
 * Creador de mapas DMP2 - Versión final que escribe archivos válidos  
 * Genera una sala cuadrada de 1024x1024 con texturas desde TEX  
 */  
  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <stdint.h>  
  
#pragma pack(push, 1)  
  
// ========================================  
// ESTRUCTURAS DMP2 (coincidentes con libmod_heightmap.h)  
// ========================================  
  
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
  
// Función para crear una sala cuadrada DMP2 para TEX  
int create_square_room_dmp2_tex(const char *filename) {  
    FILE *file = fopen(filename, "wb");  
    if (!file) {  
        printf("Error: No se pudo crear archivo DMP2: %s\n", filename);  
        return 0;  
    }  
      
    printf("DEBUG: Creando sala DMP2 1024x1024 para TEX\n");  
    printf("DEBUG: sizeof(DMP2_HEADER) = %zu bytes\n", sizeof(DMP2_HEADER));  
    printf("DEBUG: sizeof(DMP2_POINT) = %zu bytes\n", sizeof(DMP2_POINT));  
    printf("DEBUG: sizeof(DMP2_REGION) = %zu bytes\n", sizeof(DMP2_REGION));  
    printf("DEBUG: sizeof(DMP2_WALL) = %zu bytes\n", sizeof(DMP2_WALL));  
      
    // ========================================  
    // HEADER DMP2  
    // ========================================  
      
    DMP2_HEADER header;  
    memset(&header, 0, sizeof(DMP2_HEADER));  
    memcpy(header.magic, "DMP2", 4);  
    header.version = 1;  
    header.num_points = 4;        // 4 esquinas  
    header.num_regions = 1;       // 1 región interior  
    header.num_walls = 4;         // 4 paredes  
    header.num_textures = 3;      // pared, suelo, techo  
      
    printf("DEBUG: Escribiendo header DMP2 (%zu bytes)\n", sizeof(DMP2_HEADER));  
    size_t header_written = fwrite(&header, sizeof(DMP2_HEADER), 1, file);  
    printf("DEBUG: Header escrito: %zu/1\n", header_written);  
      
    // ========================================  
    // PUNTOS DMP2 (esquinas de la sala)  
    // ========================================  
      
    DMP2_POINT dmp2_points[4] = {  
        {-512, -512, 0, -1},     // esquina inferior izquierda  
        { 512, -512, 0, -1},     // esquina inferior derecha  
        { 512,  512, 0, -1},     // esquina superior derecha  
        {-512,  512, 0, -1}      // esquina superior izquierda  
    };  
      
    printf("DEBUG: Escribiendo %d puntos DMP2 (%zu bytes)\n", header.num_points,   
           sizeof(DMP2_POINT) * header.num_points);  
    size_t points_written = fwrite(dmp2_points, sizeof(DMP2_POINT), header.num_points, file);  
    printf("DEBUG: Puntos escritos: %zu/%d\n", points_written, header.num_points);  
      
    // ========================================  
    // REGIONES DMP2 (sala interior)  
    // ========================================  
      
    DMP2_REGION dmp2_regions[1] = {  
        {0, 256, 2, 3, 255, 0}    // suelo=2, techo=3, iluminación máxima  
    };  
      
    printf("DEBUG: Escribiendo %d regiones DMP2 (%zu bytes)\n", header.num_regions,  
           sizeof(DMP2_REGION) * header.num_regions);  
    size_t regions_written = fwrite(dmp2_regions, sizeof(DMP2_REGION), header.num_regions, file);  
    printf("DEBUG: Regiones escritas: %zu/%d\n", regions_written, header.num_regions);  
      
    // ========================================  
    // PAREDES DMP2 (4 paredes de la sala)  
    // ========================================  
      
    DMP2_WALL dmp2_walls[4] = {  
        // Pared inferior (punto 0 -> punto 1)  
        {0, 1, 0, -1, 1, 0, 0, 0},  
        // Pared derecha (punto 1 -> punto 2)  
        {1, 2, 0, -1, 1, 0, 0, 0},  
        // Pared superior (punto 2 -> punto 3)  
        {2, 3, 0, -1, 1, 0, 0, 0},  
        // Pared izquierda (punto 3 -> punto 0)  
        {3, 0, 0, -1, 1, 0, 0, 0}  
    };  
      
    printf("DEBUG: Escribiendo %d paredes DMP2 (%zu bytes)\n", header.num_walls,  
           sizeof(DMP2_WALL) * header.num_walls);  
    size_t walls_written = fwrite(dmp2_walls, sizeof(DMP2_WALL), header.num_walls, file);  
    printf("DEBUG: Paredes escritas: %zu/%d\n", walls_written, header.num_walls);  
      
    // ========================================  
    // TEXTURAS DMP2 (índices para archivo TEX)  
    // ========================================  
      
    int16_t texture_indices[3] = {1, 2, 3};  // pared=1, suelo=2, techo=3  
      
    printf("DEBUG: Escribiendo %d texturas DMP2 (%zu bytes)\n", header.num_textures,  
           sizeof(int16_t) * header.num_textures);  
    size_t textures_written = fwrite(texture_indices, sizeof(int16_t), header.num_textures, file);  
    printf("DEBUG: Texturas escritas: %zu/%d\n", textures_written, header.num_textures);  
      
    // ========================================  
    // VERIFICACIÓN FINAL  
    // ========================================  
      
    long current_pos = ftell(file);  
    printf("DEBUG: Posición final del archivo: %ld bytes\n", current_pos);  
      
    // Calcular tamaño esperado  
    long expected_size = sizeof(DMP2_HEADER) +   
                        (sizeof(DMP2_POINT) * header.num_points) +  
                        (sizeof(DMP2_REGION) * header.num_regions) +  
                        (sizeof(DMP2_WALL) * header.num_walls) +  
                        (sizeof(int16_t) * header.num_textures);  
      
    printf("DEBUG: Tamaño esperado: %ld bytes\n", expected_size);  
    printf("DEBUG: Tamaño real: %ld bytes\n", current_pos);  
    printf("DEBUG: Diferencia: %ld bytes\n", current_pos - expected_size);  
      
    fclose(file);  
      
    printf("✓ Archivo DMP2 creado: %s (%ld bytes)\n", filename, current_pos);  
    printf("✓ Configuración para TEX: texturas 1=pared, 2=suelo, 3=techo\n");  
      
    return 1;  
}  
  
int main() {  
    printf("=== Creador de Mapas DMP2 para Sistema TEX ===\n");  
    printf("Creando sala de 1024x1024 unidades...\n\n");  
      
    if (create_square_room_dmp2_tex("test_room_1024.dmp2")) {  
        printf("\n✓ Éxito: Mapa DMP2 creado correctamente\n");  
        printf("Usa en BennuGD2:\n");  
        printf("  LOAD_TEX_FILE(\"assets/textures.tex\");\n");  
        printf("  map_id = HEIGHTMAP_LOAD_DMP2(\"test_room_1024.dmp2\", 1);\n");  
        return 0;  
    }  
      
    printf("\n✗ Error: No se pudo crear el mapa DMP2\n");  
    return 1;  
}