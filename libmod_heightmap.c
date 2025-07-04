/*
 * Heightmap Module Implementation for BennuGD2 - Con soporte de luz, agua y color de cielo
 */

#include "libmod_heightmap.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "libbggfx.h"  

// Si aún tienes problemas, puedes definir manualmente los offsets:  
#ifndef CTYPE  
enum {  
    CTYPE = 0,  
    CNUMBER,  
    COORDX,  
    COORDY,  
    COORDZ,  
    FILEID,  
    GRAPHID,  
    GRAPHSIZE,  
    ANGLE,  
    FLAGS,  
    REGIONID,  
    RESOLUTION,  
    GRAPHSIZEX,  
    GRAPHSIZEY,  
    XGRAPH,  
    _OBJECTID,  
    XGRAPH_FLAGS,  
    STATUS,  
    PROCESS_ID,  
    RENDER_FILEID,  
    RENDER_GRAPHID  
};  
#endif

#define MAX_BILLBOARDS 64  
  
typedef struct {  
    float world_x, world_y, world_z;  
    int graph_id;  
    float scale;  
    int active;  
} VOXEL_BILLBOARD;  

typedef struct {  
    uint32_t final_color;  // Color final con transparencia aplicada  
    uint8_t has_water;     // 1 si hay agua, 0 si no  
    float water_depth;     // Profundidad del agua para efectos  
} PRECALC_WATER_DATA;  
  
static PRECALC_WATER_DATA *precalc_water = NULL;
// Variables globales para el sistema de texturas de agua  
static GRAPH *water_texture = NULL;  
static int64_t water_texture_id = 0;  
static float water_texture_scale = 1.0f;  
static float water_texture_offset_x = 0.0f;  
static float water_texture_offset_y = 0.0f;  
static int water_texture_alpha_override = -1; // -1 = usar alpha de la textura, 0-255 = override
  
static VOXEL_BILLBOARD voxel_billboards[MAX_BILLBOARDS];
static int billboard_count = 0;  
static int current_heightmap_id = 0; 
static GRAPH *render_buffer = NULL;
  static float *fog_table = NULL;      
    static int fog_table_size = 0;      
    static int fog_table_initialized = 0;     

#define C_BILLBOARD 999  
  
// Declaración forward corregida  
int64_t libmod_heightmap_world_to_screen(INSTANCE *my, int64_t *params);
uint32_t calculate_water_color(float world_x, float world_y, float water_depth, float time);

  
#ifndef M_PI_2
#define M_PI_2 1.57079632679f
#endif

#define RGBA32_R(color) ((color >> 16) & 0xFF)
#define RGBA32_G(color) ((color >> 8) & 0xFF)
#define RGBA32_B(color) (color & 0xFF)
#define RGBA32_A(color) ((color >> 24) & 0xFF)

// Variables globales para configuración de renderizado
static float max_render_distance = 800.0f;
static int chunk_size = 96;
static int chunk_radius = 4;

// Variables globales para el color del cielo
static Uint8 sky_color_r = 135; // Azul cielo por defecto
static Uint8 sky_color_g = 206;
static Uint8 sky_color_b = 235;
static Uint8 sky_color_a = 255; // Alpha del cielo

// Variables globales para el color y la transparencia del agua
static Uint8 water_color_r = 64;
static Uint8 water_color_g = 128;
static Uint8 water_color_b = 255;
static Uint8 water_color_a = 128; // Valor inicial para transparencia (0-255)
static float water_time = 0.0f;

// Variable global para almacenar el ID del sprite a seguir
static int64_t camera_follow_sprite_id = 0;
static float camera_follow_offset_x = 0.0f;
static float camera_follow_offset_y = 0.0f;
static float camera_follow_offset_z = 10.0f;
static float camera_follow_speed = 5.0f;

// Declaración por si no está en el header
void gr_alpha_put_pixel(GRAPH *dest, int x, int y, uint32_t color, uint8_t alpha);

HEIGHTMAP heightmaps[MAX_HEIGHTMAPS];
CAMERA_3D camera = {0, 0, 0, 0, 0, DEFAULT_FOV, DEFAULT_NEAR, DEFAULT_FAR};
int64_t next_heightmap_id = 1;
float water_level = -1.0f;
int light_intensity = 255;

static float mouse_sensitivity = 50.0f;
static float move_speed = 3.0f;
static float height_speed = 3.0f;

float get_height_at(HEIGHTMAP *hm, float x, float y);
void build_height_cache(HEIGHTMAP *hm);
void clamp_camera_to_terrain(HEIGHTMAP *hm);
void clamp_camera_to_bounds(HEIGHTMAP *hm);
uint32_t get_texture_color_bilinear(GRAPH *texture, float x, float y);
int64_t libmod_heightmap_project_billboard(INSTANCE *my, int64_t *params);

void libmod_heightmap_destroy_render_buffer() {  
    if (render_buffer) {  
        bitmap_destroy(render_buffer);  
        render_buffer = NULL;  
    }  
}

void __bgdexport(libmod_heightmap, module_initialize)()
{
    memset(heightmaps, 0, sizeof(heightmaps));
    next_heightmap_id = 1;
}

void __bgdexport(libmod_heightmap, module_finalize)()  
{  
    // Liberar recursos asociados a cada heightmap  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)  
    {  
        if (heightmaps[i].heightmap) // Verificar si el heightmap existe/está en uso  
        {  
            // Liberar el cache de altura si existe  
            if (heightmaps[i].height_cache)  
            {  
                free(heightmaps[i].height_cache);  
                heightmaps[i].height_cache = NULL;  
            }  
  
            // Destruir el GRAPH del heightmap principal  
            bitmap_destroy(heightmaps[i].heightmap);  
            heightmaps[i].heightmap = NULL;  
  
            // Destruir el GRAPH del texturemap si existe  
            if (heightmaps[i].texturemap)  
            {  
                bitmap_destroy(heightmaps[i].texturemap);  
                heightmaps[i].texturemap = NULL;  
            }  
        }  
    }  
  
    // Liberar el render_buffer global una sola vez  
    libmod_heightmap_destroy_render_buffer();  
  
    // Liberar el fog_table global si existe (como se discutió previamente)  
    if (fog_table) {  
        free(fog_table);  
        fog_table = NULL;  
    }  
  
    // Limpiar la estructura global heightmaps (opcional, pero buena práctica)  
    memset(heightmaps, 0, sizeof(heightmaps));  
}

int64_t libmod_heightmap_set_light(INSTANCE *my, int64_t *params)
{
    int level = params[0];
    if (level < 0)
        level = 0;
    if (level > 255)
        level = 255;
    light_intensity = level;
    return 1;
}

int64_t libmod_heightmap_set_water_level(INSTANCE *my, int64_t *params)
{
    water_level = (float)params[0];
    return 1;
}

/* Cargar mapa de altura desde archivo */
int64_t libmod_heightmap_load(INSTANCE *my, int64_t *params)
{
    const char *filename = string_get(params[0]);
    int64_t map_id = gr_load_img(filename);
    string_discard(params[0]);

    if (!map_id)
        return 0;
    GRAPH *graph = bitmap_get(0, map_id);
    if (!graph)
        return 0;

    int slot = -1;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (!heightmaps[i].heightmap)
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
        return 0;

    heightmaps[slot].id = next_heightmap_id++;
    heightmaps[slot].heightmap = graph;
    heightmaps[slot].texturemap = NULL;
    heightmaps[slot].width = graph->width;
    heightmaps[slot].height = graph->height;
    heightmaps[slot].height_cache = NULL;
    heightmaps[slot].cache_valid = 0;

    build_height_cache(&heightmaps[slot]);

    return heightmaps[slot].id;
}

