// tex_creator_simple.c - Versión sin conversión gamma  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <stdint.h>  
#include <png.h>  
#include <dirent.h>  
  
#pragma pack(push, 1)  
  
typedef struct {  
    char magic[4];  
    uint32_t version;  
    uint16_t num_images;  
    uint8_t reserved[6];  
} TEX_HEADER;  
  
typedef struct {  
    uint16_t index;  
    uint16_t width;  
    uint16_t height;  
    uint16_t format;  
    uint8_t reserved[250];  
} TEX_ENTRY;  
  
#pragma pack(pop)  
  
int load_png_to_rgba(const char *filename, uint8_t **rgba_data, int *width, int *height) {  
    FILE *fp = fopen(filename, "rb");  
    if (!fp) {  
        printf("Error: No se pudo abrir PNG: %s\n", filename);  
        return 0;  
    }  
      
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);  
    if (!png) {  
        fclose(fp);  
        return 0;  
    }  
      
    png_infop info = png_create_info_struct(png);  
    if (!info) {  
        png_destroy_read_struct(&png, NULL, NULL);  
        fclose(fp);  
        return 0;  
    }  
      
    if (setjmp(png_jmpbuf(png))) {  
        png_destroy_read_struct(&png, &info, NULL);  
        fclose(fp);  
        return 0;  
    }  
      
    png_init_io(png, fp);  
    png_read_info(png, info);  
      
    *width = png_get_image_width(png, info);  
    *height = png_get_image_height(png, info);  
      
    png_byte color_type = png_get_color_type(png, info);  
    png_byte bit_depth = png_get_bit_depth(png, info);  
      
    if (bit_depth == 16) png_set_strip_16(png);  
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);  
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);  
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);  
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)  
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);  
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)  
        png_set_gray_to_rgb(png);  
      
    png_read_update_info(png, info);  
      
    *rgba_data = malloc((*width) * (*height) * 4);  
    png_bytep *row_pointers = malloc(sizeof(png_bytep) * (*height));  
      
    for (int y = 0; y < *height; y++) {  
        row_pointers[y] = (*rgba_data) + y * (*width) * 4;  
    }  
      
    png_read_image(png, row_pointers);  
      
    free(row_pointers);  
    png_destroy_read_struct(&png, &info, NULL);  
    fclose(fp);  
      
    return 1;  
}  
  
int scan_png_files(const char *folder, char ***png_files, int **indices) {  
    DIR *dir;  
    struct dirent *entry;  
    int count = 0;  
    int capacity = 10;  
      
    *png_files = malloc(capacity * sizeof(char*));  
    *indices = malloc(capacity * sizeof(int));  
      
    dir = opendir(folder);  
    if (!dir) {  
        printf("Error: No se pudo abrir carpeta %s\n", folder);  
        return 0;  
    }  
      
    while ((entry = readdir(dir)) != NULL) {  
        if (strstr(entry->d_name, ".png") || strstr(entry->d_name, ".PNG")) {  
            char *full_path = malloc(strlen(folder) + strlen(entry->d_name) + 2);  
            sprintf(full_path, "%s/%s", folder, entry->d_name);  
              
            if (count >= capacity) {  
                capacity *= 2;  
                *png_files = realloc(*png_files, capacity * sizeof(char*));  
                *indices = realloc(*indices, capacity * sizeof(int));  
            }  
              
            (*png_files)[count] = full_path;  
            (*indices)[count] = count + 1;  
              
            printf("Encontrado PNG: %s -> índice %d\n", entry->d_name, count + 1);  
            count++;  
        }  
    }  
      
    closedir(dir);  
    return count;  
}  
  
int create_tex_from_folder(const char *tex_filename, const char *folder) {  
    FILE *tex_file = fopen(tex_filename, "wb");  
    if (!tex_file) {  
        printf("Error: No se pudo crear archivo TEX: %s\n", tex_filename);  
        return 0;  
    }  
      
    char **png_files = NULL;  
    int *indices = NULL;  
    int num_files = scan_png_files(folder, &png_files, &indices);  
      
    if (num_files == 0) {  
        printf("No se encontraron archivos PNG en %s\n", folder);  
        fclose(tex_file);  
        return 0;  
    }  
      
    TEX_HEADER header;  
    memcpy(header.magic, "TEX\0", 4);  
    header.version = 1;  
    header.num_images = num_files;  
    memset(header.reserved, 0, 6);  
      
    fwrite(&header, sizeof(TEX_HEADER), 1, tex_file);  
      
    for (int i = 0; i < num_files; i++) {  
        uint8_t *rgba_data = NULL;  
        int width, height;  
          
        if (load_png_to_rgba(png_files[i], &rgba_data, &width, &height)) {  
            TEX_ENTRY entry;  
            entry.index = indices[i];  
            entry.width = width;  
            entry.height = height;  
            entry.format = 1;  
            memset(entry.reserved, 0, 250);  
              
            fwrite(&entry, sizeof(TEX_ENTRY), 1, tex_file);  
            fwrite(rgba_data, 1, width * height * 4, tex_file);  
              
            printf("Añadida imagen %d: %s (%dx%d)\n", indices[i], png_files[i], width, height);  
              
            free(rgba_data);  
        }  
          
        free(png_files[i]);  
    }  
      
    fclose(tex_file);  
    free(png_files);  
    free(indices);  
      
    printf("Archivo TEX creado: %s (%d imágenes)\n", tex_filename, num_files);  
    return 1;  
}  
  
int main() {  
    printf("=== Creador de Archivos TEX (Simple) ===\n");  
      
    if (create_tex_from_folder("assets/textures.tex", "assets")) {  
        printf("\n¡Éxito! Los índices se asignaron automáticamente:\n");  
        printf("  - Primer PNG encontrado -> índice 1\n");  
        printf("  - Segundo PNG encontrado -> índice 2\n");  
        printf("  - etc...\n");  
        return 0;  
    }  
      
    printf("\nError al crear archivo TEX\n");  
    return 1;  
}