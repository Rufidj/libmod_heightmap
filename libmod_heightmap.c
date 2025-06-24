/*
 * Heightmap Module Implementation for BennuGD2 - Con soporte de luz, agua y color de cielo
 */

#include "libmod_heightmap.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

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
void __bgdexport(libmod_heightmap, module_initialize)()
{
    memset(heightmaps, 0, sizeof(heightmaps));
    next_heightmap_id = 1;
}

void __bgdexport(libmod_heightmap, module_finalize)()
{
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].heightmap)
        {
            if (heightmaps[i].height_cache)
            {
                free(heightmaps[i].height_cache);
                heightmaps[i].height_cache = NULL;
            }

            if (heightmaps[i].heightmap)
            {
                bitmap_destroy(heightmaps[i].heightmap);
                heightmaps[i].heightmap = NULL;
            }

            if (heightmaps[i].texturemap)
            {
                bitmap_destroy(heightmaps[i].texturemap);
                heightmaps[i].texturemap = NULL;
            }
        }
    }
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

int64_t libmod_heightmap_render_voxelspace(INSTANCE *my, int64_t *params)
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

    GRAPH *render_buffer = bitmap_new_syslib(320, 240);
    if (!render_buffer)
        return 0;

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

    float angle_step = 0.7f / 160.0f;
    float base_angle = camera.angle - 0.7f * 0.5f;
    float pitch_offset = camera.pitch * 40.0f;
    float light_factor = light_intensity / 255.0f;

    static float *fog_table = NULL;
    static int fog_table_size = 0;
    static int fog_table_initialized = 0;

    if (!fog_table_initialized || fog_table_size != (int)max_render_distance)
    {
        if (fog_table)
            free(fog_table);
        fog_table_size = (int)max_render_distance;
        fog_table = malloc(fog_table_size * sizeof(float));

        for (int i = 0; i < fog_table_size; i++)
        {
            float fog = 1.0f - (i / (float)fog_table_size);
            fog_table[i] = (fog < 0.6f) ? 0.6f : fog;
        }
        fog_table_initialized = 1;
    }

    for (int screen_x = 0; screen_x < 320; screen_x += quality_step)
    {
        float angle = base_angle + screen_x * angle_step;

        float camera_fov_half = camera.fov * 0.5f;
        float min_angle = camera.angle - camera_fov_half;
        float max_angle = camera.angle + camera_fov_half;
        if (angle < min_angle || angle > max_angle)
            continue;

        float cos_angle = cosf(angle);
        float sin_angle = sinf(angle);
        int lowest_y = 240;

        for (float distance = 1.0f; distance < max_render_distance; distance += (distance < 50.0f ? 0.1f : distance < 200.0f ? 0.25f
                                                                                                                             : 1.0f))
        {
            float world_x = camera.x + cos_angle * distance;
            float world_y = camera.y + sin_angle * distance;

            if (world_x < 0 || world_x >= hm->width - 1 || world_y < 0 || world_y >= hm->height - 1)
                continue;

            int current_chunk_x = (int)(world_x / chunk_size);
            int current_chunk_y = (int)(world_y / chunk_size);
            if (current_chunk_x < min_chunk_x || current_chunk_x > max_chunk_x ||
                current_chunk_y < min_chunk_y || current_chunk_y > max_chunk_y)
            {
                continue;
            }

            float terrain_height = get_height_at(hm, world_x, world_y);

            float render_height = terrain_height;
            int render_water = 0;
            if (water_level > 0 && terrain_height < water_level)
            {
                render_height = water_level;
                render_water = 1;
            }

            float height_on_screen = (camera.z - render_height) / distance * 300.0f + 120.0f;
            height_on_screen += pitch_offset;

            int screen_y = (int)height_on_screen;
            if (screen_y < 0)
                screen_y = 0;
            if (screen_y >= 240)
                continue;

            if (screen_y < lowest_y)
            {
                int fog_index = (int)distance;
                if (fog_index >= fog_table_size)
                    fog_index = fog_table_size - 1;
                float fog = fog_table[fog_index];
                uint32_t color;

                Uint8 terrain_r = 0, terrain_g = 0, terrain_b = 0;
                if (hm->texturemap)
                {
                    uint32_t tex = get_texture_color_bilinear(hm->texturemap, world_x, world_y);
                    if (tex == 0)
                    {
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
                }
                else
                {
                    int base = (int)(terrain_height * 2.5f) + 20;
                    if (base > 255)
                        base = 255;
                    if (base < 0)
                        base = 0;

                    int grid_x = (int)(world_x * 2.0f) % 8;
                    int grid_y = (int)(world_y * 2.0f) % 8;
                    int grid_variation = (grid_x + grid_y) % 3 - 1;
                    base += grid_variation * 15;
                    if (base > 255)
                        base = 255;
                    if (base < 0)
                        base = 0;

                    terrain_r = (Uint8)((base + 60));
                    terrain_g = (Uint8)((base + 30));
                    terrain_b = (Uint8)(base);
                }

                float total_light = light_factor * fog;
                terrain_r = (Uint8)(terrain_r * total_light);
                terrain_g = (Uint8)(terrain_g * total_light);
                terrain_b = (Uint8)(terrain_b * total_light);

                if (!render_water)
                {
                    color = SDL_MapRGB(gPixelFormat, terrain_r, terrain_g, terrain_b);
                }
                else
                {
                    // Agua azul pura - sin mezcla con terreno
                    Uint8 water_r = (Uint8)(water_color_r * total_light);
                    Uint8 water_g = (Uint8)(water_color_g * total_light);
                    Uint8 water_b = (Uint8)(water_color_b * total_light);

                    color = SDL_MapRGB(gPixelFormat, water_r, water_g, water_b);
                }

                for (int y = screen_y; y < lowest_y; y++)
                {
                    gr_put_pixel(render_buffer, screen_x, y, color);
                }
                lowest_y = screen_y;
            }

            if (lowest_y <= 0)
                break;
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
int64_t libmod_heightmap_adjust_sprite_to_terrain(INSTANCE *my, int64_t *params)  
{  
    int64_t hm_id = params[0];  
    int64_t sprite_x = params[1];  
    int64_t sprite_y = params[2];  
    int64_t *adjusted_y = (int64_t *)params[3]; // Puntero a variable Y del sprite  
  
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
  
    float world_x = (float)sprite_x / 10.0f;  
    float world_y = (float)sprite_y / 10.0f;  
  
    if (world_x < 0 || world_x >= hm->width - 1 ||  
        world_y < 0 || world_y >= hm->height - 1)  
        return 0;  
  
    float terrain_height = get_height_at(hm, world_x, world_y);  
  
    // Ajustar Y del sprite para que esté sobre el terreno  
    *adjusted_y = (int64_t)(terrain_height * 10.0f);  
  
    // Depuración: Altura del terreno y Y ajustada  
    printf("ASTT: World(%.2f, %.2f) TerrainH(%.2f) AdjustedY(%lld)\n", world_x, world_y, terrain_height, *adjusted_y); 
    fflush(stdout);
    return 1;  
}

/* Calcular escala de sprite basada en distancia a la cámara */  
int64_t libmod_heightmap_get_sprite_scale(INSTANCE *my, int64_t *params)  
{  
    int64_t sprite_x = params[0];  
    int64_t sprite_y = params[1];  
  
    // Convertir coordenadas de sprite a coordenadas del mundo  
    float world_x = (float)sprite_x / 10.0f;  
    float world_y = (float)sprite_y / 10.0f;  
  
    // Calcular distancia a la cámara  
    float dx = world_x - camera.x;  
    float dy = world_y - camera.y;  
    float distance = sqrtf(dx * dx + dy * dy);  
  
    // Escala basada en distancia (más lejos = más pequeño)  
    float scale = 300.0f / (distance + 50.0f); // Ajusta estos valores según necesites  
  
    // Limitar escala entre 10% y 200%  
    if (scale < 0.1f)  
        scale = 0.1f;  
    if (scale > 2.0f)  
        scale = 2.0f;  
  
    // Depuración: Distancia y escala  
    printf("GSS: World(%.2f, %.2f) Dist(%.2f) Scale(%.2f)\n", world_x, world_y, distance, scale);  
    fflush(stdout);
    return (int64_t)(scale * 100.0f); // Retornar como porcentaje  
}

/* Convertir coordenadas del mundo a coordenadas de pantalla */
int64_t libmod_heightmap_world_to_screen(INSTANCE *my, int64_t *params)    
{    
    float world_x = (float)params[0] / 10.0f;    
    float world_y = (float)params[1] / 10.0f;    
    float world_z = (float)params[2] / 10.0f;    
    int64_t *screen_x = (int64_t *)params[3];    
    int64_t *screen_y = (int64_t *)params[4];    
    
    // Calcular vector desde cámara al objeto  
    float dx = world_x - camera.x;    
    float dy = world_y - camera.y;    
    float dz = world_z - camera.z;    
    
    // Transformación de vista: rotar el mundo según la cámara  
    float cos_a = cosf(camera.angle);    
    float sin_a = sinf(camera.angle);    
    
    // Vista en primera persona correcta  
    float forward = dx * cos_a + dy * sin_a;   // Profundidad (Z en vista)  
    float right = -dx * sin_a + dy * cos_a;    // Lateral (X en vista)  
    
    // Solo proyectar si está delante de la cámara y a distancia razonable  
    if (forward > 0.5f && forward < 1000.0f)  
    {    
        // Proyección perspectiva con límites  
        float screen_scale = 200.0f / forward;  
          
        // Limitar el factor de escala para evitar proyecciones extremas  
        if (screen_scale > 10.0f) screen_scale = 10.0f;  
        if (screen_scale < 0.1f) screen_scale = 0.1f;  
          
        float projected_x = 160.0f + right * screen_scale;  
        float projected_y = 120.0f - dz * screen_scale + camera.pitch * 30.0f;  
          
        // Limitar coordenadas de pantalla para evitar valores extremos  
        if (projected_x < -50.0f) projected_x = -50.0f;  
        if (projected_x > 370.0f) projected_x = 370.0f;  
        if (projected_y < -50.0f) projected_y = -50.0f;  
        if (projected_y > 290.0f) projected_y = 290.0f;  
          
        *screen_x = (int64_t)projected_x;  
        *screen_y = (int64_t)projected_y;  
    
        return 1;  
    }    
    
    return 0; // No visible  
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

int64_t libmod_heightmap_set_water_color(INSTANCE *my, int64_t *params)
{
    water_color_r = (Uint8)params[0];
    water_color_g = (Uint8)params[1];
    water_color_b = (Uint8)params[2];
    water_color_a = (Uint8)params[3]; // Transparencia (0 = transparente, 255 = opaco)
    return 1;
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

#include "libmod_heightmap_exports.h"