/* Crear mapa de altura en memoria */
int64_t libmod_heightmap_create(INSTANCE *my, int64_t *params)
{
    int64_t width = params[0];
    int64_t height = params[1];

    GRAPH *graph = bitmap_new_syslib(width, height);
    if (!graph)
        return 0;

    int slot = -1;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (!heightmaps[i].heightmap)
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
        return 0;

    heightmaps[slot].id = next_heightmap_id++;
    heightmaps[slot].heightmap = graph;
    heightmaps[slot].texturemap = NULL;
    heightmaps[slot].width = width;
    heightmaps[slot].height = height;
    heightmaps[slot].height_cache = NULL;
    heightmaps[slot].cache_valid = 0;

    build_height_cache(&heightmaps[slot]);
    if (!heightmaps[slot].cache_valid)
    {
        bitmap_destroy(heightmaps[slot].heightmap);
        memset(&heightmaps[slot], 0, sizeof(HEIGHTMAP));
        return 0;
    }

    return heightmaps[slot].id; // ← Esta línea faltaba
} // ← Esta llave de cierre faltaba

/* Descargar mapa de altura */
int64_t libmod_heightmap_unload(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];

    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            if (heightmaps[i].height_cache)
            {
                free(heightmaps[i].height_cache);
                heightmaps[i].height_cache = NULL;
            }

            // Destruir correctamente la estructura GRAPH
            if (heightmaps[i].heightmap)
            {
                bitmap_destroy(heightmaps[i].heightmap);
                heightmaps[i].heightmap = NULL;
            }

            // Limpiar mapa de textura si existe
            if (heightmaps[i].texturemap)
            {
                bitmap_destroy(heightmaps[i].texturemap);
                heightmaps[i].texturemap = NULL;
            }

            memset(&heightmaps[i], 0, sizeof(HEIGHTMAP));
            return 1;
        }
    }
    return 0;
}

/* Obtener altura */
int64_t libmod_heightmap_get_height(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    float x = (float)params[1];
    float y = (float)params[2];

    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            float height = get_height_at(&heightmaps[i], x, y);
            return (int64_t)(height * 1000);
        }
    }
    return 0;
}

float get_height_at(HEIGHTMAP *hm, float x, float y)
{
    if (!hm->cache_valid)
        return 0;

    int ix = (int)x;
    int iy = (int)y;

    if (ix < 0 || ix >= hm->width - 1 || iy < 0 || iy >= hm->height - 1)
        return 0;

    float fx = x - ix;
    float fy = y - iy;

    float h00 = hm->height_cache[iy * hm->width + ix];
    float h10 = hm->height_cache[iy * hm->width + (ix + 1)];
    float h01 = hm->height_cache[(iy + 1) * hm->width + ix];
    float h11 = hm->height_cache[(iy + 1) * hm->width + (ix + 1)];

    float h0 = h00 + fx * (h10 - h00);
    float h1 = h01 + fx * (h11 - h01);
    float h = h0 + fy * (h1 - h0);

    return h;
}

/* Configurar cámara 3D - Original */
int64_t libmod_heightmap_set_camera(INSTANCE *my, int64_t *params)
{
    camera.x = (float)params[0];
    camera.y = (float)params[1];
    camera.z = (float)params[2];
    camera.angle = (float)params[3] / 1000.0f;
    camera.pitch = (float)params[4] / 1000.0f;
    camera.fov = (params[5] > 0) ? (float)params[5] : DEFAULT_FOV;

    // Limitación de pitch original
    const float max_pitch = M_PI_2 * 0.99f;
    if (camera.pitch > max_pitch)
        camera.pitch = max_pitch;
    if (camera.pitch < -max_pitch)
        camera.pitch = -max_pitch;

    return 1;
}

/* Cargar textura */
int64_t libmod_heightmap_load_texture(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    const char *filename = string_get(params[1]);

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm)
    {
        string_discard(params[1]);
        return 0;
    }

    int64_t map_id = gr_load_img(filename);
    string_discard(params[1]);
    if (!map_id)
        return 0;

    GRAPH *graph = bitmap_get(0, map_id);
    if (!graph)
        return 0;

    hm->texturemap = graph;
    return 1;
}

