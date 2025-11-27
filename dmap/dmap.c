/*  
 * Creador de mapas DMP2 - Formato TEX para libmod_heightmap  
 * Genera una sala cuadrada de 1024x1024 con texturas desde TEX  
 */  
  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <stdint.h>  
  
#pragma pack(push, 1)  
  
// Estructuras DMP2 (coincidentes con las del header)  
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
    int16_t type;  
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
} DMP2_WALL;  // Total: 16 byte
  
#pragma pack(pop)  
  
// Función para crear una sala cuadrada DMP2 para TEX  
int create_square_room_dmp2_tex(const char *filename, int room_size) {  
    FILE *file = fopen(filename, "wb");  
    if (!file) {  
        printf("Error: No se pudo crear archivo %s\n", filename);  
        return 0;  
    }  
      
    // ========================================  
    // CONFIGURACIÓN DE LA SALA (1024x1024)  
    // ========================================  
      
    // Definir tamaño de la sala (1024x1024 unidades)  
    int half_size = room_size / 2;  
      
    // Índices de texturas en TEX (no FPG)  
    // 1 = textura de paredes  
    // 2 = textura de suelo    
    // 3 = textura de techo  
    int16_t wall_texture_index = 1;  
    int16_t floor_texture_index = 2;  
    int16_t ceiling_texture_index = 3;  
      
    // ========================================  
    // PREPARAR HEADER DMP2  
    // ========================================  
      
    DMP2_HEADER header;  
    memcpy(header.magic, "DMP2", 4);  
    header.version = 1;  
    header.num_points = 4;      // 4 esquinas  
    header.num_regions = 1;     // 1 región interior  
    header.num_walls = 4;       // 4 paredes  
    header.num_textures = 3;    // pared, suelo, techo (desde TEX)  
      
    // ========================================  
    // CREAR PUNTOS (ESQUINAS)  
    // ========================================  
      
    DMP2_POINT points[4];  
      
    // Esquina superior izquierda  
    points[0].x = -half_size;  
    points[0].y = -half_size;  
    points[0].type = 0;  
    points[0].link = -1;  
      
    // Esquina superior derecha  
    points[1].x = half_size;  
    points[1].y = -half_size;  
    points[1].type = 0;  
    points[1].link = -1;  
      
    // Esquina inferior derecha  
    points[2].x = half_size;  
    points[2].y = half_size;  
    points[2].type = 0;  
    points[2].link = -1;  
      
    // Esquina inferior izquierda  
    points[3].x = -half_size;  
    points[3].y = half_size;  
    points[3].type = 0;  
    points[3].link = -1;  
      
    // ========================================  
    // CREAR REGIÓN (INTERIOR)  
    // ========================================  
      
    DMP2_REGION region;  
    region.floor_height = 0;           // Suelo a nivel 0  
    region.ceiling_height = 256;       // Techo a 256 unidades de altura  
    region.floor_texture = floor_texture_index;   // Textura 2 desde TEX  
    region.ceiling_texture = ceiling_texture_index; // Textura 3 desde TEX  
    region.light_level = 255;          // Iluminación máxima  
    region.flags = 0;                  // Sin flags especiales  
      
    // ========================================  
    // CREAR PAREDES  
    // ========================================  
      
    DMP2_WALL walls[4];  
      
    // Pared norte (superior)  
    walls[0].point1 = 0;  
    walls[0].point2 = 1;  
    walls[0].region1 = 0;      // Interior  
    walls[0].region2 = -1;     // Exterior (sólido)  
    walls[0].texture = wall_texture_index;  // Textura 1 desde TEX  
    walls[0].flags = 0;  
    walls[0].x_offset = 0;  
    walls[0].y_offset = 0;  
      
    // Pared este (derecha)  
    walls[1].point1 = 1;  
    walls[1].point2 = 2;  
    walls[1].region1 = 0;      // Interior  
    walls[1].region2 = -1;     // Exterior (sólido)  
    walls[1].texture = wall_texture_index;  // Textura 1 desde TEX  
    walls[1].flags = 0;  
    walls[1].x_offset = 0;  
    walls[1].y_offset = 0;  
      
    // Pared sur (inferior)  
    walls[2].point1 = 2;  
    walls[2].point2 = 3;  
    walls[2].region1 = 0;      // Interior  
    walls[2].region2 = -1;     // Exterior (sólido)  
    walls[2].texture = wall_texture_index;  // Textura 1 desde TEX  
    walls[2].flags = 0;  
    walls[2].x_offset = 0;  
    walls[2].y_offset = 0;  
      
    // Pared oeste (izquierda)  
    walls[3].point1 = 3;  
    walls[3].point2 = 0;  
    walls[3].region1 = 0;      // Interior  
    walls[3].region2 = -1;     // Exterior (sólido)  
    walls[3].texture = wall_texture_index;  // Textura 1 desde TEX  
    walls[3].flags = 0;  
    walls[3].x_offset = 0;  
    walls[3].y_offset = 0;  
      
    // ========================================  
    // ESCRIBIR ARCHIVO DMP2  
    // ========================================  
      
    // Escribir header  
    fwrite(&header, sizeof(DMP2_HEADER), 1, file);  
      
    // Escribir puntos  
    fwrite(points, sizeof(DMP2_POINT), header.num_points, file);  
      
    // Escribir región  
    fwrite(&region, sizeof(DMP2_REGION), header.num_regions, file);  
      
    // Escribir paredes  
    fwrite(walls, sizeof(DMP2_WALL), header.num_walls, file);  
      
    // Escribir índices de texturas (para TEX)  
    int16_t texture_indices[] = {wall_texture_index, floor_texture_index, ceiling_texture_index};  
    fwrite(texture_indices, sizeof(int16_t), header.num_textures, file);  
      
    fclose(file);  
      
    printf("Mapa DMP2 creado: %s\n", filename);  
    printf("  - Sala cuadrada: %dx%d unidades\n", room_size, room_size);  
    printf("  - Puntos: %d, Regiones: %d, Paredes: %d\n",   
           header.num_points, header.num_regions, header.num_walls);  
    printf("  - Texturas TEX: pared=%d, suelo=%d, techo=%d\n",  
           wall_texture_index, floor_texture_index, ceiling_texture_index);  
    printf("  - Cargar con: LOAD_TEX_FILE(\"textures.tex\")\n");  
    printf("  - Luego: HEIGHTMAP_LOAD_DMP2(\"%s\", 1)\n", filename);  
      
    return 1;  
}  
  
// Función principal para probar la creación  
int main() {  
    printf("=== Creador de Mapas DMP2 para TEX ===\n");  
      
    // Crear sala cuadrada de 1024x1024 unidades  
    if (create_square_room_dmp2_tex("assets/test_room_1024.dmp2", 1024)) {  
        printf("\n¡Mapa DMP2 para TEX creado exitosamente!\n");  
        printf("Usa en BennuGD2:\n");  
        printf("  LOAD_TEX_FILE(\"assets/textures.tex\");\n");  
        printf("  map_id = HEIGHTMAP_LOAD_DMP2(\"assets/test_room_1024.dmp2\", 1);\n");  
        printf("  HEIGHTMAP_RENDER_SECTOR_CPU(map_id, 640, 480);\n");  
        return 0;  
    }  
      
    printf("\nError al crear el mapa DMP2\n");  
    return 1;  
}