/* Función auxiliar para obtener color corregido de textura */
static uint32_t get_texture_pixel_color_corrected(GRAPH *texture, int x, int y, float brightness, float total_light)
{
    if (!texture)
        return 0;

    uint32_t tex = gr_get_pixel(texture, x, y);

    // Usar SDL_GetRGB para extraer componentes correctamente
    Uint8 r, g, b;
    SDL_GetRGB(tex, gPixelFormat, &r, &g, &b);

    // Aplicar brillo y luz total
    r = (Uint8)(r * brightness * total_light);
    g = (Uint8)(g * brightness * total_light);
    b = (Uint8)(b * brightness * total_light);

    // Clamp automático con Uint8, pero verificar por seguridad
    if (r > 255)
        r = 255;
    if (g > 255)
        g = 255;
    if (b > 255)
        b = 255;

    // Usar SDL_MapRGB para reconstruir el color correctamente
    return SDL_MapRGB(gPixelFormat, r, g, b);
}
int64_t libmod_heightmap_render_voxelspace(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    HEIGHTMAP *hm = NULL;  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == hm_id) {  
            hm = &heightmaps[i];  
            break;  
        }  
    }  
  
    if (!hm || !hm->cache_valid)  
        return 0;  
  
    if (!render_buffer) {  
        render_buffer = bitmap_new_syslib(320, 240);  
        if (!render_buffer) return 0;  
    }  
  
    static float *depth_buffer = NULL;  
    if (!depth_buffer) {  
        depth_buffer = malloc(320 * 240 * sizeof(float));  
    }  
  
    for (int i = 0; i < 320 * 240; i++) {  
        depth_buffer[i] = max_render_distance;  
    }  
  
    uint32_t background_color = SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, sky_color_a);  
    gr_clear_as(render_buffer, background_color);  
  
    static float last_camera_x = 0, last_camera_y = 0, last_camera_angle = 0;  
    float movement = fabs(camera.x - last_camera_x) + fabs(camera.y - last_camera_y) + fabs(camera.angle - last_camera_angle);  
    int quality_step = (movement > 5.0f) ? 2 : 1;  
    last_camera_x = camera.x;  
    last_camera_y = camera.y;  
    last_camera_angle = camera.angle;  
  
    int chunk_x = (int)(camera.x / chunk_size);  
    int chunk_y = (int)(camera.y / chunk_size);  
    int min_chunk_x = chunk_x - chunk_radius;  
    int max_chunk_x = chunk_x + chunk_radius;  
    int min_chunk_y = chunk_y - chunk_radius;  
    int max_chunk_y = chunk_y + chunk_radius;  
  
    float terrain_fov = 0.7f;  
    float angle_step = terrain_fov / 320.0f;  
    float base_angle = camera.angle - terrain_fov * 0.5f;  
    float pitch_offset = camera.pitch * 40.0f;  
    float light_factor = light_intensity / 255.0f;  
  
    float time = SDL_GetTicks() / 1000.0f;  
    static float cached_water_time = 0.0f;  
    static int water_frame_counter = 0;  
  
    if (water_frame_counter % 4 == 0) {  
        cached_water_time = time;  
    }  
    water_frame_counter++;  
  
    int camera_underwater = (camera.z < water_level);  
  
    if (camera_underwater) {  
        light_factor *= 0.7f;  
        background_color = SDL_MapRGBA(gPixelFormat,  
            sky_color_r * 0.5f, sky_color_g * 0.7f, sky_color_b * 1.0f, sky_color_a);  
        gr_clear_as(render_buffer, background_color);  
    }  
  
    if (!fog_table_initialized || fog_table_size != (int)max_render_distance) {  
        if (fog_table)  
            free(fog_table);  
        fog_table_size = (int)max_render_distance;  
        fog_table = malloc(fog_table_size * sizeof(float));  
  
        for (int i = 0; i < fog_table_size; i++) {  
            float fog = 1.0f - (i / (float)fog_table_size);  
            fog_table[i] = (fog < 0.6f) ? 0.6f : fog;  
        }  
        fog_table_initialized = 1;  
    }  
  
    for (int screen_x = 0; screen_x < 320; screen_x += quality_step) {  
        float angle = base_angle + screen_x * angle_step;  
  
        float camera_fov_half = terrain_fov * 0.5f;  
        float min_angle = camera.angle - camera_fov_half;  
        float max_angle = camera.angle + camera_fov_half;  
        if (angle < min_angle || angle > max_angle)  
            continue;  
  
        float cos_angle = cosf(angle);  
        float sin_angle = sinf(angle);  
        int lowest_y = 240;  
  
        for (float distance = 1.0f; distance < max_render_distance; distance += (distance < 50.0f ? 0.2f : distance < 200.0f ? 0.5f : 1.0f)) {  
            float world_x = camera.x + cos_angle * distance;  
            float world_y = camera.y + sin_angle * distance;  
  
            if (world_x < 0 || world_x >= hm->width - 1 || world_y < 0 || world_y >= hm->height - 1)  
                continue;  
  
            int current_chunk_x = (int)(world_x / chunk_size);  
            int current_chunk_y = (int)(world_y / chunk_size);  
            if (current_chunk_x < min_chunk_x || current_chunk_x > max_chunk_x ||  
                current_chunk_y < min_chunk_y || current_chunk_y > max_chunk_y) {  
                continue;  
            }  
  
            float terrain_height = get_height_at(hm, world_x, world_y);  
  
            float render_height = terrain_height;  
            int render_water = 0;  
            if (water_level > 0 && terrain_height < water_level) {  
                float simple_wave = sin(cached_water_time + world_x * 0.05f) * 0.1f;  
                render_height = water_level + simple_wave;  
                render_water = 1;  
            }  
  
            float height_on_screen = (camera.z - render_height) / distance * 300.0f + 120.0f;  
            height_on_screen += pitch_offset;  
  
            int screen_y = (int)height_on_screen;  
            if (screen_y < 0)  
                screen_y = 0;  
            if (screen_y >= 240)  
                continue;  
  
            if (screen_y < lowest_y) {  
                int fog_index = (int)distance;  
                if (fog_index >= fog_table_size)  
                    fog_index = fog_table_size - 1;  
                float fog = fog_table[fog_index];  
                uint32_t color;  
  
                Uint8 terrain_r = 0, terrain_g = 0, terrain_b = 0;  
                if (hm->texturemap) {  
                    uint32_t tex = get_texture_color_bilinear(hm->texturemap, world_x, world_y);  
                    if (tex == 0) {  
                        int tx = (int)world_x;  
                        int ty = (int)world_y;  
                        while (tx >= hm->texturemap->width)  
                            tx -= hm->texturemap->width;  
                        while (ty >= hm->texturemap->height)  
                            ty -= hm->texturemap->height;  
                        while (tx < 0)  
                            tx += hm->texturemap->width;  
                        while (ty < 0)  
                            ty += hm->texturemap->height;  
                        tex = gr_get_pixel(hm->texturemap, tx, ty);  
                    }  
                    SDL_GetRGB(tex, gPixelFormat, &terrain_r, &terrain_g, &terrain_b);  
                } else {  
                    int base = (int)(terrain_height * 2.5f) + 20;  
                    if (base > 255) base = 255;  
                    if (base < 0) base = 0;  
  
                    int grid_x = (int)(world_x * 2.0f) % 8;  
                    int grid_y = (int)(world_y * 2.0f) % 8;  
                    int grid_variation = (grid_x + grid_y) % 3 - 1;  
                    base += grid_variation * 15;  
                    if (base > 255) base = 255;  
                    if (base < 0) base = 0;  
  
                    terrain_r = (Uint8)((base + 60));  
                    terrain_g = (Uint8)((base + 30));  
                    terrain_b = (Uint8)(base);  
                }  
  
                float total_light = light_factor * fog;  
                terrain_r = (Uint8)(terrain_r * total_light);  
                terrain_g = (Uint8)(terrain_g * total_light);  
                terrain_b = (Uint8)(terrain_b * total_light);  
  
                if (!render_water) {  
                    color = SDL_MapRGB(gPixelFormat, terrain_r, terrain_g, terrain_b);  
                } else {  
                    // SISTEMA DE TEXTURAS PARA EL AGUA CORREGIDO - SIN DOBLE CAPA  
                    if (water_texture) {  
                        // Calcular coordenadas UV con animación  
                        float u = (world_x * water_texture_scale + water_texture_offset_x + cached_water_time * 0.1f);  
                        float v = (world_y * water_texture_scale + water_texture_offset_y + cached_water_time * 0.05f);  
                          
                        // Normalizar coordenadas UV  
                        u = u - floor(u);  
                        v = v - floor(v);  
                          
                        // Convertir a coordenadas de píxel  
                        int tex_x = (int)(u * water_texture->width);  
                        int tex_y = (int)(v * water_texture->height);  
                          
                        // Asegurar que estén dentro de los límites  
                        tex_x = tex_x % water_texture->width;  
                        tex_y = tex_y % water_texture->height;  
                        if (tex_x < 0) tex_x += water_texture->width;  
                        if (tex_y < 0) tex_y += water_texture->height;  
                          
                        // Obtener color de la textura  
                        uint32_t water_tex_color = gr_get_pixel(water_texture, tex_x, tex_y);  
                          
                        // Extraer componentes RGBA de la textura  
                        Uint8 tex_r, tex_g, tex_b, tex_a;  
                        SDL_GetRGBA(water_tex_color, gPixelFormat, &tex_r, &tex_g, &tex_b, &tex_a);  
                          
                        // Determinar alpha final  
                        float final_alpha;  
                        if (water_texture_alpha_override >= 0) {  
                            final_alpha = water_texture_alpha_override / 255.0f;  
                        } else if (tex_a == 255) {  
                            final_alpha = water_color_a / 255.0f;  
                        } else {  
                            float texture_alpha = tex_a / 255.0f;  
                            float global_alpha = water_color_a / 255.0f;  
                            final_alpha = texture_alpha * global_alpha;  
                        }  
                          
                        // CLAVE: Si el alpha es muy bajo, mostrar solo el terreno  
                        if (final_alpha < 0.01f) {  
                            color = SDL_MapRGB(gPixelFormat, terrain_r, terrain_g, terrain_b);  
                        } else {  
                            // Mezclar PRIMERO con el terreno usando transparencia  
                            tex_r = (Uint8)(tex_r * final_alpha + terrain_r * (1.0f - final_alpha));  
                            tex_g = (Uint8)(tex_g * final_alpha + terrain_g * (1.0f - final_alpha));  
                            tex_b = (Uint8)(tex_b * final_alpha + terrain_b * (1.0f - final_alpha));  
                              
                            // DESPUÉS aplicar iluminación al resultado mezclado  
                            tex_r = (Uint8)(tex_r * total_light);  
                            tex_g = (Uint8)(tex_g * total_light);  
                            tex_b = (Uint8)(tex_b * total_light);  
                              
                            color = SDL_MapRGB(gPixelFormat, tex_r, tex_g, tex_b);  
                        }  
                    } else {  
                        // Solo usar fallback si NO hay textura cargada  
                        Uint8 water_r = water_color_r;  
                        Uint8 water_g = water_color_g;  
                        Uint8 water_b = water_color_b;  
                          
                        float alpha_factor = water_color_a / 255.0f;  
                        water_r = (Uint8)(water_r * alpha_factor + terrain_r * (1.0f - alpha_factor));  
                        water_g = (Uint8)(water_g * alpha_factor + terrain_g * (1.0f - alpha_factor));  
                        water_b = (Uint8)(water_b * alpha_factor + terrain_b * (1.0f - alpha_factor));  
                          
                        water_r = (Uint8)(water_r * total_light);  
                        water_g = (Uint8)(water_g * total_light);  
                        water_b = (Uint8)(water_b * total_light);  
                          
                        color = SDL_MapRGB(gPixelFormat, water_r, water_g, water_b);  
                    }  
                }  
  
                for (int y = screen_y; y < lowest_y; y++) {  
                    gr_put_pixel(render_buffer, screen_x, y, color);  
                    int depth_index = y * 320 + screen_x;  
                    if (depth_index >= 0 && depth_index < 320 * 240) {  
                        depth_buffer[depth_index] = distance;  
                    }  
                }  
                lowest_y = screen_y;  
            }  
  
            if (lowest_y <= 0)  
                break;  
        }  
    }  
  
    // RENDERIZADO DE BILLBOARDS  
    for (int i = 0; i < billboard_count; i++) {  
        if (!voxel_billboards[i].active) continue;  
  
        VOXEL_BILLBOARD *bb = &voxel_billboards[i];  
  
        float dx = bb->world_x - camera.x;  
        float dy = bb->world_y - camera.y;  
        float distance = sqrtf(dx * dx + dy * dy);  
  
        if (distance > max_render_distance || distance < 0.1f) continue;  
  
        float angle_to_billboard = atan2f(dy, dx);  
        float angle_diff = angle_to_billboard - camera.angle;  
  
        while (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;  
        while (angle_diff < -M_PI) angle_diff += 2.0f * M_PI;  
  
        float fov_half = terrain_fov * 0.5f;  
        if (fabs(angle_diff) > fov_half) continue;  
  
        float pixels_per_radian = 320.0f / terrain_fov;  
        int screen_x = (int)(160 + (angle_diff * pixels_per_radian));  
  
        if (screen_x < 0 || screen_x >= 320) continue;  
  
        float height_on_screen = (camera.z - bb->world_z) / distance * 300.0f + 120.0f;  
        height_on_screen += camera.pitch * 40.0f;  
  
        int screen_y = (int)height_on_screen;  
        if (screen_y < 0 || screen_y >= 240) continue;  
  
        int depth_index = screen_y * 320 + screen_x;  
        if (depth_index >= 0 && depth_index < 320 * 240) {  
            if (distance >= depth_buffer[depth_index]) {  
                continue;  
            }  
        }  
  
        GRAPH *billboard_graph = bitmap_get(0, bb->graph_id);  
        if (billboard_graph) {  
            float scale = 50.0f / distance;  
            if (scale > 3.0f) scale = 3.0f;  
            if (scale < 0.1f) scale = 0.1f;  
  
            int fog_index = (int)distance;  
            if (fog_index >= fog_table_size) fog_index = fog_table_size - 1;  
            float fog = fog_table[fog_index];  
  
            Uint8 alpha = (Uint8)(255 * fog);  
  
            gr_blit(render_buffer, NULL, screen_x, screen_y, 0, 0,  
                   (int)(scale * 100), (int)(scale * 100),  
                   billboard_graph->width/2, billboard_graph->height/2,  
                   billboard_graph, NULL, 255, 255, 255, alpha, 0, NULL);  
        }  
    }  
  
    return render_buffer->code;  
}
/* Funciones auxiliares */
void build_height_cache(HEIGHTMAP *hm)
{
    if (!hm->heightmap)
        return;

    if (hm->height_cache)
    {
        free(hm->height_cache);
        hm->height_cache = NULL;
    }

    hm->height_cache = malloc(hm->width * hm->height * sizeof(float));
    if (!hm->height_cache)
    {
        hm->cache_valid = 0;
        return;
    }

    for (int y = 0; y < hm->height; y++)
    {
        for (int x = 0; x < hm->width; x++)
        {
            uint32_t pixel = gr_get_pixel(hm->heightmap, x, y);
            float height = (float)((pixel >> 16) & 0xFF);
            hm->height_cache[y * hm->width + x] = height;
        }
    }

    hm->cache_valid = 1;
}

/* Devuelve un color RGB interpolado usando funciones SDL */
uint32_t get_texture_color_bilinear(GRAPH *texture, float x, float y)
{
    if (!texture)
        return 0;

    int ix = (int)x;
    int iy = (int)y;

    if (ix < 0 || iy < 0 || ix >= texture->width - 1 || iy >= texture->height - 1)
        return 0;

    float fx = x - ix;
    float fy = y - iy;

    uint32_t c00 = gr_get_pixel(texture, ix, iy);
    uint32_t c10 = gr_get_pixel(texture, ix + 1, iy);
    uint32_t c01 = gr_get_pixel(texture, ix, iy + 1);
    uint32_t c11 = gr_get_pixel(texture, ix + 1, iy + 1);

    // Extraer componentes usando SDL_GetRGB
    Uint8 r00, g00, b00, r10, g10, b10, r01, g01, b01, r11, g11, b11;

    SDL_GetRGB(c00, gPixelFormat, &r00, &g00, &b00);
    SDL_GetRGB(c10, gPixelFormat, &r10, &g10, &b10);
    SDL_GetRGB(c01, gPixelFormat, &r01, &g01, &b01);
    SDL_GetRGB(c11, gPixelFormat, &r11, &g11, &b11);

    // Interpolación bilineal
    float r0 = r00 + fx * (r10 - r00);
    float r1 = r01 + fx * (r11 - r01);
    float rf = r0 + fy * (r1 - r0);

    float g0 = g00 + fx * (g10 - g00);
    float g1 = g01 + fx * (g11 - g01);
    float gf = g0 + fy * (g1 - g0);

    float b0 = b00 + fx * (b10 - b00);
    float b1 = b01 + fx * (b11 - b01);
    float bf = b0 + fy * (b1 - b0);

    // Reconstruir usando SDL_MapRGB
    return SDL_MapRGB(gPixelFormat, (Uint8)rf, (Uint8)gf, (Uint8)bf);
}

/* Función para mantener cámara sobre el terreno */
void clamp_camera_to_terrain(HEIGHTMAP *hm)
{
    if (!hm || !hm->cache_valid)
        return;

    // Obtener altura del terreno en posición de cámara
    float terrain_height = get_height_at(hm, camera.x, camera.y);

    // Altura mínima sobre el terreno (por ejemplo, 5 unidades)
    float min_height_above_terrain = 5.0f;

    // Ajustar Z de cámara si está muy cerca del terreno
    if (camera.z < terrain_height + min_height_above_terrain)
    {
        camera.z = terrain_height + min_height_above_terrain;
    }
}

/* Función para mantener cámara dentro de límites */
void clamp_camera_to_bounds(HEIGHTMAP *hm)
{
    if (!hm)
        return;

    // Margen desde los bordes
    float margin = 2.0f;

    // Limitar X
    if (camera.x < margin)
        camera.x = margin;
    if (camera.x >= hm->width - margin)
        camera.x = hm->width - margin - 0.1f;

    // Limitar Y
    if (camera.y < margin)
        camera.y = margin;
    if (camera.y >= hm->height - margin)
        camera.y = hm->height - margin - 0.1f;
}

/* Inicializar cámara en posición válida del terreno */
int64_t libmod_heightmap_init_camera_on_terrain(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 0;

    // Colocar en el centro del mapa con margen de seguridad
    float margin = 10.0f;
    float x = hm->width / 2.0f;
    float y = hm->height / 2.0f;

    if (x < margin)
        x = margin;
    if (x >= hm->width - margin)
        x = hm->width - margin - 1;
    if (y < margin)
        y = margin;
    if (y >= hm->height - margin)
        y = hm->height - margin - 1;

    camera.x = x;
    camera.y = y;

    // Obtener altura del terreno y situar cámara justo encima
    float terrain_height = get_height_at(hm, camera.x, camera.y);
    camera.z = terrain_height + 10.0f;

    // Ángulos iniciales
    camera.angle = 0.0f;
    camera.pitch = 0.0f;
    camera.fov = DEFAULT_FOV;

    return 1;
}

/* Configurar sensibilidad de controles */
int64_t libmod_heightmap_set_control_sensitivity(INSTANCE *my, int64_t *params)
{
    mouse_sensitivity = (float)params[0];
    move_speed = (float)params[1];
    height_speed = (float)params[2];
    return 1;
}

/* Movimiento hacia adelante */
int64_t libmod_heightmap_move_forward(INSTANCE *my, int64_t *params)
{
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;

    camera.x += cosf(camera.angle) * speed / 1.5f;
    camera.y += sinf(camera.angle) * speed / 1.5f;

    return 1;
}

/* Movimiento hacia atrás */
int64_t libmod_heightmap_move_backward(INSTANCE *my, int64_t *params)
{
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;

    camera.x -= cosf(camera.angle) * speed / 1.5f;
    camera.y -= sinf(camera.angle) * speed / 1.5f;

    return 1;
}

/* Movimiento lateral izquierda */
int64_t libmod_heightmap_strafe_left(INSTANCE *my, int64_t *params)
{
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;

    camera.x += cosf(camera.angle - M_PI_2) * speed / 2.0f;
    camera.y += sinf(camera.angle - M_PI_2) * speed / 2.0f;

    return 1;
}

/* Movimiento lateral derecha */
int64_t libmod_heightmap_strafe_right(INSTANCE *my, int64_t *params)
{
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;

    camera.x += cosf(camera.angle + M_PI_2) * speed / 2.0f;
    camera.y += sinf(camera.angle + M_PI_2) * speed / 2.0f;

    return 1;
}

/* Control de rotación horizontal */
int64_t libmod_heightmap_look_horizontal(INSTANCE *my, int64_t *params)
{
    float delta = (float)params[0];
    camera.angle += delta * mouse_sensitivity / 1000.0f;
    return 1;
}

/* Control de rotación vertical */
int64_t libmod_heightmap_look_vertical(INSTANCE *my, int64_t *params)
{
    float delta = (float)params[0];
    camera.pitch -= delta * mouse_sensitivity / 1000.0f;

    // Limitar pitch
    const float max_pitch = M_PI_2 * 0.99f;
    if (camera.pitch > max_pitch)
        camera.pitch = max_pitch;
    if (camera.pitch < -max_pitch)
        camera.pitch = -max_pitch;

    return 1;
}

/* Ajustar altura */
int64_t libmod_heightmap_adjust_height(INSTANCE *my, int64_t *params)
{
    float delta = (float)params[0];
    camera.z += delta * height_speed / 2.0f;
    if (camera.z < 20)
        camera.z = 20;
    return 1;
}

/* Crear heightmap procedural */
int64_t libmod_heightmap_create_procedural(INSTANCE *my, int64_t *params)
{
    int64_t width = params[0];
    int64_t height = params[1];

    GRAPH *graph = bitmap_new_syslib(width, height);
    if (!graph)
        return 0;

    // Generar directamente en el bitmap sin llamadas externas
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int color = 128 +
                        sin(x * 360 / 20) * 60 +
                        cos(y * 360 / 15) * 70 +
                        sin((x + y) * 360 / 12) * 40 +
                        sin(x * 360 / 6) * 25 +
                        cos(y * 360 / 8) * 30;

            if (rand() % 100 < 5)
            {
                color += rand() % 100 + 50;
            }

            if (color < 0)
                color = 0;
            if (color > 255)
                color = 255;

            gr_put_pixel(graph, x, y, (color << 16) | (color << 8) | color);
        }
    }

    // Crear entrada en el array de heightmaps
    int slot = -1;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (!heightmaps[i].heightmap)
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
        return 0;

    heightmaps[slot].id = next_heightmap_id++;
    heightmaps[slot].heightmap = graph;
    heightmaps[slot].width = width;
    heightmaps[slot].height = height;
    build_height_cache(&heightmaps[slot]);

    return heightmaps[slot].id;
}

/* Obtener posición actual de la cámara del módulo */
int64_t libmod_heightmap_get_camera_position(INSTANCE *my, int64_t *params)
{
    // Los parámetros son punteros a las variables del PRG
    int64_t *camera_x_ptr = (int64_t *)params[0];
    int64_t *camera_y_ptr = (int64_t *)params[1];
    int64_t *camera_z_ptr = (int64_t *)params[2];
    int64_t *camera_angle_ptr = (int64_t *)params[3];
    int64_t *camera_pitch_ptr = (int64_t *)params[4];

    // Actualizar las variables del PRG con los valores del módulo
    *camera_x_ptr = (int64_t)camera.x;
    *camera_y_ptr = (int64_t)camera.y;
    *camera_z_ptr = (int64_t)camera.z;
    *camera_angle_ptr = (int64_t)(camera.angle * 1000.0f);
    *camera_pitch_ptr = (int64_t)(camera.pitch * 1000.0f);

    return 1;
}

/* Verificar colisión con terreno y ajustar posición de cámara */
int64_t libmod_heightmap_check_terrain_collision(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 0;

    // Obtener altura del terreno en la posición actual de la cámara
    float terrain_height = get_height_at(hm, camera.x, camera.y);

    // Altura mínima sobre el terreno (altura del jugador)
    float min_height_above_terrain = 5.0f;

    // Si la cámara está por debajo del terreno + altura mínima, ajustar
    if (camera.z < terrain_height + min_height_above_terrain)
    {
        camera.z = terrain_height + min_height_above_terrain;
        return 1; // Colisión detectada y corregida
    }

    return 0; // Sin colisión
}

/* Movimiento con colisión - versión mejorada */
int64_t libmod_heightmap_move_forward_with_collision(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 0;

    // Calcular nueva posición
    float new_x = camera.x + cosf(camera.angle) * speed / 1.5f;
    float new_y = camera.y + sinf(camera.angle) * speed / 1.5f;

    // Verificar límites del mapa
    clamp_camera_to_bounds(hm);

    // Verificar si la nueva posición está dentro del mapa
    if (new_x >= 2.0f && new_x < hm->width - 2.0f &&
        new_y >= 2.0f && new_y < hm->height - 2.0f)
    {
        // Obtener altura del terreno en la nueva posición
        float terrain_height = get_height_at(hm, new_x, new_y);
        float min_height = terrain_height + 5.0f;

        // Solo mover si no hay colisión vertical extrema (pendiente muy pronunciada)
        float current_terrain_height = get_height_at(hm, camera.x, camera.y);
        float height_diff = fabs(terrain_height - current_terrain_height);

        // Permitir movimiento si la diferencia de altura no es muy grande
        if (height_diff < 20.0f) // Máxima pendiente permitida
        {
            camera.x = new_x;
            camera.y = new_y;

            // Ajustar altura de cámara al terreno
            if (camera.z < min_height)
            {
                camera.z = min_height;
            }
        }
    }

    return 1;
}

/* Versiones con colisión para todos los movimientos */
int64_t libmod_heightmap_move_backward_with_collision(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 0;

    float new_x = camera.x - cosf(camera.angle) * speed / 1.5f;
    float new_y = camera.y - sinf(camera.angle) * speed / 1.5f;

    if (new_x >= 2.0f && new_x < hm->width - 2.0f &&
        new_y >= 2.0f && new_y < hm->height - 2.0f)
    {
        float terrain_height = get_height_at(hm, new_x, new_y);
        float current_terrain_height = get_height_at(hm, camera.x, camera.y);
        float height_diff = fabs(terrain_height - current_terrain_height);

        if (height_diff < 20.0f)
        {
            camera.x = new_x;
            camera.y = new_y;

            float min_height = terrain_height + 5.0f;
            if (camera.z < min_height)
            {
                camera.z = min_height;
            }
        }
    }

    return 1;
}

int64_t libmod_heightmap_strafe_left_with_collision(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 0;

    float new_x = camera.x + cosf(camera.angle - M_PI_2) * speed / 2.0f;
    float new_y = camera.y + sinf(camera.angle - M_PI_2) * speed / 2.0f;

    if (new_x >= 2.0f && new_x < hm->width - 2.0f &&
        new_y >= 2.0f && new_y < hm->height - 2.0f)
    {
        float terrain_height = get_height_at(hm, new_x, new_y);
        float current_terrain_height = get_height_at(hm, camera.x, camera.y);
        float height_diff = fabs(terrain_height - current_terrain_height);

        if (height_diff < 20.0f)
        {
            camera.x = new_x;
            camera.y = new_y;

            float min_height = terrain_height + 5.0f;
            if (camera.z < min_height)
            {
                camera.z = min_height;
            }
        }
    }

    return 1;
}

int64_t libmod_heightmap_strafe_right_with_collision(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 0;

    float new_x = camera.x + cosf(camera.angle + M_PI_2) * speed / 2.0f;
    float new_y = camera.y + sinf(camera.angle + M_PI_2) * speed / 2.0f;

    if (new_x >= 2.0f && new_x < hm->width - 2.0f &&
        new_y >= 2.0f && new_y < hm->height - 2.0f)
    {
        float terrain_height = get_height_at(hm, new_x, new_y);
        float current_terrain_height = get_height_at(hm, camera.x, camera.y);
        float height_diff = fabs(terrain_height - current_terrain_height);

        if (height_diff < 20.0f)
        {
            camera.x = new_x;
            camera.y = new_y;

            float min_height = terrain_height + 5.0f;
            if (camera.z < min_height)
            {
                camera.z = min_height;
            }
        }
    }

    return 1;
}

/* Obtener altura del terreno en coordenadas de sprite */
int64_t libmod_heightmap_get_terrain_height_at_sprite(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    int64_t sprite_x = params[1];
    int64_t sprite_y = params[2];

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 0;

    // Convertir coordenadas de sprite a coordenadas del heightmap
    float world_x = (float)sprite_x / 10.0f; // Ajusta el factor según tu escala
    float world_y = (float)sprite_y / 10.0f;

    float terrain_height = get_height_at(hm, world_x, world_y);
    return (int64_t)(terrain_height * 10.0f); // Devolver en coordenadas de sprite
}

/* Verificar si un sprite puede moverse a una posición sin atravesar terreno */
int64_t libmod_heightmap_can_sprite_move_to(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    int64_t new_x = params[1];
    int64_t new_y = params[2];
    int64_t sprite_height = params[3]; // Altura del sprite sobre el suelo

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 1; // Permitir movimiento si no hay heightmap

    // Convertir a coordenadas del heightmap
    float world_x = (float)new_x / 10.0f;
    float world_y = (float)new_y / 10.0f;

    // Verificar límites del mapa
    if (world_x < 0 || world_x >= hm->width - 1 ||
        world_y < 0 || world_y >= hm->height - 1)
        return 0; // No permitir movimiento fuera del mapa

    float terrain_height = get_height_at(hm, world_x, world_y);
    float required_height = terrain_height + (float)sprite_height / 10.0f;

    // Verificar si hay suficiente espacio vertical
    // Puedes añadir lógica adicional aquí para verificar pendientes, etc.

    return 1; // Permitir movimiento
}

/* Ajustar posición Y de sprite según altura del terreno */  
/* Ajustar posición Z de sprite según altura del terreno */    
int64_t libmod_heightmap_adjust_sprite_to_terrain(INSTANCE *my, int64_t *params)    
{    
    int64_t hm_id = params[0];    
    int64_t sprite_x = params[1];    
    int64_t sprite_y = params[2];    
    int64_t *adjusted_z = (int64_t *)params[3]; // CAMBIO: ajustar Z, no Y  
    
    HEIGHTMAP *hm = NULL;    
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)    
    {    
        if (heightmaps[i].id == hm_id)    
        {    
            hm = &heightmaps[i];    
            break;    
        }    
    }    
    
    if (!hm || !hm->cache_valid)    
        return 0;    
    
    // CORRECCIÓN: Usar coordenadas directas para heightmap 1024x1024  
    float world_x = (float)sprite_x / 10.0f;    
    float world_y = (float)sprite_y / 10.0f;    
    
    // CORRECCIÓN: Verificación de límites más permisiva  
    if (world_x < 0 || world_x >= hm->width - 1 ||    
        world_y < 0 || world_y >= hm->height - 1)    
    {  
        // Si está fuera de límites, usar altura por defecto  
        *adjusted_z = 0;  
        return 0;  
    }  
    
    float terrain_height = get_height_at(hm, world_x, world_y);    
    
    // CORRECCIÓN: Ajustar Z para altura sobre terreno en coordenadas de sprite  
    *adjusted_z = (int64_t)(terrain_height * 10.0f);    
    
    return 1;    
}

/* Convertir coordenadas del mundo a coordenadas de pantalla */
int64_t libmod_heightmap_world_to_screen(INSTANCE *my, int64_t *params)    
{    
    float world_x = (float)params[0] / 10.0f;    
    float world_y = (float)params[1] / 10.0f;    
    float world_z = (float)params[2] / 10.0f;    
    int64_t *screen_x = (int64_t *)params[3];    
    int64_t *screen_y = (int64_t *)params[4];    
        
    // Vector desde cámara al objeto  
    float dx = world_x - camera.x;    
    float dy = world_y - camera.y;    
    float dz = world_z - camera.z;    
        
    // Calcular distancia 3D real  
    float distance_3d = sqrtf(dx * dx + dy * dy + dz * dz);  
      
    // Transformación de vista  
    float cos_a = cosf(camera.angle);    
    float sin_a = sinf(camera.angle);    
        
    float forward = dx * cos_a + dy * sin_a;    
    float right = -dx * sin_a + dy * cos_a;    
        
    // Verificar que está delante de la cámara  
    if (forward > 1.0f && forward < max_render_distance) {    
        // Proyección perspectiva CORREGIDA con escalado real  
        float perspective_factor = 100.0f / distance_3d; // Usar distancia 3D real  
          
        float projected_x = 160.0f + (right * perspective_factor);    
        float projected_y = 120.0f - (dz * perspective_factor) + (camera.pitch * 30.0f);    
            
        if (projected_x >= -100.0f && projected_x <= 420.0f &&    
            projected_y >= -100.0f && projected_y <= 340.0f) {    
                
            *screen_x = (int64_t)projected_x;    
            *screen_y = (int64_t)projected_y;    
            return 1;    
        }    
    }    
        
    return 0;    
}

/* Obtener iluminación del terreno para aplicar al sprite */
int64_t libmod_heightmap_get_terrain_lighting(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    int64_t sprite_x = params[1];
    int64_t sprite_y = params[2];

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 255; // Iluminación completa por defecto

    float world_x = (float)sprite_x / 10.0f;
    float world_y = (float)sprite_y / 10.0f;

    // Calcular distancia a la cámara para fog
    float dx = world_x - camera.x;
    float dy = world_y - camera.y;
    float distance = sqrtf(dx * dx + dy * dy);

    float fog = 1.0f - (distance / 800.0f);
    if (fog < 0.6f)
        fog = 0.6f;

    float total_light = (light_intensity / 255.0f) * fog;

    return (int64_t)(total_light * 255.0f);
}

/* Asignar sprite para que la cámara lo siga */
int64_t libmod_heightmap_set_camera_follow(INSTANCE *my, int64_t *params)
{
    camera_follow_sprite_id = params[0]; // ID del sprite a seguir (0 = desactivar)

    if (params[1] != -1)
        camera_follow_offset_x = (float)params[1] / 10.0f;
    if (params[2] != -1)
        camera_follow_offset_y = (float)params[2] / 10.0f;
    if (params[3] != -1)
        camera_follow_offset_z = (float)params[3] / 10.0f;
    if (params[4] != -1)
        camera_follow_speed = (float)params[4];

    return 1;
}

/* Actualizar posición de cámara siguiendo al sprite */
/* Versión simplificada usando variables globales del sprite */
int64_t libmod_heightmap_update_camera_follow(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    int64_t sprite_x = params[1]; // Pasar X del sprite como parámetro
    int64_t sprite_y = params[2]; // Pasar Y del sprite como parámetro

    if (camera_follow_sprite_id == 0)
        return 0;

    // Convertir coordenadas
    float world_x = (float)sprite_x / 10.0f;
    float world_y = (float)sprite_y / 10.0f;

    // Calcular posición objetivo
    float target_x = world_x + camera_follow_offset_x;
    float target_y = world_y + camera_follow_offset_y;

    // Resto de la lógica igual...

    return 1;
}
/* Obtener ID del sprite que sigue la cámara */
int64_t libmod_heightmap_get_camera_follow(INSTANCE *my, int64_t *params)
{
    return camera_follow_sprite_id;
}


int64_t libmod_heightmap_update_water_time(INSTANCE *my, int64_t *params)
{
    static uint32_t last_ticks = 0;
    uint32_t current_ticks = SDL_GetTicks();

    if (last_ticks == 0)
        last_ticks = current_ticks;

    float delta_time = (current_ticks - last_ticks) / 1000.0f;
    water_time += delta_time;

    if (water_time > 100.0f)
        water_time = 0.0f;
    last_ticks = current_ticks;
    return 1;
}

int64_t libmod_heightmap_set_sky_color(INSTANCE *my, int64_t *params)
{
    sky_color_r = (Uint8)params[0];
    sky_color_g = (Uint8)params[1];
    sky_color_b = (Uint8)params[2];
    sky_color_a = (Uint8)params[3];
    return 1;
}

// Nueva función para configurar distancia de dibujado
int64_t libmod_heightmap_set_render_distance(INSTANCE *my, int64_t *params)
{
    max_render_distance = (float)params[0];
    if (max_render_distance < 100.0f)
        max_render_distance = 100.0f;
    if (max_render_distance > 2000.0f)
        max_render_distance = 2000.0f;
    return 1;
}

// Nueva función para configurar chunks
int64_t libmod_heightmap_set_chunk_config(INSTANCE *my, int64_t *params)
{
    chunk_size = (int)params[0];
    chunk_radius = (int)params[1];

    if (chunk_size < 32)
        chunk_size = 32;
    if (chunk_size > 256)
        chunk_size = 256;
    if (chunk_radius < 2)
        chunk_radius = 2;
    if (chunk_radius > 10)
        chunk_radius = 10;

    return 1;
}

/* Marcar una instancia como billboard */  
int64_t libmod_heightmap_set_billboard(INSTANCE *my, int64_t *params)      
{      
    // Si no se pasa ID, usar la instancia actual    
    int64_t instance_id = (params[0] == 0) ? LOCQWORD(my, PROCESS_ID) : params[0];    
      
    // Usar la función nativa de BennuGD2 en lugar de iterar manualmente  
    INSTANCE *target = instance_get(instance_id);  
      
    if (target) {  
        LOCQWORD(target, CTYPE) = C_BILLBOARD;      
        printf("✅ Billboard marcado exitosamente - ID: %ld\n", instance_id);    
        return 1;    
    }  
      
    printf("❌ ERROR: No se encontró instancia con ID: %ld\n", instance_id);    
    return 0;      
}
  
int64_t libmod_heightmap_unset_billboard(INSTANCE *my, int64_t *params)  
{  
    int64_t instance_id = params[0];  
      
    extern INSTANCE *first_instance;  
    INSTANCE *current = first_instance;  
      
    while (current) {  
        // CORRECCIÓN: Usar LOCQWORD para acceder al PROCESS_ID.  
        if (LOCQWORD(current, PROCESS_ID) == instance_id) {  
            // Si se encuentra la instancia, se restablece su CTYPE a 0.  
            LOCQWORD(current, CTYPE) = 0;  
            return 1;  
        }  
        current = current->next;  
    }  
      
    return 0;  
}  
  
int64_t libmod_heightmap_is_billboard(INSTANCE *my, int64_t *params)  
{  
    int64_t instance_id = params[0];  
      
    extern INSTANCE *first_instance;  
    INSTANCE *current = first_instance;  
      
    while (current) {  
        // CORRECCIÓN: Usar LOCQWORD para acceder al PROCESS_ID.  
        if (LOCQWORD(current, PROCESS_ID) == instance_id) {  
            // Se verifica si el CTYPE de la instancia es C_BILLBOARD.  
            return (LOCQWORD(current, CTYPE) == C_BILLBOARD) ? 1 : 0;  
        }  
        current = current->next;  
    }  
      
    return 0;  
}

int64_t libmod_heightmap_auto_adjust_billboard_to_terrain(INSTANCE *my, int64_t *params)  
{  
    int64_t hm_id = params[0];  
    int64_t instance_id = params[1];  
      
    HEIGHTMAP *hm = NULL;  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == hm_id) {  
            hm = &heightmaps[i];  
            break;  
        }  
    }  
      
    if (!hm || !hm->cache_valid)  
        return 0;  
      
    // Encontrar la instancia  
    extern INSTANCE *first_instance;  
    INSTANCE *current = first_instance;  
      
    while (current) {  
        if (LOCQWORD(current, PROCESS_ID) == instance_id) {  
            float world_x = LOCDOUBLE(current, COORDX) / 10.0f;  
            float world_y = LOCDOUBLE(current, COORDY) / 10.0f;  
              
            float terrain_height = get_height_at(hm, world_x, world_y);  
              
            // Ajustar Z para que esté sobre el terreno  
            LOCDOUBLE(current, COORDZ) = terrain_height * 10.0f + 50.0f;  
              
            return 1;  
        }  
        current = current->next;  
    }  
      
    return 0;  
}

// Devuelve screen_x, screen_y y z_cam para una posición 3D
int64_t libmod_heightmap_project_billboard(INSTANCE *my, int64_t *params)  
{  
    int heightmap_id = params[0];  
    float wx = *(float*)&params[1]; // Convertir correctamente desde int64_t  
    float wy = *(float*)&params[2];  
    float wz = *(float*)&params[3];  
  
    if (heightmap_id < 0 || heightmap_id >= MAX_HEIGHTMAPS)  
        return 0;  
  
    // Diferencia respecto a la cámara  
    float dx = wx - camera.x;  
    float dy = wy - camera.y;  
  
    // Convertir ángulo a radianes y aplicar rotación inversa  
    float angle_rad = -camera.angle * M_PI / 180.0f;  
    float x_cam = dx * cosf(angle_rad) - dy * sinf(angle_rad);  
    float z_cam = dx * sinf(angle_rad) + dy * cosf(angle_rad);  
  
    // Rechazar objetos detrás de la cámara  
    if (z_cam < 1.0f)  
        return 0;  
  
    // Proyección en perspectiva  
    float fov_scale = 240.0f / tanf(camera.fov * 0.5f * M_PI / 180.0f);  
    int screen_x = (int)(160 + x_cam * fov_scale / z_cam);  // Centrado en 160 para 320px  
    int screen_y = (int)(120 - (wz - camera.z) * fov_scale / z_cam); // Centrado en 120 para 240px  
  
    // Verificar límites de pantalla  
    if (screen_x < -50 || screen_x > 370 || screen_y < -50 || screen_y > 290)  
        return 0;  
  
    // Empaquetar: screen_x (16 bits) | screen_y (16 bits) | z_cam como float (32 bits)  
    uint32_t z_as_uint = *(uint32_t*)&z_cam;  
    return (int64_t)(((uint64_t)(uint16_t)screen_x << 48) |  
                     ((uint64_t)(uint16_t)screen_y << 32) |  
                     ((uint64_t)z_as_uint));  
}

float libmod_heightmap_convert_screen_to_world_x(INSTANCE *my, int64_t *params)  
{  
    int heightmap_id = params[0];  
    float screen_x = *(float*)&params[1];  
      
    HEIGHTMAP *hm = NULL;  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == heightmap_id) {  
            hm = &heightmaps[i];  
            break;  
        }  
    }  
      
    if (!hm) return 0.0f;  
      
    // Factor automático basado en el tamaño del heightmap  
    float factor = hm->width / 640.0f; // Asumiendo pantalla de referencia 640px  
    return screen_x / factor;  
}  
  
float libmod_heightmap_convert_screen_to_world_y(INSTANCE *my, int64_t *params)  
{  
    int heightmap_id = params[0];  
    float screen_y = *(float*)&params[1];  
      
    HEIGHTMAP *hm = NULL;  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == heightmap_id) {  
            hm = &heightmaps[i];  
            break;  
        }  
    }  
      
    if (!hm) return 0.0f;  
      
    // Factor automático basado en el tamaño del heightmap  
    float factor = hm->height / 480.0f; // Asumiendo pantalla de referencia 480px  
    return screen_y / factor;  
}

// Función para añadir billboards directamente al sistema voxelspace  
int64_t libmod_heightmap_add_voxel_billboard(INSTANCE *my, int64_t *params) {  
    float world_x = *(float*)&params[0];  
    float world_y = *(float*)&params[1];   
    float height_offset = *(float*)&params[2]; // Ahora es un offset sobre el terreno  
    int graph_id = params[3];  
      
    // Obtener la altura del heightmap actual  
    HEIGHTMAP *hm = NULL;  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == current_heightmap_id) { // Asumiendo que tienes un current_heightmap_id global  
            hm = &heightmaps[i];  
            break;  
        }  
    }  
  
    float terrain_height = 0.0f;  
    if (hm) {  
        terrain_height = get_height_at(hm, world_x, world_y);  
    }  
      
    float final_world_z = terrain_height + height_offset; // Altura del terreno + offset  
      
    printf("=== DEBUG ADD BILLBOARD ===\n");  
    printf("Añadiendo billboard #%d\n", billboard_count);  
    printf("Coordenadas recibidas: x=%.2f, y=%.2f, offset_z=%.2f\n", world_x, world_y, height_offset);  
    printf("Altura terreno: %.2f, Z final: %.2f\n", terrain_height, final_world_z);  
    printf("Graph ID: %d\n", graph_id);  
      
    if (billboard_count < MAX_BILLBOARDS) {  
        voxel_billboards[billboard_count].world_x = world_x;  
        voxel_billboards[billboard_count].world_y = world_y;  
        voxel_billboards[billboard_count].world_z = final_world_z; // Usar la Z calculada  
        voxel_billboards[billboard_count].graph_id = graph_id;  
        voxel_billboards[billboard_count].scale = 1.0f;  
        voxel_billboards[billboard_count].active = 1;  
          
        printf("Billboard almacenado en posición: x=%.2f, y=%.2f, z=%.2f\n",   
               voxel_billboards[billboard_count].world_x,  
               voxel_billboards[billboard_count].world_y,  
               voxel_billboards[billboard_count].world_z);  
        printf("===========================\n");  
          
        return billboard_count++;  
    }  
    printf("ERROR: Límite de billboards alcanzado\n");  
    return -1;  
}

int64_t libmod_heightmap_water_texture(INSTANCE *my, int64_t *params) {  
    const char *texture_path = string_get(params[0]);  
    int alpha_override = params[1]; // Nuevo parámetro alpha (0-255)  
      
    water_texture_id = gr_load_img(texture_path);  
    string_discard(params[0]);  
      
    if (water_texture_id > 0) {  
        water_texture = bitmap_get(0, water_texture_id);  
          
        // Guardar el alpha override para usar en el renderizado  
        water_texture_alpha_override = alpha_override;  
          
        return 1;  
    }  
    return 0;  
}

// Función para calcular el color del terreno  
uint32_t calculate_terrain_color(HEIGHTMAP *hm, float world_x, float world_y, float terrain_height) {  
    Uint8 terrain_r = 0, terrain_g = 0, terrain_b = 0;  
      
    if (hm->texturemap) {  
        uint32_t tex = get_texture_color_bilinear(hm->texturemap, world_x, world_y);  
        if (tex == 0) {  
            int tx = (int)world_x;  
            int ty = (int)world_y;  
            while (tx >= hm->texturemap->width) tx -= hm->texturemap->width;  
            while (ty >= hm->texturemap->height) ty -= hm->texturemap->height;  
            while (tx < 0) tx += hm->texturemap->width;  
            while (ty < 0) ty += hm->texturemap->height;  
            tex = gr_get_pixel(hm->texturemap, tx, ty);  
        }  
        SDL_GetRGB(tex, gPixelFormat, &terrain_r, &terrain_g, &terrain_b);  
    } else {  
        int base = (int)(terrain_height * 2.5f) + 20;  
        if (base > 255) base = 255;  
        if (base < 0) base = 0;  
          
        int grid_x = (int)(world_x * 2.0f) % 8;  
        int grid_y = (int)(world_y * 2.0f) % 8;  
        int grid_variation = (grid_x + grid_y) % 3 - 1;  
        base += grid_variation * 15;  
        if (base > 255) base = 255;  
        if (base < 0) base = 0;  
          
        terrain_r = (Uint8)((base + 60));  
        terrain_g = (Uint8)((base + 30));  
        terrain_b = (Uint8)(base);  
    }  
      
    return SDL_MapRGB(gPixelFormat, terrain_r, terrain_g, terrain_b);  
}  
  

// Función mejorada de ruido simple para movimiento orgánico  
float simple_noise(float x, float y) {  
    int n = (int)(x + y * 57.0f);  
    n = (n << 13) ^ n;  
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);  
}  

void precalculate_water_transparency(HEIGHTMAP *hm) {  
    if (!precalc_water) {  
        precalc_water = malloc(hm->width * hm->height * sizeof(PRECALC_WATER_DATA));  
    }  
  
    for (int y = 0; y < hm->height; y++) {  
        for (int x = 0; x < hm->width; x++) {  
            int index = y * hm->width + x;  
            float terrain_height = get_height_at(hm, x, y);  
  
            if (water_level > 0 && terrain_height < water_level) {  
                precalc_water[index].has_water = 1;  
                precalc_water[index].water_depth = water_level - terrain_height;  
                // Eliminar cálculos de color - se harán con texturas  
            } else {  
                precalc_water[index].has_water = 0;  
            }  
        }  
    }  
}

uint32_t sample_water_texture(float world_x, float world_y, float time) {  
    if (!water_texture) {  
        // Fallback a color sólido si no hay textura  
        return SDL_MapRGB(gPixelFormat, water_color_r, water_color_g, water_color_b);  
    }  
      
    // Calcular coordenadas UV con animación  
    float u = (world_x * water_texture_scale + water_texture_offset_x + time * 0.1f);  
    float v = (world_y * water_texture_scale + water_texture_offset_y + time * 0.05f);  
      
    // Wrap UV coordinates  
    u = fmod(u, 1.0f);  
    v = fmod(v, 1.0f);  
    if (u < 0) u += 1.0f;  
    if (v < 0) v += 1.0f;  
      
    // Convertir a coordenadas de píxel  
    int tex_x = (int)(u * water_texture->width) % water_texture->width;  
    int tex_y = (int)(v * water_texture->height) % water_texture->height;  
      
    return gr_get_pixel(water_texture, tex_x, tex_y);  
}
#include "libmod_heightmap_exports.h"
