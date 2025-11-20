/*
 * Heightmap Module Implementation for BennuGD2 - Con soporte de luz, agua y color de cielo
 */

#include "libmod_heightmap.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "libbggfx.h"  
#include <GL/glew.h>
#include <inttypes.h>  

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

#define MAX_BILLBOARDS 1000  
#define MAX_STATIC_BILLBOARDS 500  
#define MAX_DYNAMIC_BILLBOARDS 500  
  
typedef struct {    
    int active;    
    float world_x, world_y, world_z;    
    int graph_id;    
    int64_t process_id;  
    float scale;  
    int billboard_type;  // NUEVO CAMPO  
} VOXEL_BILLBOARD;

typedef struct {  
    uint32_t final_color;  // Color final con transparencia aplicada  
    uint8_t has_water;     // 1 si hay agua, 0 si no  
    float water_depth;     // Profundidad del agua para efectos  
} PRECALC_WATER_DATA;  

// Estructura temporal para ordenamiento de billboards  
typedef struct {  
    VOXEL_BILLBOARD *billboard;  
    BILLBOARD_PROJECTION projection;  
    GRAPH *graph;  
    float distance;  
} BILLBOARD_RENDER_DATA;
  

// Variables para textura del cielo  
static GRAPH *sky_texture = NULL;  
static float sky_texture_scale = 1.0f;  
static float sky_animation_speed = 0.3f;  
static float sky_texture_offset_x = 0.0f;  
static float sky_texture_offset_y = 0.0f;

// Variables globales para máscaras de overlay  
static GRAPH* overlay_mask = NULL;  
static GRAPH* bridge_texture = NULL;  
static float bridge_height_offset = 5.0f; // Altura adicional para puentes  


static PRECALC_WATER_DATA *precalc_water = NULL;
// Variables globales para el sistema de texturas de agua  
static GRAPH *water_texture = NULL;  
static int64_t water_texture_id = 0;  
static float water_texture_scale = 1.0f;  
static float water_texture_offset_x = 0.0f;  
static float water_texture_offset_y = 0.0f;  
static int water_texture_alpha_override = -1; // -1 = usar alpha de la textura, 0-255 = override
static float wave_amplitude = 0.1f; // Nueva variable para controlar el tamaño de las olas
  
static VOXEL_BILLBOARD static_billboards[MAX_STATIC_BILLBOARDS];  
static VOXEL_BILLBOARD dynamic_billboards[MAX_DYNAMIC_BILLBOARDS];  
static int static_billboard_count = 0;  
static int dynamic_billboard_count = 0;  

static int current_heightmap_id = 0; 
static float *fog_table = NULL;      
static int fog_table_size = 0;      
static int fog_table_initialized = 0;   

static float cached_fog_intensity = -1.0f;  
static Uint8 cached_fog_r = 0, cached_fog_g = 0, cached_fog_b = 0;

// Variables para el sistema de shaders de BennuGD2  
static BGD_SHADER *voxel_shader = NULL;  
static BGD_SHADER_PARAMETERS *voxel_params = NULL;  
static BGD_SHADER *sector_shader = NULL;  
static BGD_SHADER_PARAMETERS *sector_params = NULL;
static GRAPH *render_buffer = NULL;

#define BILLBOARD_TYPE_STATIC     0  
#define BILLBOARD_TYPE_PLAYER     1  
#define BILLBOARD_TYPE_ENEMY      2  
#define BILLBOARD_TYPE_PROJECTILE 3

#define C_BILLBOARD 999  
  
// Declaración forward corregida  
int64_t libmod_heightmap_world_to_screen(INSTANCE *my, int64_t *params);
uint32_t calculate_water_color(float world_x, float world_y, float water_depth, float time);
static float calculated_bridge_height = -1.0f;  

  
#ifndef M_PI_2
#define M_PI_2 1.57079632679f
#endif

#define RGBA32_R(color) ((color >> 16) & 0xFF)
#define RGBA32_G(color) ((color >> 8) & 0xFF)
#define RGBA32_B(color) (color & 0xFF)
#define RGBA32_A(color) ((color >> 24) & 0xFF)

// Variables globales para configuración de renderizado
static float max_render_distance = 8000.0f;
static int chunk_size = 512;
static int chunk_radius = 15;

// Variables globales para el color del cielo
static Uint8 sky_color_r = 135; // Azul cielo por defecto
static Uint8 sky_color_g = 206;
static Uint8 sky_color_b = 235;
static Uint8 sky_color_a = 255; // Alpha del cielo

// Variables para niebla mejorada  
static float fog_vertical_gradient = 0.5f;  
static Uint8 fog_color_r = 200;  
static Uint8 fog_color_g = 200;   
static Uint8 fog_color_b = 200;  
static float fog_intensity = 0.0f;


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

// fov para billboards
static float billboard_render_fov = 1.5f;  // FOV independiente para billboards


// Declaración por si no está en el header
void gr_alpha_put_pixel(GRAPH *dest, int x, int y, uint32_t color, uint8_t alpha);

HEIGHTMAP heightmaps[MAX_HEIGHTMAPS];
CAMERA_3D camera = {0, 0, 0, 0, 0, DEFAULT_FOV, DEFAULT_NEAR, DEFAULT_FAR};
int64_t next_heightmap_id = 1;
float water_level = -1.0f;
int light_intensity = 255;

// Dimensiones configurables del render buffer  
static int current_render_width = 320;  
static int current_render_height = 240;

//-------------------------------------
static float mouse_sensitivity = 50.0f;
static float move_speed = 3.0f;
static float height_speed = 3.0f;

float get_height_at(HEIGHTMAP *hm, float x, float y);
void build_height_cache(HEIGHTMAP *hm);
void clamp_camera_to_terrain(HEIGHTMAP *hm);
void clamp_camera_to_bounds(HEIGHTMAP *hm);
uint32_t get_texture_color_bilinear(GRAPH *texture, float x, float y);
int64_t libmod_heightmap_project_billboard(INSTANCE *my, int64_t *params);
static HEIGHTMAP* find_heightmap_by_id(int64_t hm_id);  
static int apply_terrain_collision(HEIGHTMAP *hm, float new_x, float new_y);  
static int move_camera_with_collision(int64_t hm_id, float angle_offset, float speed_factor, float speed);
static float convert_screen_to_world_coordinate(int heightmap_id, float screen_coord, int is_x_axis);
static void collect_visible_billboards_from_array(VOXEL_BILLBOARD *billboard_array, int array_size,   
                                                  BILLBOARD_RENDER_DATA *visible_billboards,   
                                                  int *visible_count, float terrain_fov);

static void cleanup_gpu_resources(void) {  
    // Limpiar shader de BennuGD2  
    if (voxel_shader) {  
        shader_free(voxel_shader);  
        voxel_shader = NULL;  
    }  
      
    // Limpiar parámetros del shader  
    if (voxel_params) {  
        shader_free_parameters(voxel_params);  
        voxel_params = NULL;  
    }  

    if (sector_shader) {  
    shader_free(sector_shader);  
    sector_shader = NULL;  
    }  
  
    if (sector_params) {  
    shader_free_parameters(sector_params);  
    sector_params = NULL;  
    }   
      
}
void libmod_heightmap_destroy_render_buffer() {  
    if (render_buffer) {  
        bitmap_destroy(render_buffer);  
        render_buffer = NULL;  
    }  
}

void __bgdexport(libmod_heightmap, module_initialize)()    
{    
    memset(heightmaps, 0, sizeof(heightmaps));    
    memset(static_billboards, 0, sizeof(static_billboards));    
    memset(dynamic_billboards, 0, sizeof(dynamic_billboards));    
    static_billboard_count = 0;    
    next_heightmap_id = 1;    
      
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {    
        heightmaps[i].id = 0;    
        heightmaps[i].type = MAP_TYPE_HEIGHTMAP;  
        heightmaps[i].heightmap = NULL;    
        heightmaps[i].texturemap = NULL;    
        heightmaps[i].height_cache = NULL;    
        heightmaps[i].cache_valid = 0;    
          
        // Inicializar campos de sectores legacy  
        heightmaps[i].sectors = NULL;    
        heightmaps[i].num_sectors = 0;    
        heightmaps[i].walls = NULL;    
        heightmaps[i].num_walls = 0;    
        heightmaps[i].textures = NULL;    
        heightmaps[i].num_textures = 0;  
          
        // NUEVO: Inicializar campos DMAP v2  
        heightmaps[i].vertices = NULL;  
        heightmaps[i].num_vertices = 0;  
        heightmaps[i].sidedefs = NULL;  
        heightmaps[i].num_sidedefs = 0;  
        heightmaps[i].things = NULL;  
        heightmaps[i].num_things = 0;  
        heightmaps[i].bsp_nodes = NULL;  
        heightmaps[i].num_bsp_nodes = 0;  
        heightmaps[i].subsectors = NULL;  
        heightmaps[i].num_subsectors = 0;  
        heightmaps[i].segs = NULL;  
        heightmaps[i].num_segs = 0;  
        heightmaps[i].blockmap_grid = NULL;  
        heightmaps[i].blockmap_width = 0;  
        heightmaps[i].blockmap_height = 0;  
        heightmaps[i].blockmap_cell_size = 0;  
    }    
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
        cleanup_gpu_resources();    
    }      
      
    // Liberar el render_buffer global una sola vez      
    libmod_heightmap_destroy_render_buffer();      
      
    // Liberar el fog_table global si existe      
    if (fog_table) {      
        free(fog_table);      
        fog_table = NULL;      
    }    
        
    // Liberar memoria de mapas de sectores (DMAP v1 y v2)    
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {      
        if (heightmaps[i].type == MAP_TYPE_SECTOR) {      
                
            // Determinar si es DMAP v1 (legacy) o v2    
            // Si hay vertices globales, es v2; si no, es v1    
            int is_dmap_v2 = (heightmaps[i].vertices != NULL);    
                
            if (is_dmap_v2) {    
                // DMAP v2: Liberar vertex_indices de cada sector    
                if (heightmaps[i].sectors) {      
                    for (int j = 0; j < heightmaps[i].num_sectors; j++) {    
                        SECTOR_V2 *sector = &((SECTOR_V2*)heightmaps[i].sectors)[j];    
                        if (sector->vertex_indices) {      
                            free(sector->vertex_indices);    
                            sector->vertex_indices = NULL;    
                        }      
                    }      
                    free(heightmaps[i].sectors);    
                    heightmaps[i].sectors = NULL;    
                }    
            } else {    
                // DMAP v1 legacy: Liberar vertices de cada sector    
                if (heightmaps[i].sectors) {      
                    for (int j = 0; j < heightmaps[i].num_sectors; j++) {    
                        SECTOR_LEGACY *sector = &((SECTOR_LEGACY*)heightmaps[i].sectors)[j];    
                        if (sector->vertices) {      
                            free(sector->vertices);    
                            sector->vertices = NULL;    
                        }      
                    }      
                    free(heightmaps[i].sectors);    
                    heightmaps[i].sectors = NULL;    
                }    
            }    
                  
            // Liberar paredes (común para v1 y v2)    
            if (heightmaps[i].walls) {      
                free(heightmaps[i].walls);    
                heightmaps[i].walls = NULL;    
            }      
                  
            // Liberar tabla de texturas (común para v1 y v2)    
            if (heightmaps[i].textures) {      
                free(heightmaps[i].textures);    
                heightmaps[i].textures = NULL;    
            }    
                
            // Liberar estructuras DMAP v2 adicionales    
            if (heightmaps[i].vertices) {    
                free(heightmaps[i].vertices);    
                heightmaps[i].vertices = NULL;    
            }    
  
            if (heightmaps[i].sidedefs) {    
                free(heightmaps[i].sidedefs);    
                heightmaps[i].sidedefs = NULL;    
            }    
  
            if (heightmaps[i].things) {    
                free(heightmaps[i].things);    
                heightmaps[i].things = NULL;    
            }    
  
            if (heightmaps[i].bsp_nodes) {    
                free(heightmaps[i].bsp_nodes);    
                heightmaps[i].bsp_nodes = NULL;    
            }    
  
            if (heightmaps[i].subsectors) {    
                free(heightmaps[i].subsectors);    
                heightmaps[i].subsectors = NULL;    
            }    
  
            if (heightmaps[i].segs) {    
                free(heightmaps[i].segs);    
                heightmaps[i].segs = NULL;    
            }    
  
            // CORREGIDO: Liberar blockmap correctamente  
            if (heightmaps[i].blockmap_grid) {  
                // Primero liberar wall_indices de cada celda  
                uint32_t blockmap_size = heightmaps[i].blockmap_width * heightmaps[i].blockmap_height;  
                for (uint32_t j = 0; j < blockmap_size; j++) {  
                    if (heightmaps[i].blockmap_grid[j].wall_indices) {  
                        free(heightmaps[i].blockmap_grid[j].wall_indices);  
                        heightmaps[i].blockmap_grid[j].wall_indices = NULL;  
                    }  
                }  
                // Luego liberar el grid completo  
                free(heightmaps[i].blockmap_grid);  
                heightmaps[i].blockmap_grid = NULL;  
            }  
        }      
    }    
        
    // Limpiar la estructura global heightmaps      
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

        if (slot == -1) {  
        fprintf(stderr, "Error: MAX_HEIGHTMAPS (%d) alcanzado\n", MAX_HEIGHTMAPS);  
        return 0;  
    }  
      
    // Verificar que next_heightmap_id no desborde  
    if (next_heightmap_id >= INT64_MAX - 1) {  
        fprintf(stderr, "Error: next_heightmap_id overflow\n");  
        return 0;  
    }

    heightmaps[slot].id = next_heightmap_id++;
    heightmaps[slot].type = MAP_TYPE_HEIGHTMAP;  // NUEVO: Inicializar como heightmap
    heightmaps[slot].heightmap = graph;
    heightmaps[slot].texturemap = NULL;
    heightmaps[slot].width = graph->width;
    heightmaps[slot].height = graph->height;
    heightmaps[slot].height_cache = NULL;
    heightmaps[slot].cache_valid = 0;
    
    // NUEVO: Inicializar campos de sectores como NULL  
    heightmaps[slot].sectors = NULL;  
    heightmaps[slot].num_sectors = 0;  
    heightmaps[slot].walls = NULL;  
    heightmaps[slot].num_walls = 0;  
    heightmaps[slot].textures = NULL;  
    heightmaps[slot].num_textures = 0;  
  
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

       if (slot == -1) {  
        fprintf(stderr, "Error: MAX_HEIGHTMAPS (%d) alcanzado\n", MAX_HEIGHTMAPS);  
        return 0;  
    }  
      
    // Verificar que next_heightmap_id no desborde  
    if (next_heightmap_id >= INT64_MAX - 1) {  
        fprintf(stderr, "Error: next_heightmap_id overflow\n");  
        return 0;  
    }

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

  HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
if (hm) {  
    float height = get_height_at(hm, x, y);  
    return (int64_t)(height * 1000);  
}  
return 0;
}

float get_height_at(HEIGHTMAP *hm, float x, float y) {    
    // Código original para heightmaps tradicionales    
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
    fprintf(stderr, "DEBUG SET_CAMERA: params[0]=%lld params[1]=%lld params[2]=%lld\n",   
            params[0], params[1], params[2]);  
      
    camera.x = (float)params[0];  
    camera.y = (float)params[1];  
    camera.z = (float)params[2];  
    camera.angle = (float)params[3] / 1000.0f;  
    camera.pitch = (float)params[4] / 1000.0f;  
    camera.fov = (params[5] > 0) ? (float)params[5] : DEFAULT_FOV;  
      
    fprintf(stderr, "DEBUG SET_CAMERA: camera.x=%.2f camera.y=%.2f camera.z=%.2f\n",  
            camera.x, camera.y, camera.z);  
  
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

int64_t libmod_heightmap_set_sky_texture(INSTANCE *my, int64_t *params)  
{  
    const char *filename = string_get(params[0]);  
    float scale = (params[1] > 0) ? (float)params[1] / 1000.0f : 1.0f;  
      
    if (sky_texture) {  
        bitmap_destroy(sky_texture);  
        sky_texture = NULL;  
    }  
      
    int64_t map_id = gr_load_img(filename);  
    string_discard(params[0]);  
    if (!map_id) return 0;  
      
    sky_texture = bitmap_get(0, map_id);  
    if (!sky_texture) return 0;  
      
    sky_texture_scale = scale;  
    return 1;  
}
 
// Función auxiliar para samplear la textura del cielo con proyección esférica corregida  
static uint32_t sample_sky_texture(float screen_x, float screen_y, float camera_angle, float camera_pitch, float time) {      
    if (!sky_texture) {      
        return SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, sky_color_a);      
    }      
          
    float terrain_fov = 0.1f;    
    float fov_horizontal = terrain_fov;      
    float fov_vertical = terrain_fov * 0.75f;    
      
    // CORREGIDO: Usar dimensiones dinámicas  
    float half_width = current_render_width / 2.0f;  
    float half_height = current_render_height / 2.0f;  
      
    float ray_angle_h = ((screen_x - half_width) / (float)current_render_width) * fov_horizontal;      
    float ray_angle_v = ((screen_y - half_height) / (float)current_render_height) * fov_vertical;  
          
    float world_angle_h = camera_angle + ray_angle_h;      
    float world_angle_v = camera_pitch + ray_angle_v;      
          
    float u = (world_angle_h + M_PI) / (2.0f * M_PI);      
    float v = (world_angle_v + M_PI_2) / M_PI;      
          
    if (u < 0.0f) u = 0.0f;      
    if (u > 1.0f) u = 1.0f;      
    if (v < 0.0f) v = 0.0f;      
    if (v > 1.0f) v = 1.0f;      
          
    int tex_x = (int)(u * sky_texture->width) % sky_texture->width;      
    int tex_y = (int)(v * sky_texture->height) % sky_texture->height;      
          
    return gr_get_pixel(sky_texture, tex_x, tex_y);      
}


// Función de renderizado del skybox corregida  
static void render_skybox(float camera_angle, float camera_pitch, float time, int quality_step) {    
    if (!sky_texture) {    
        uint32_t background_color = SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, sky_color_a);    
        gr_clear_as(render_buffer, background_color);    
        return;    
    }    
        
    uint32_t background_color = SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, sky_color_a);    
    gr_clear_as(render_buffer, background_color);    
      
    // CORREGIDO: Usar dimensiones dinámicas  
    for (int y = 0; y < current_render_height; y += quality_step) {    
        for (int x = 0; x < current_render_width; x += quality_step) {    
            uint32_t sky_color = sample_sky_texture((float)x, (float)y, camera_angle, camera_pitch, time);    
                
            for (int dy = 0; dy < quality_step && (y + dy) < current_render_height; dy++) {    
                for (int dx = 0; dx < quality_step && (x + dx) < current_render_width; dx++) {    
                    gr_put_pixel(render_buffer, x + dx, y + dy, sky_color);    
                }    
            }    
        }    
    }    
}

static BILLBOARD_PROJECTION calculate_proyection(VOXEL_BILLBOARD *bb, GRAPH *billboard_graph, float terrain_fov) {        
    BILLBOARD_PROJECTION result = {0};        
            
    float dx = bb->world_x - camera.x;        
    float dy = bb->world_y - camera.y;        
    float dz = bb->world_z - camera.z;        
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);        
            
    if (distance > max_render_distance * 1.1f || distance < 0.5f) {        
        result.valid = 0;        
        return result;        
    }        
            
    result.distance = distance;      
          
    float effective_fov = terrain_fov;      
    float billboard_angle = atan2f(dy, dx);      
    float angle_diff = billboard_angle - camera.angle;      
          
    while (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;      
    while (angle_diff < -M_PI) angle_diff += 2.0f * M_PI;      
          
    float terrain_fov_half = effective_fov * 0.5f;      
          
    if (fabs(angle_diff) > terrain_fov_half * 1.2f) {      
        result.valid = 0;      
        return result;      
    }      
        
    float half_width = current_render_width / 2.0f;    
    float screen_x_float = half_width + (angle_diff / effective_fov) * (float)current_render_width;      
        
    float extended_width = current_render_width * 1.25f;    
    if (screen_x_float < -extended_width || screen_x_float >= current_render_width + extended_width) {      
        result.valid = 0;      
        return result;      
    }      
          
    result.screen_x = (int)screen_x_float;      
          
    float cos_angle = cosf(camera.angle);        
    float sin_angle = sinf(camera.angle);        
    float cam_forward = dx * cos_angle + dy * sin_angle;      
          
    if (cam_forward <= 0.1f) {        
        result.valid = 0;        
        return result;        
    }      
        
    float half_height = current_render_height / 2.0f;    
    float height_on_screen = half_height + (camera.z - bb->world_z) / cam_forward * 300.0f;      
    height_on_screen += camera.pitch * 40.0f;      
        
    float extended_height = current_render_height * 1.67f;    
    if (height_on_screen < -extended_height || height_on_screen >= current_render_height + extended_height) {      
        result.valid = 0;      
        return result;      
    }      
          
    result.screen_y = (int)height_on_screen;      
          
    // Escalado según tipo de billboard    
    float base_scale_factor;      
    float max_scale, min_scale;      
          
    switch(bb->billboard_type) {      
        case BILLBOARD_TYPE_PLAYER:      
            base_scale_factor = 30.0f;      
            max_scale = 1.2f;      
            min_scale = 0.3f;      
            break;      
        case BILLBOARD_TYPE_ENEMY:      
            base_scale_factor = 120.0f;      
            max_scale = 6.0f;      
            min_scale = 0.1f;      
            break;      
        case BILLBOARD_TYPE_PROJECTILE:      
            base_scale_factor = 80.0f;      
            max_scale = 4.0f;      
            min_scale = 0.05f;      
            break;      
        default:      
            base_scale_factor = 150.0f;      
            max_scale = 8.0f;      
            min_scale = 0.05f;      
    }      
          
    result.distance_scale = base_scale_factor / cam_forward;      
    if (result.distance_scale > max_scale) result.distance_scale = max_scale;      
    if (result.distance_scale < min_scale) result.distance_scale = min_scale;      
          
    result.scaled_width = (int)(result.distance_scale * billboard_graph->width);      
    result.scaled_height = (int)(result.distance_scale * billboard_graph->height);      
          
    if (result.scaled_width < 1) result.scaled_width = 1;      
    if (result.scaled_height < 1) result.scaled_height = 1;      
          
    // ============================================================  
    // CORRECCIÓN: Cálculo de alpha mejorado  
    // ============================================================  
    float fog = 1.0f;  // Empezar con opacidad completa  
      
    // Solo aplicar fog_table si la distancia es significativa  
    if (distance > max_render_distance * 0.3f) {  
        int fog_index = (int)distance;      
        if (fog_index >= fog_table_size) fog_index = fog_table_size - 1;      
        fog = fog_table[fog_index];      
    }  
          
    // Fade-out por distancia (solo para objetos lejanos)  
    if (distance > max_render_distance * 0.7f) {      
        float fade_range = max_render_distance * 0.3f;    
        float fade_progress = (distance - max_render_distance * 0.7f) / fade_range;      
        float distance_fade = 1.0f - fade_progress;      
        distance_fade = fmaxf(0.0f, fminf(1.0f, distance_fade));      
        fog *= distance_fade;      
    }      
          
    // Fade-out por FOV (bordes del campo de visión)  
    if (fabs(angle_diff) > terrain_fov_half * 0.7f) {      
        float fov_fade_range = terrain_fov_half * 0.3f;      
        float fov_fade_progress = (fabs(angle_diff) - terrain_fov_half * 0.7f) / fov_fade_range;      
        float fov_fade = 1.0f - fov_fade_progress;      
        fov_fade = fmaxf(0.0f, fminf(1.0f, fov_fade));      
        fog *= fov_fade;      
    }      
          
    result.alpha = (Uint8)(255 * fog);      
        
    // Tintado de niebla (solo para objetos lejanos con niebla activa)  
    if (fog_intensity > 0.0f && distance > max_render_distance * 0.3f) {      
        float fog_start = max_render_distance * 0.3f;      
        float fog_range = max_render_distance - fog_start;      
        float fog_progress = (distance - fog_start) / fog_range;      
        fog_progress = fog_progress * fog_progress;      
              
        float fog_tint_factor = fog_progress * fog_intensity * 0.3f;      
        if (fog_tint_factor > 0.5f) fog_tint_factor = 0.5f;      
              
        result.fog_tint_factor = fog_tint_factor;      
    } else {      
        result.fog_tint_factor = 0.0f;      
    }    
          
    result.valid = 1;      
    return result;      
}

// Función de comparación para qsort (más lejanos primero)  
int compare_billboards_by_distance(const void *a, const void *b) {  
    BILLBOARD_RENDER_DATA *bb_a = (BILLBOARD_RENDER_DATA *)a;  
    BILLBOARD_RENDER_DATA *bb_b = (BILLBOARD_RENDER_DATA *)b;  
      
    // Renderizar primero los más lejanos (mayor distancia)  
    if (bb_a->distance > bb_b->distance) return -1;  
    if (bb_a->distance < bb_b->distance) return 1;  
    return 0;  
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
        render_buffer = bitmap_new_syslib(160, 120);    
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
        
    static float last_camera_x = 0, last_camera_y = 0, last_camera_angle = 0;    
    float movement = fabs(camera.x - last_camera_x) + fabs(camera.y - last_camera_y) + fabs(camera.angle - last_camera_angle);    
    int quality_step = (movement > 15.0f) ? 2 : 1;  // Umbral más alto
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
    render_skybox(camera.angle, camera.pitch, cached_water_time, quality_step);  
      
   
        
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
        // Solo aplicar mínimo si fog_intensity > 0  
        if (fog_intensity > 0.0f) {  
        fog_table[i] = (fog < 0.6f) ? 0.6f : fog;  
        } else {  
        fog_table[i] = fog;  // Sin mínimo cuando fog_intensity = 0  
    }  
}
        fog_table_initialized = 1;    
    }    
     
        // NUEVO: Precalcular cos/sin para todas las columnas  
        static float cos_cache[320];  
        static float sin_cache[320];  
        for (int i = 0; i < 320; i++) {  
        float angle = base_angle + i * angle_step;  
        cos_cache[i] = cosf(angle);  
        sin_cache[i] = sinf(angle);  
    }
        // Precalcular límites de FOV (ANTES del loop)  
        float camera_fov_half = terrain_fov * 0.5f;      
        float min_angle = camera.angle - camera_fov_half;      
        float max_angle = camera.angle + camera_fov_half;  
  
        for (int screen_x = 0; screen_x < 320; screen_x += quality_step) {      
        float angle = base_angle + screen_x * angle_step;      
          
        if (angle < min_angle || angle > max_angle)      
        continue;      
          
        float cos_angle = cos_cache[screen_x];  
        float sin_angle = sin_cache[screen_x];  
        int lowest_y = 240;    
            
        for (float distance = 1.0f; distance < max_render_distance;   
        distance += (distance < 50.0f ? 0.3f : distance < 200.0f ? 0.8f : 1.5f)){    
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
              
            // Renderizar terreno/agua según su altura real    
            float render_height;    
            int render_water = 0;    
                
            if (water_level > 0 && terrain_height < water_level) {  
            // Aproximación simple del ruido para CPU (usando múltiples ondas)  
            float wave1 = sin(cached_water_time * 0.5f + world_x * 0.05f) * wave_amplitude * 0.5f;  
            float wave2 = sin(cached_water_time * 0.8f + world_y * 0.03f) * wave_amplitude * 0.3f;  
            float wave3 = sin(cached_water_time * 1.2f + (world_x + world_y) * 0.02f) * wave_amplitude * 0.2f;  
            float simple_wave = wave1 + wave2 + wave3;  
      
    render_height = water_level + simple_wave;  
    render_water = 1; 
            } else {    
                
                render_height = terrain_height;    
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
                    
                if (render_water) {    
                    // Renderizar agua (sin cambios)  
                if (water_texture && water_texture->width > 0 && water_texture->height > 0) {  
                float u = (world_x * 0.01f + cached_water_time * 0.1f);  
                float v = (world_y * 0.01f + cached_water_time * 0.05f);  
      
                u = u - floor(u);  
                v = v - floor(v);  
      
                int tex_x = (int)(u * water_texture->width);  
                int tex_y = (int)(v * water_texture->height);  
      
                 // Clamp en lugar de modulo para evitar valores negativos  
                tex_x = (tex_x < 0) ? 0 : (tex_x >= water_texture->width) ? water_texture->width - 1 : tex_x;  
                tex_y = (tex_y < 0) ? 0 : (tex_y >= water_texture->height) ? water_texture->height - 1 : tex_y;   
                            
                      //                        uint32_t water_color = gr_get_pixel(water_texture, tex_x, tex_y);
//
                        BGD_Rect clip;
                        clip.x = tex_x;
                        clip.y = tex_y;
                        clip.w = 1;
                        clip.h = 1;

//                        {
//                            uint8_t r, g, b;
//                            SDL_GetRGB(water_color, gPixelFormat, &r, &g, &b);
//                            water_color = SDL_MapRGBA(gPixelFormat, r, g, b, 128);
//                        }

                        gr_blit(render_buffer, NULL,
                               screen_x, screen_y,
                               0,
                               0,
                               100, 100*(lowest_y-screen_y+1),
                               0, 0,
                               water_texture, &clip, 128, 255, 255, 255, BLEND_NORMAL, NULL);

                        for (int y = screen_y; y < lowest_y; y++) {
//                            gr_put_pixel(render_buffer, screen_x, y, water_color);
                            int depth_index = y * 320 + screen_x;
                            if (depth_index >= 0 && depth_index < 320 * 240) {
                                depth_buffer[depth_index] = distance;
                            }
                        } 
                    }
                } else {    
                    // Renderizar terreno normal con efectos atmosféricos avanzados  
                    GRAPH* texture_to_use = hm->texturemap;    
                    Uint8 terrain_r = 0, terrain_g = 0, terrain_b = 0;    
                        
                    if (texture_to_use) {    
                        uint32_t tex = get_texture_color_bilinear(texture_to_use, world_x, world_y);    
                        if (tex == 0) {    
                            int tx = (int)world_x;    
                            int ty = (int)world_y;    
                            while (tx >= texture_to_use->width)    
                                tx -= texture_to_use->width;    
                            while (ty >= texture_to_use->height)    
                                ty -= texture_to_use->height;    
                            while (tx < 0)    
                                tx += texture_to_use->width;    
                            while (ty < 0)    
                                ty += texture_to_use->height;    
                            tex = gr_get_pixel(texture_to_use, tx, ty);    
                        }    

                    // DESPUÉS (versión correcta):  
                    terrain_r = (tex >> gPixelFormat->Rshift) & 0xFF;  
                    terrain_g = (tex >> gPixelFormat->Gshift) & 0xFF;  
                    terrain_b = (tex >> gPixelFormat->Bshift) & 0xFF;

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
                        
                 uint32_t terrain_color = SDL_MapRGB(gPixelFormat, terrain_r, terrain_g, terrain_b);  
// NIEBLA FORZADA - después de la línea 946  
if (fog_intensity > 0.0f) {  
    // Aplicar niebla desde distancias más cercanas  
    if (distance > max_render_distance * 0.2f) {  
        float fog_progress = (distance - max_render_distance * 0.2f) / (max_render_distance * 0.8f);  
          
        // Factor de niebla MUY agresivo para que se vea  
        float fog_factor = fog_progress * fog_intensity * 2.0f; // Multiplicador x2  
          
        if (fog_factor > 0.9f) fog_factor = 0.9f; // Permitir hasta 90% de niebla  
          
        if (fog_factor > 0.1f) { // Aplicar desde 10% en adelante  
            float terrain_factor = 1.0f - fog_factor;  
              
            Uint8 final_r = (Uint8)(terrain_r * terrain_factor + fog_color_r * fog_factor);  
            Uint8 final_g = (Uint8)(terrain_g * terrain_factor + fog_color_g * fog_factor);  
            Uint8 final_b = (Uint8)(terrain_b * terrain_factor + fog_color_b * fog_factor);  
              
            terrain_color = SDL_MapRGB(gPixelFormat, final_r, final_g, final_b);  
        }  
    }  
}
                    for (int y = screen_y; y < lowest_y; y++) {    
                        gr_put_pixel(render_buffer, screen_x, y, terrain_color);    
                        int depth_index = y * 320 + screen_x;    
                        if (depth_index >= 0 && depth_index < 320 * 240) {    
                            depth_buffer[depth_index] = distance;    
                        }    
                    }    
                }    
                    
                lowest_y = screen_y;    
            }    
            if (lowest_y <= 0)    
                break;    
        }    
    }    
        
        
// Array temporal para todos los billboards visibles    
BILLBOARD_RENDER_DATA visible_billboards[MAX_STATIC_BILLBOARDS + MAX_DYNAMIC_BILLBOARDS];    
int visible_count = 0;    
    
// PASO 4A: Recopilar billboards estáticos visibles    
collect_visible_billboards_from_array(static_billboards, static_billboard_count,   
                                     visible_billboards, &visible_count, terrain_fov); 
    
// PASO 4B: Recopilar billboards dinámicos visibles    
collect_visible_billboards_from_array(dynamic_billboards, MAX_DYNAMIC_BILLBOARDS,   
                                     visible_billboards, &visible_count, terrain_fov);
    
// PASO 4C: Ordenar por distancia (más lejanos primero)    
qsort(visible_billboards, visible_count, sizeof(BILLBOARD_RENDER_DATA), compare_billboards_by_distance);    
    
// PASO 4D: Renderizar en orden correcto con fade-out mejorado  
for (int i = 0; i < visible_count; i++) {    
    BILLBOARD_RENDER_DATA *render_data = &visible_billboards[i];    
    BILLBOARD_PROJECTION proj = render_data->projection;    
        
    // Aplicar la verificación de oclusión mejorada    
    int billboard_visible = 1;    
    int center_x = proj.screen_x;    
    int center_y = proj.screen_y;    
    int half_width = proj.scaled_width / 2;    
    int half_height = proj.scaled_height / 2;    
        
    float depth_tolerance = 2.0f;    
    int occlusion_samples = 0;    
    int total_samples = 0;    
    
    for (int check_y = center_y - half_height; check_y <= center_y + half_height; check_y += 8) {    
        for (int check_x = center_x - half_width; check_x <= center_x + half_width; check_x += 8) {    
            if (check_x >= 0 && check_x < 320 && check_y >= 0 && check_y < 240) {    
                int depth_index = check_y * 320 + check_x;    
                total_samples++;    
                if (proj.distance > depth_buffer[depth_index] + depth_tolerance) {    
                    occlusion_samples++;    
                }    
            }    
        }    
    }    
        
    if (total_samples > 0 && (float)occlusion_samples / total_samples > 0.75f) {    
        billboard_visible = 0;    
    }    
    
    if (!billboard_visible) continue;    
    
    // Renderizar billboard con alpha mejorado (incluye fade-out)  
    gr_blit(render_buffer, NULL,    
           proj.screen_x - proj.scaled_width/2,    
           proj.screen_y - proj.scaled_height/2,    
           0, 0, proj.scaled_width, proj.scaled_height,    
           render_data->graph->width/2, render_data->graph->height/2,    
           render_data->graph, NULL, 255, 255, 255, proj.alpha, 0, NULL);    
    
    // Actualizar depth buffer    
    int update_radius = 2;    
    for (int y = center_y - update_radius; y <= center_y + update_radius; y++) {    
        for (int x = center_x - update_radius; x <= center_x + update_radius; x++) {    
            if (x >= 0 && x < 320 && y >= 0 && y < 240) {    
                int idx = y * 320 + x;    
                if (proj.distance < depth_buffer[idx]) {    
                    depth_buffer[idx] = proj.distance;    
                }    
            }    
        }    
    }    
}
      
    return render_buffer->code;  
}
 
// ============================================================================  
// RENDERIZADO GPU CON SHADERS  
// ============================================================================  
  


// Vertex shader    
static const char* voxel_vertex_shader_source =        
"#version 330 core\n"    
"\n"    
"in vec2 bgd_Vertex;\n"        
"in vec2 bgd_TexCoord;\n"        
"out vec2 v_uv;\n"        
"\n"        
"void main() {\n"        
"    v_uv = bgd_Vertex * 0.5 + 0.5;\n"  // CAMBIO: Convertir de [-1,1] a [0,1]  
"    gl_Position = vec4(bgd_Vertex, 0.0, 1.0);\n"        
"}\n";  
 
static const char* voxel_fragment_shader_source =                
"#version 330 core\n"            
"\n"                
"in vec2 v_uv;\n"                
"\n"                
"uniform sampler2D u_heightmap;\n"                
"uniform sampler2D u_texturemap;\n"                
"uniform sampler2D u_water_texture;\n"                
"uniform vec3 u_camera_pos;\n"                
"uniform float u_camera_angle;\n"                
"uniform float u_camera_pitch;\n"                
"uniform float u_fov;\n"                
"uniform float u_max_distance;\n"                
"uniform float u_water_level;\n"                
"uniform float u_water_time;\n"                
"uniform float u_wave_amplitude;\n"                
"uniform float u_light_intensity;\n"                
"uniform vec2 u_heightmap_size;\n"                
"uniform vec3 u_sky_color;\n"                
"uniform float u_current_distance;\n"          
"uniform vec3 u_fog_color;\n"          
"uniform float u_fog_intensity;\n"          
"uniform vec2 u_chunk_min;\n"          
"uniform vec2 u_chunk_max;\n"  
"\n"  
"// NUEVO: Uniforms para mapas de sectores\n"  
"uniform int u_map_type;\n"  
"uniform int u_num_sectors;\n"  
"uniform int u_num_walls;\n"  
"\n"            
"out vec4 FragColor;\n"            
"\n"        
"// Funciones de ruido Simplex 2D\n"    
"vec3 mod289(vec3 x) {\n"    
"    return x - floor(x * (1.0 / 289.0)) * 289.0;\n"    
"}\n"    
"\n"    
"vec2 mod289(vec2 x) {\n"    
"    return x - floor(x * (1.0 / 289.0)) * 289.0;\n"    
"}\n"    
"\n"    
"vec3 permute(vec3 x) {\n"    
"    return mod289(((x*34.0)+1.0)*x);\n"    
"}\n"    
"\n"    
"float snoise(vec2 v) {\n"    
"    const vec4 C = vec4(0.211324865405187, 0.366025403784439,\n"    
"                       -0.577350269189626, 0.024390243902439);\n"    
"    vec2 i  = floor(v + dot(v, C.yy));\n"    
"    vec2 x0 = v - i + dot(i, C.xx);\n"    
"    vec2 i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);\n"    
"    vec4 x12 = x0.xyxy + C.xxzz;\n"    
"    x12.xy -= i1;\n"    
"    i = mod289(i);\n"    
"    vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));\n"    
"    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);\n"    
"    m = m*m; m = m*m;\n"    
"    vec3 x = 2.0 * fract(p * C.www) - 1.0;\n"    
"    vec3 h = abs(x) - 0.5;\n"    
"    vec3 ox = floor(x + 0.5);\n"    
"    vec3 a0 = x - ox;\n"    
"    m *= 1.79284291400159 - 0.85373472095314 * (a0*a0 + h*h);\n"    
"    vec3 g;\n"    
"    g.x  = a0.x  * x0.x  + h.x  * x0.y;\n"    
"    g.yz = a0.yz * x12.xz + h.yz * x12.yw;\n"    
"    return 130.0 * dot(m, g);\n"    
"}\n"    
"\n"    
"// Filtrado bilinear\n"    
"vec4 texture_bilinear(sampler2D tex, vec2 uv, vec2 tex_size) {\n"        
"    vec2 pixel_pos = uv * tex_size;\n"        
"    vec2 pixel_floor = floor(pixel_pos - 0.5) + 0.5;\n"        
"    vec2 frac = pixel_pos - pixel_floor;\n"        
"    vec2 uv00 = pixel_floor / tex_size;\n"        
"    vec2 uv10 = (pixel_floor + vec2(1.0, 0.0)) / tex_size;\n"        
"    vec2 uv01 = (pixel_floor + vec2(0.0, 1.0)) / tex_size;\n"        
"    vec2 uv11 = (pixel_floor + vec2(1.0, 1.0)) / tex_size;\n"        
"    vec4 c00 = texture(tex, uv00);\n"        
"    vec4 c10 = texture(tex, uv10);\n"        
"    vec4 c01 = texture(tex, uv01);\n"        
"    vec4 c11 = texture(tex, uv11);\n"        
"    vec4 c0 = mix(c00, c10, frac.x);\n"        
"    vec4 c1 = mix(c01, c11, frac.x);\n"        
"    return mix(c0, c1, frac.y);\n"        
"}\n"        
"\n"
"// Función principal del shader\n"  
"void main() {\n"  
"    // NUEVO: Detectar tipo de mapa y renderizar según corresponda\n"  
"    if (u_map_type == 1) {\n"  
"        // Renderizado estilo Doom para sectores\n"  
"        float column_angle = u_camera_angle - u_fov * 0.5 + v_uv.x * u_fov;\n"  
"        float cos_angle = cos(column_angle);\n"  
"        float sin_angle = sin(column_angle);\n"  
"        \n"  
"        // Por ahora, renderizar un cubo simple basado en la posición de la cámara\n"  
"        // Asumimos que el sector va de (0,0) a (200,200) con altura 0-150\n"  
"        vec2 ray_dir = vec2(cos_angle, sin_angle);\n"  
"        vec2 ray_pos = u_camera_pos.xy;\n"  
"        \n"  
"        // Detectar intersección con las 4 paredes del cubo\n"  
"        float t_min = 999999.0;\n"  
"        bool hit = false;\n"  
"        \n"  
"        // Pared norte (y = 200)\n"  
"        if (ray_dir.y > 0.001) {\n"  
"            float t = (200.0 - ray_pos.y) / ray_dir.y;\n"  
"            if (t > 0.0 && t < t_min) {\n"  
"                float hit_x = ray_pos.x + ray_dir.x * t;\n"  
"                if (hit_x >= 0.0 && hit_x <= 200.0) {\n"  
"                    t_min = t;\n"  
"                    hit = true;\n"  
"                }\n"  
"            }\n"  
"        }\n"  
"        \n"  
"        // Pared sur (y = 0)\n"  
"        if (ray_dir.y < -0.001) {\n"  
"            float t = -ray_pos.y / ray_dir.y;\n"  
"            if (t > 0.0 && t < t_min) {\n"  
"                float hit_x = ray_pos.x + ray_dir.x * t;\n"  
"                if (hit_x >= 0.0 && hit_x <= 200.0) {\n"  
"                    t_min = t;\n"  
"                    hit = true;\n"  
"                }\n"  
"            }\n"  
"        }\n"  
"        \n"  
"        // Pared este (x = 200)\n"  
"        if (ray_dir.x > 0.001) {\n"  
"            float t = (200.0 - ray_pos.x) / ray_dir.x;\n"  
"            if (t > 0.0 && t < t_min) {\n"  
"                float hit_y = ray_pos.y + ray_dir.y * t;\n"  
"                if (hit_y >= 0.0 && hit_y <= 200.0) {\n"  
"                    t_min = t;\n"  
"                    hit = true;\n"  
"                }\n"  
"            }\n"  
"        }\n"  
"        \n"  
"        // Pared oeste (x = 0)\n"  
"        if (ray_dir.x < -0.001) {\n"  
"            float t = -ray_pos.x / ray_dir.x;\n"  
"            if (t > 0.0 && t < t_min) {\n"  
"                float hit_y = ray_pos.y + ray_dir.y * t;\n"  
"                if (hit_y >= 0.0 && hit_y <= 200.0) {\n"  
"                    t_min = t;\n"  
"                    hit = true;\n"  
"                }\n"  
"            }\n"  
"        }\n"  
"        \n"  
"        if (hit) {\n"  
"            // Calcular altura de la pared en pantalla\n"  
"            float wall_height = 150.0;  // Altura del techo\n"  
"            float floor_height = 0.0;\n"  
"            \n"  
"            // Proyección de la pared\n"  
"            float wall_top = 0.5 + ((u_camera_pos.z - wall_height) / t_min * 300.0 + u_camera_pitch * 40.0) / 240.0;\n"  
"            float wall_bottom = 0.5 + ((u_camera_pos.z - floor_height) / t_min * 300.0 + u_camera_pitch * 40.0) / 240.0;\n"  
"            \n"  
"            if (v_uv.y >= wall_top && v_uv.y <= wall_bottom) {\n"  
"                // Renderizar pared\n"  
"                float fog = 1.0 - (t_min / u_max_distance);\n"  
"                fog = clamp(fog, 0.3, 1.0);\n"  
"                vec3 wall_color = vec3(0.6, 0.5, 0.4) * u_light_intensity;\n"  
"                vec3 final_color = mix(u_sky_color, wall_color, fog);\n"  
"                FragColor = vec4(final_color, 1.0);\n"  
"            } else if (v_uv.y > wall_bottom) {\n"  
"                // Renderizar piso\n"  
"                FragColor = vec4(0.3, 0.3, 0.3, 1.0);\n"  
"            } else {\n"  
"                // Renderizar techo\n"  
"                FragColor = vec4(0.2, 0.2, 0.2, 1.0);\n"  
"            }\n"  
"        } else {\n"  
"            // No hay intersección, renderizar cielo\n"  
"            FragColor = vec4(u_sky_color, 1.0);\n"  
"        }\n"  
"        \n"  
"        return;\n"  
"    }\n"
"\n"  
"    // Código existente para heightmaps (modo voxelspace)\n"  
"    float column_angle = u_camera_angle - u_fov * 0.5 + v_uv.x * u_fov;\n"                
"    \n"                
"    float cos_angle = cos(column_angle);\n"                
"    float sin_angle = sin(column_angle);\n"                
"    \n"                
"    vec2 world_pos = u_camera_pos.xy + vec2(cos_angle, sin_angle) * u_current_distance;\n"                
"    \n"        
"    // Verificar límites de chunks\n"  
"    if (world_pos.x < u_chunk_min.x || world_pos.x > u_chunk_max.x ||\n"        
"        world_pos.y < u_chunk_min.y || world_pos.y > u_chunk_max.y) {\n"        
"        discard;\n"        
"    }\n"        
"    \n"        
"    // Verificar límites del heightmap\n"  
"    if (world_pos.x < 0.0 || world_pos.x >= u_heightmap_size.x ||\n"        
"        world_pos.y < 0.0 || world_pos.y >= u_heightmap_size.y) {\n"        
"        discard;\n"        
"    }\n"        
"    \n"        
"    vec2 uv = world_pos / u_heightmap_size;\n"                
"    \n"                
"    float terrain_height = texture(u_heightmap, uv).r * 255.0;\n"                
"    \n"                
"    bool is_water = terrain_height < u_water_level;\n"                
"    \n"        
"    float render_height;\n"        
"    if (is_water) {\n"        
"        // Calcular olas de agua usando simplex noise\n"  
"        vec2 wave_coord = world_pos * 0.01 + vec2(u_water_time * 0.1, u_water_time * 0.05);\n"    
"        \n"    
"        float noise1 = snoise(wave_coord) * 0.5;\n"    
"        float noise2 = snoise(wave_coord * 2.0) * 0.25;\n"    
"        float noise3 = snoise(wave_coord * 4.0) * 0.125;\n"    
"        \n"    
"        float wave = (noise1 + noise2 + noise3) * u_wave_amplitude;\n"    
"        float water_surface = u_water_level + wave;\n"      
"        \n"      
"        bool camera_underwater = u_camera_pos.z < u_water_level;\n"      
"        \n"      
"        if (camera_underwater) {\n"      
"            render_height = terrain_height;\n"      
"        } else {\n"      
"            render_height = water_surface;\n"      
"        }\n"      
"    } else {\n"        
"        render_height = terrain_height;\n"        
"    }\n"        
"    \n"
"    float height_diff = u_camera_pos.z - render_height;\n"                
"    float projected_y = height_diff / u_current_distance * 300.0;\n"                
"    projected_y += u_camera_pitch * 40.0;\n"                
"    \n"                
"    float screen_y = 0.5 + (projected_y / 240.0);\n"                
"    \n"                
"    if (v_uv.y >= screen_y) {\n"                
"        float fog = 1.0 - (u_current_distance / u_max_distance);\n"                
"        fog = clamp(fog, 0.3, 1.0);\n"                
"        \n"          
"        float fog_factor = 0.0;\n"          
"        if (u_fog_intensity > 0.0) {\n"    
"            float distance_fog = 0.0;\n"    
"            if (u_current_distance > u_max_distance * 0.3) {\n"    
"                float fog_start = u_max_distance * 0.3;\n"    
"                float fog_range = u_max_distance - fog_start;\n"    
"                float fog_progress = (u_current_distance - fog_start) / fog_range;\n"    
"                fog_progress = fog_progress * fog_progress;\n"    
"                distance_fog = fog_progress * u_fog_intensity;\n"    
"            }\n"    
"            \n"    
"            float height_fog = 0.0;\n"    
"            if (render_height < 100.0) {\n"    
"                height_fog = (1.0 - render_height / 100.0) * u_fog_intensity * 0.3;\n"    
"            }\n"    
"            \n"    
"            fog_factor = max(distance_fog, height_fog);\n"    
"            fog_factor = clamp(fog_factor, 0.0, 0.7);\n"    
"        }\n"    
"        \n"
"        if (is_water) {\n"        
"            bool camera_underwater = u_camera_pos.z < u_water_level;\n"      
"            \n"      
"            if (camera_underwater) {\n"      
"                vec2 terrain_tex_size = vec2(textureSize(u_texturemap, 0));\n"        
"                vec3 terrain_color = texture_bilinear(u_texturemap, uv, terrain_tex_size).rgb;\n"        
"                terrain_color *= u_light_intensity * 0.7;\n"    
"                terrain_color.b *= 1.3;\n"      
"                \n"      
"                vec3 base_color = mix(u_sky_color, terrain_color, fog);\n"        
"                if (fog_factor > 0.1) {\n"        
"                    base_color = mix(base_color, u_fog_color, fog_factor);\n"        
"                }\n"        
"                FragColor = vec4(base_color, 1.0);\n"      
"            } else {\n"      
"                float water_depth = u_water_level - terrain_height;\n"        
"                float water_surface_alpha = clamp(water_depth / 30.0, 0.4, 0.85);\n"        
"                \n"        
"                vec2 water_uv = mod(world_pos * 0.01 + vec2(u_water_time * 0.1, u_water_time * 0.05), 1.0);\n"        
"                vec2 water_tex_size = vec2(textureSize(u_water_texture, 0));\n"        
"                vec4 water_sample = texture_bilinear(u_water_texture, water_uv, water_tex_size);\n"        
"                \n"        
"                vec2 terrain_tex_size = vec2(textureSize(u_texturemap, 0));\n"        
"                vec3 terrain_color = texture_bilinear(u_texturemap, uv, terrain_tex_size).rgb;\n"        
"                terrain_color *= u_light_intensity;\n"    
"                \n"        
"                vec3 view_dir = normalize(vec3(world_pos.x - u_camera_pos.x, world_pos.y - u_camera_pos.y, -1.0));\n"    
"                vec3 water_normal = vec3(0.0, 0.0, 1.0);\n"    
"                vec3 reflect_dir = reflect(view_dir, water_normal);\n"    
"                \n"    
"                float fresnel = pow(1.0 - max(dot(-view_dir, water_normal), 0.0), 3.0);\n"    
"                vec3 reflection_color = u_sky_color;\n"    
"                \n"        
"                vec3 water_color = mix(terrain_color, water_sample.rgb, water_surface_alpha);\n"        
"                water_color = mix(water_color, reflection_color, fresnel * 0.3);\n"    
"                \n"    
"                vec3 base_color = mix(u_sky_color, water_color, fog);\n"        
"                if (fog_factor > 0.1) {\n"        
"                    base_color = mix(base_color, u_fog_color, fog_factor);\n"        
"                }\n"        
"                FragColor = vec4(base_color, 1.0);\n"      
"            }\n"      
"        } else {\n"                
"            vec2 terrain_tex_size = vec2(textureSize(u_texturemap, 0));\n"        
"            vec3 terrain_color = texture_bilinear(u_texturemap, uv, terrain_tex_size).rgb;\n"        
"            terrain_color *= u_light_intensity;\n"          
"            vec3 base_color = mix(u_sky_color, terrain_color, fog);\n"          
"            if (fog_factor > 0.1) {\n"          
"                base_color = mix(base_color, u_fog_color, fog_factor);\n"          
"            }\n"          
"            FragColor = vec4(base_color, 1.0);\n"                
"        }\n"                
"    } else {\n"                
"        discard;\n"                
"    }\n"                
"}\n";

//-------------------------------------------------------------------------------------------------//


static int create_voxelspace_shader() {  
    if (voxel_shader) return 1;  
      
    voxel_shader = shader_create(  
        (char*)voxel_vertex_shader_source,  
        (char*)voxel_fragment_shader_source  
    );  
      
    if (!voxel_shader) {  
        fprintf(stderr, "ERROR: No se pudo crear shader de voxelspace\n");  
        return 0;  
    }  
      
    fprintf(stderr, "Shader de voxelspace creado exitosamente\n");  
     // AÑADIR AQUÍ - Crear parámetros del shader  
    if (!voxel_params) {  
        voxel_params = shader_create_parameters(25); // 14 uniforms  
        if (!voxel_params) {  
            fprintf(stderr, "ERROR: No se pudo crear parámetros del shader\n");  
            return 0;  
        }  
        fprintf(stderr, "Parámetros del shader creados exitosamente\n");  
    }  

     // AÑADIR AQUÍ - Verificar ubicaciones de uniforms  
    int loc_heightmap = shader_getuniformlocation(voxel_shader, "u_heightmap");  
    int loc_texturemap = shader_getuniformlocation(voxel_shader, "u_texturemap");  
    int loc_camera_pos = shader_getuniformlocation(voxel_shader, "u_camera_pos");  
    int loc_current_distance = shader_getuniformlocation(voxel_shader, "u_current_distance");  
      
    fprintf(stderr, "Uniform locations:\n");  
    fprintf(stderr, "  u_heightmap: %d\n", loc_heightmap);  
    fprintf(stderr, "  u_texturemap: %d\n", loc_texturemap);  
    fprintf(stderr, "  u_camera_pos: %d\n", loc_camera_pos);  
    fprintf(stderr, "  u_current_distance: %d\n", loc_current_distance);  
      
    if (loc_heightmap < 0 || loc_texturemap < 0) {  
        fprintf(stderr, "WARNING: Algunos uniforms críticos no se encontraron\n");  
    }  
    return 1;  

}

static int setup_shader_parameters(HEIGHTMAP *hm) {  
    if (!voxel_shader) return 0;  
 
      
    if (!voxel_params) {  
        voxel_params = shader_create_parameters(11);  
        if (!voxel_params) {  
            fprintf(stderr, "ERROR: No se pudo crear parámetros del shader\n");  
            return 0;  
        }  
    }  
      
    // Obtener ubicaciones de uniforms  
    int loc_heightmap = shader_getuniformlocation(voxel_shader, "u_heightmap");  
    int loc_texturemap = shader_getuniformlocation(voxel_shader, "u_texturemap");  
    int loc_camera_pos = shader_getuniformlocation(voxel_shader, "u_camera_pos");  
    int loc_camera_angle = shader_getuniformlocation(voxel_shader, "u_camera_angle");  
    int loc_camera_pitch = shader_getuniformlocation(voxel_shader, "u_camera_pitch");  
    int loc_fov = shader_getuniformlocation(voxel_shader, "u_fov");  
    int loc_max_distance = shader_getuniformlocation(voxel_shader, "u_max_distance");  
    int loc_water_level = shader_getuniformlocation(voxel_shader, "u_water_level");  
    int loc_light_intensity = shader_getuniformlocation(voxel_shader, "u_light_intensity");  
    int loc_heightmap_size = shader_getuniformlocation(voxel_shader, "u_heightmap_size");  
    int loc_sky_color = shader_getuniformlocation(voxel_shader, "u_sky_color");  

 
      
    // Configurar texturas (pasar GRAPH* directamente)  
    if (loc_heightmap >= 0) {  
        shader_set_param(voxel_params, SHADER_IMAGE, loc_heightmap, 0,  
                        hm->heightmap, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_texturemap >= 0 && hm->texturemap) {  
        shader_set_param(voxel_params, SHADER_IMAGE, loc_texturemap, 1,  
                        hm->texturemap, 0, 0, 0, 0, 0);  
    }  
      
    // Configurar vec3 u_camera_pos  
    if (loc_camera_pos >= 0) {  
        float camera_pos[3] = { camera.x, camera.y, camera.z };  
        shader_set_param(voxel_params, UNIFORM_FLOAT3_ARRAY, loc_camera_pos, 1,  
                        camera_pos, 0, 0, 0, 0, 0);  
    }  
      
    // Configurar floats individuales  
    if (loc_camera_angle >= 0) {  
        float angle_val = camera.angle;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_camera_angle, 0,  
                        (void*)(intptr_t)*(int32_t*)&angle_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_camera_pitch >= 0) {  
        float pitch_val = camera.pitch;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_camera_pitch, 0,  
                        (void*)(intptr_t)*(int32_t*)&pitch_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_fov >= 0) {  
        float fov_val = 0.7f;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_fov, 0,  
                        (void*)(intptr_t)*(int32_t*)&fov_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_max_distance >= 0) {  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_max_distance, 0,  
                        (void*)(intptr_t)*(int32_t*)&max_render_distance, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_water_level >= 0) {  
        float water_val = water_level;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_water_level, 0,  
                        (void*)(intptr_t)*(int32_t*)&water_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_light_intensity >= 0) {  
        float light_val = light_intensity / 255.0f;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_light_intensity, 0,  
                        (void*)(intptr_t)*(int32_t*)&light_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_heightmap_size >= 0) {  
        float heightmap_size[2] = { (float)hm->width, (float)hm->height };  
        shader_set_param(voxel_params, UNIFORM_FLOAT2_ARRAY, loc_heightmap_size, 1,  
                        heightmap_size, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_sky_color >= 0) {  
        float sky_color[3] = {  
            sky_color_r / 255.0f,  
            sky_color_g / 255.0f,  
            sky_color_b / 255.0f  
        };  
        shader_set_param(voxel_params, UNIFORM_FLOAT3_ARRAY, loc_sky_color, 1,  
                        sky_color, 0, 0, 0, 0, 0);  
    }  
      
    return 1;  
}


// GPU...
int64_t libmod_heightmap_render_voxelspace_gpu(INSTANCE *my, int64_t *params) {                
    int64_t hm_id = params[0];                
    int64_t render_width = params[1];                
    int64_t render_height = params[2];                
                    
    // Buscar heightmap                
    HEIGHTMAP *hm = NULL;                
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {                
        if (heightmaps[i].id == hm_id) {                
            hm = &heightmaps[i];                
            break;                
        }                
    }                
                    
    if (!hm) {                
        fprintf(stderr, "ERROR: Heightmap no encontrado\n");                
        return 0;                
    }  
      
                    
    if (!create_voxelspace_shader()) {                
        return 0;                
    }                
                    
    if (!render_buffer || render_buffer->width != render_width ||                 
        render_buffer->height != render_height) {                
        if (render_buffer) {                
            bitmap_destroy(render_buffer);                
        }                
        render_buffer = bitmap_new_syslib(render_width, render_height);                
        if (!render_buffer) {                
            fprintf(stderr, "ERROR: No se pudo crear render_buffer\n");                
            return 0;                
        }       
        // Actualizar variables globales con las nuevas dimensiones    
        current_render_width = render_width;    
        current_render_height = render_height;             
    }                
                    
    uint32_t sky_color = SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, 255);                
    gr_clear_as(render_buffer, sky_color);                
            
    if (sky_texture) {        
        render_skybox(camera.angle, camera.pitch, water_time, 1);        
    }        
                    
    static GRAPH *quad_source = NULL;              
                  
    if (!quad_source) {              
        quad_source = bitmap_new_syslib(2, 2);              
        if (!quad_source) {              
            fprintf(stderr, "ERROR: No se pudo crear quad_source\n");              
            return 0;              
        }              
        gr_clear_as(quad_source, SDL_MapRGBA(gPixelFormat, 255, 255, 255, 255));              
    }

       if (!voxel_params) {                
        voxel_params = shader_create_parameters(25);  // AUMENTADO: de 19 a 25 para nuevos uniforms  
        if (!voxel_params) {                
            fprintf(stderr, "ERROR: No se pudo crear parámetros del shader\n");                
            return 0;                
        }                
    }                
                    
    static int loc_heightmap = -1;              
    static int loc_texturemap = -1;              
    static int loc_camera_pos = -1;              
    static int loc_camera_angle = -1;              
    static int loc_camera_pitch = -1;              
    static int loc_fov = -1;              
    static int loc_max_distance = -1;              
    static int loc_water_level = -1;              
    static int loc_light_intensity = -1;              
    static int loc_heightmap_size = -1;              
    static int loc_sky_color = -1;              
    static int loc_current_distance = -1;          
    static int loc_fog_color = -1;        
    static int loc_fog_intensity = -1;        
    static int loc_water_texture = -1;        
    static int loc_water_time = -1;        
    static int loc_wave_amplitude = -1;        
    static int loc_chunk_min = -1;      
    static int loc_chunk_max = -1;  
      
                  
    static int locations_initialized = 0;              
    if (!locations_initialized) {              
        loc_heightmap = shader_getuniformlocation(voxel_shader, "u_heightmap");                
        loc_texturemap = shader_getuniformlocation(voxel_shader, "u_texturemap");                
        loc_camera_pos = shader_getuniformlocation(voxel_shader, "u_camera_pos");                
        loc_camera_angle = shader_getuniformlocation(voxel_shader, "u_camera_angle");                
        loc_camera_pitch = shader_getuniformlocation(voxel_shader, "u_camera_pitch");                
        loc_fov = shader_getuniformlocation(voxel_shader, "u_fov");                
        loc_max_distance = shader_getuniformlocation(voxel_shader, "u_max_distance");                
        loc_water_level = shader_getuniformlocation(voxel_shader, "u_water_level");                
        loc_light_intensity = shader_getuniformlocation(voxel_shader, "u_light_intensity");                
        loc_heightmap_size = shader_getuniformlocation(voxel_shader, "u_heightmap_size");                
        loc_sky_color = shader_getuniformlocation(voxel_shader, "u_sky_color");                
        loc_current_distance = shader_getuniformlocation(voxel_shader, "u_current_distance");          
        loc_fog_color = shader_getuniformlocation(voxel_shader, "u_fog_color");        
        loc_fog_intensity = shader_getuniformlocation(voxel_shader, "u_fog_intensity");        
        loc_water_texture = shader_getuniformlocation(voxel_shader, "u_water_texture");        
        loc_water_time = shader_getuniformlocation(voxel_shader, "u_water_time");        
        loc_wave_amplitude = shader_getuniformlocation(voxel_shader, "u_wave_amplitude");        
        loc_chunk_min = shader_getuniformlocation(voxel_shader, "u_chunk_min");      
        loc_chunk_max = shader_getuniformlocation(voxel_shader, "u_chunk_max");  
                 
        locations_initialized = 1;              
    }              
                  
    static float camera_pos[3];              
    static float heightmap_size[2];              
    static float sky_color_arr[3];          
    static float fog_color_arr[3];        
    static float chunk_min[2];      
    static float chunk_max[2];

    // Configurar texturas del heightmap (sin condicionales)  
    if (loc_heightmap >= 0 && hm->heightmap) {  
        shader_set_param(voxel_params, SHADER_IMAGE, loc_heightmap, 0,  
                        (void*)hm->heightmap, 0, 0, 0, 0, 0);  
    }  
    
    if (loc_texturemap >= 0 && hm->texturemap) {  
        shader_set_param(voxel_params, SHADER_IMAGE, loc_texturemap, 0,  
                        (void*)hm->texturemap, 1, 0, 0, 0, 0);  
    }
            
    if (loc_water_texture >= 0 && water_texture) {        
        shader_set_param(voxel_params, SHADER_IMAGE, loc_water_texture, 0,        
                        (void*)water_texture, 2, 0, 0, 0, 0);        
    }        
                    
    shader_activate(voxel_shader);          
              
    if (loc_camera_pos >= 0) {                
        camera_pos[0] = camera.x;              
        camera_pos[1] = camera.y;              
        camera_pos[2] = camera.z;              
        shader_set_param(voxel_params, UNIFORM_FLOAT3_ARRAY, loc_camera_pos, 1,                
                        (void*)camera_pos, 0, 0, 0, 0, 0);                
    }                
                    
    if (loc_camera_angle >= 0) {                
        float angle_val = camera.angle;        
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_camera_angle, 0,                
                        *(int32_t*)&angle_val, 0, 0, 0, 0, 0);                
    }                
                    
    if (loc_camera_pitch >= 0) {                
        float pitch_val = camera.pitch;        
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_camera_pitch, 0,                
                        *(int32_t*)&pitch_val, 0, 0, 0, 0, 0);                
    }                
                    
    if (loc_fov >= 0) {                
        float fov_val = 0.7f;                
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_fov, 0,                
                        *(int32_t*)&fov_val, 0, 0, 0, 0, 0);                
    }                
                    
    if (loc_max_distance >= 0) {                
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_max_distance, 0,                
                        *(int32_t*)&max_render_distance, 0, 0, 0, 0, 0);                
    }                
                    
    if (loc_water_level >= 0) {                
        float water_val = water_level;                
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_water_level, 0,                
                        *(int32_t*)&water_val, 0, 0, 0, 0, 0);                
    }                
                    
    if (loc_light_intensity >= 0) {                
        float light_val = light_intensity / 255.0f;                
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_light_intensity, 0,                
                        *(int32_t*)&light_val, 0, 0, 0, 0, 0);                
    }    
        
    if (loc_heightmap_size >= 0) {              
        heightmap_size[0] = (float)hm->width;            
        heightmap_size[1] = (float)hm->height;            
        shader_set_param(voxel_params, UNIFORM_FLOAT2_ARRAY, loc_heightmap_size, 1,              
                        (void*)heightmap_size, 0, 0, 0, 0, 0);              
    }              
                  
    if (loc_sky_color >= 0) {              
        sky_color_arr[0] = sky_color_r / 255.0f;            
        sky_color_arr[1] = sky_color_g / 255.0f;            
        sky_color_arr[2] = sky_color_b / 255.0f;            
        shader_set_param(voxel_params, UNIFORM_FLOAT3_ARRAY, loc_sky_color, 1,              
                        (void*)sky_color_arr, 0, 0, 0, 0, 0);              
    }        
            
    if (loc_fog_color >= 0) {        
        fog_color_arr[0] = fog_color_r / 255.0f;        
        fog_color_arr[1] = fog_color_g / 255.0f;        
        fog_color_arr[2] = fog_color_b / 255.0f;        
        shader_set_param(voxel_params, UNIFORM_FLOAT3_ARRAY, loc_fog_color, 1,        
                        (void*)fog_color_arr, 0, 0, 0, 0, 0);        
    }        
            
    if (loc_fog_intensity >= 0) {        
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_fog_intensity, 0,        
                        *(int32_t*)&fog_intensity, 0, 0, 0, 0, 0);        
    }      
          
    if (loc_water_time >= 0) {      
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_water_time, 0,      
                        *(int32_t*)&water_time, 0, 0, 0, 0, 0);      
    }      
          
    if (loc_wave_amplitude >= 0) {      
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_wave_amplitude, 0,      
                        *(int32_t*)&wave_amplitude, 0, 0, 0, 0, 0);      
    }      
          
    int chunk_x = (int)(camera.x / chunk_size);      
    int chunk_y = (int)(camera.y / chunk_size);      
    int min_chunk_x = chunk_x - chunk_radius;      
    int max_chunk_x = chunk_x + chunk_radius;      
    int min_chunk_y = chunk_y - chunk_radius;      
    int max_chunk_y = chunk_y + chunk_radius;      
          
    if (loc_chunk_min >= 0) {      
        chunk_min[0] = (float)(min_chunk_x * chunk_size);      
        chunk_min[1] = (float)(min_chunk_y * chunk_size);      
        shader_set_param(voxel_params, UNIFORM_FLOAT2_ARRAY, loc_chunk_min, 1,      
                        (void*)chunk_min, 0, 0, 0, 0, 0);      
    }      
          
    if (loc_chunk_max >= 0) {      
        chunk_max[0] = (float)(max_chunk_x * chunk_size);      
        chunk_max[1] = (float)(max_chunk_y * chunk_size);      
        shader_set_param(voxel_params, UNIFORM_FLOAT2_ARRAY, loc_chunk_max, 1,      
                        (void*)chunk_max, 0, 0, 0, 0, 0);      
    }    

       // BUCLE DE RENDERIZADO DEL SHADER  
    for (float distance = max_render_distance; distance >= 1.0; ) {        
        float step;        
        if (distance < 100.0f) {        
            step = 1.5f;      
        } else if (distance < 400.0f) {        
            step = 2.5f;      
        } else {        
            step = 3.5f;      
        }        
                
        if (loc_current_distance >= 0) {              
            shader_set_param(voxel_params, UNIFORM_FLOAT, loc_current_distance, 0,              
                            *(int32_t*)&distance, 0, 0, 0, 0, 0);              
        }              
                      
        shader_apply_parameters(voxel_params);              
                      
        gr_blit(              
            render_buffer,            
            NULL,            
            0, 0,      
            0, 0,            
            (render_width / 2.0) * 100.0,      
            (render_height / 2.0) * 100.0,      
            POINT_UNDEFINED,            
            POINT_UNDEFINED,            
            quad_source,            
            NULL,            
            255, 255, 255, 255,            
            BLEND_NORMAL,    
            NULL            
        );    
            
        distance -= step;      
    }              
                  
    shader_deactivate();    
        
    // ============================================================================    
    // RENDERIZADO DE BILLBOARDS CON EFECTOS AVANZADOS    
    // ============================================================================    
        
    // Array temporal para billboards visibles    
    BILLBOARD_RENDER_DATA visible_billboards[MAX_STATIC_BILLBOARDS + MAX_DYNAMIC_BILLBOARDS];    
    int visible_count = 0;    
        
    // Recopilar billboards visibles    
    float terrain_fov = 0.7f;    
    collect_visible_billboards_from_array(static_billboards, static_billboard_count,    
                                         visible_billboards, &visible_count, terrain_fov);    
    collect_visible_billboards_from_array(dynamic_billboards, MAX_DYNAMIC_BILLBOARDS,    
                                         visible_billboards, &visible_count, terrain_fov);    
        
    // Ordenar por distancia (más lejanos primero)    
    qsort(visible_billboards, visible_count, sizeof(BILLBOARD_RENDER_DATA),     
          compare_billboards_by_distance);    
        
    // Renderizar cada billboard con efectos avanzados    
    for (int i = 0; i < visible_count; i++) {    
        BILLBOARD_RENDER_DATA *render_data = &visible_billboards[i];    
        BILLBOARD_PROJECTION proj = render_data->projection;    
            
        // Verificación de oclusión mejorada    
        int billboard_visible = 1;    
        int center_x = proj.screen_x;    
        int center_y = proj.screen_y;    
        int half_width = proj.scaled_width / 2;    
        int half_height = proj.scaled_height / 2;    
            
        // Verificar si está fuera de pantalla    
        if (center_x + half_width < 0 || center_x - half_width >= render_width ||    
            center_y + half_height < 0 || center_y - half_height >= render_height) {    
            continue;    
        }    
            
        if (!billboard_visible) continue;    
            
        // EFECTO AVANZADO 1: Calcular iluminación dinámica    
        float light_factor = light_intensity / 255.0f;    
        int r_mod = (int)(255 * light_factor);    
        int g_mod = (int)(255 * light_factor);    
        int b_mod = (int)(255 * light_factor);    
            
        // EFECTO AVANZADO 2: Aplicar tintado de niebla    
        if (proj.fog_tint_factor > 0.0f) {    
            r_mod = (int)(r_mod * (1.0f - proj.fog_tint_factor) + fog_color_r * proj.fog_tint_factor);    
            g_mod = (int)(g_mod * (1.0f - proj.fog_tint_factor) + fog_color_g * proj.fog_tint_factor);    
            b_mod = (int)(b_mod * (1.0f - proj.fog_tint_factor) + fog_color_b * proj.fog_tint_factor);    
        }    
            
        // EFECTO AVANZADO 3: Soft edges basado en distancia    
        int alpha = proj.alpha;    
        if (proj.distance > max_render_distance * 0.8f) {    
            float fade = 1.0f - ((proj.distance - max_render_distance * 0.8f) / (max_render_distance * 0.2f));    
            alpha = (int)(alpha * fade);    
        }    
            
        // Renderizar billboard con todos los efectos aplicados    
        gr_blit(render_buffer, NULL,    
               proj.screen_x - proj.scaled_width/2,    
               proj.screen_y - proj.scaled_height/2,    
               0, 0, proj.scaled_width, proj.scaled_height,    
               render_data->graph->width/2, render_data->graph->height/2,    
               render_data->graph, NULL,     
               r_mod, g_mod, b_mod, alpha, 0, NULL);    
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
    hm->cache_valid = 0;  
}  
  
hm->height_cache = malloc(hm->width * hm->height * sizeof(float));  
if (!hm->height_cache)  
{  
    hm->cache_valid = 0;  
    fprintf(stderr, "Error: No se pudo asignar height_cache para heightmap %ldx%ld\n",   
            (int)hm->width, (int)hm->height);  
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
uint32_t get_texture_color_bilinear(GRAPH *texture, float x, float y) {  
    if (!texture)  
        return 0;  
  
    // Implementar wrapping de coordenadas para tiling  
    while (x < 0) x += texture->width;  
    while (y < 0) y += texture->height;  
    x = fmod(x, texture->width);  
    y = fmod(y, texture->height);  
  
    int ix = (int)x;  
    int iy = (int)y;  
      
    // Asegurar que estamos dentro de los límites para interpolación  
    if (ix >= texture->width - 1) ix = texture->width - 2;  
    if (iy >= texture->height - 1) iy = texture->height - 2;  
    if (ix < 0) ix = 0;  
    if (iy < 0) iy = 0;  
  
    float fx = x - ix;  
    float fy = y - iy;  
  
    // Obtener los 4 píxeles para interpolación  
    uint32_t c00 = gr_get_pixel(texture, ix, iy);  
    uint32_t c10 = gr_get_pixel(texture, (ix + 1) % texture->width, iy);  
    uint32_t c01 = gr_get_pixel(texture, ix, (iy + 1) % texture->height);  
    uint32_t c11 = gr_get_pixel(texture, (ix + 1) % texture->width, (iy + 1) % texture->height);  
  
    // Verificar si algún pixel es inválido  
    if (c00 == 0 && c10 == 0 && c01 == 0 && c11 == 0) {  
        return 0; // Fallback al sistema original  
    }  
  
    // Extraer componentes RGB y Alpha si está disponible  
    Uint8 r00, g00, b00, a00 = 255;  
    Uint8 r10, g10, b10, a10 = 255;  
    Uint8 r01, g01, b01, a01 = 255;  
    Uint8 r11, g11, b11, a11 = 255;  
  
  // Al inicio de la función, después de las validaciones  
static int cached_bytes_per_pixel = 0;  
if (cached_bytes_per_pixel == 0) {  
    cached_bytes_per_pixel = gPixelFormat->BytesPerPixel;  
}  
  
// Extraer componentes usando los shifts correctos del formato  
if (cached_bytes_per_pixel == 4) {  
    // RGBA de 32 bits  
    r00 = (c00 >> gPixelFormat->Rshift) & 0xFF;   
    g00 = (c00 >> gPixelFormat->Gshift) & 0xFF;   
    b00 = (c00 >> gPixelFormat->Bshift) & 0xFF;   
    a00 = (c00 >> gPixelFormat->Ashift) & 0xFF;  
      
    r10 = (c10 >> gPixelFormat->Rshift) & 0xFF;   
    g10 = (c10 >> gPixelFormat->Gshift) & 0xFF;   
    b10 = (c10 >> gPixelFormat->Bshift) & 0xFF;   
    a10 = (c10 >> gPixelFormat->Ashift) & 0xFF;  
      
    r01 = (c01 >> gPixelFormat->Rshift) & 0xFF;   
    g01 = (c01 >> gPixelFormat->Gshift) & 0xFF;   
    b01 = (c01 >> gPixelFormat->Bshift) & 0xFF;   
    a01 = (c01 >> gPixelFormat->Ashift) & 0xFF;  
      
    r11 = (c11 >> gPixelFormat->Rshift) & 0xFF;   
    g11 = (c11 >> gPixelFormat->Gshift) & 0xFF;   
    b11 = (c11 >> gPixelFormat->Bshift) & 0xFF;   
    a11 = (c11 >> gPixelFormat->Ashift) & 0xFF;  
} else {  
    // RGB de 24 bits  
    r00 = (c00 >> gPixelFormat->Rshift) & 0xFF;   
    g00 = (c00 >> gPixelFormat->Gshift) & 0xFF;   
    b00 = (c00 >> gPixelFormat->Bshift) & 0xFF;  
      
    r10 = (c10 >> gPixelFormat->Rshift) & 0xFF;   
    g10 = (c10 >> gPixelFormat->Gshift) & 0xFF;   
    b10 = (c10 >> gPixelFormat->Bshift) & 0xFF;  
      
    r01 = (c01 >> gPixelFormat->Rshift) & 0xFF;   
    g01 = (c01 >> gPixelFormat->Gshift) & 0xFF;   
    b01 = (c01 >> gPixelFormat->Bshift) & 0xFF;  
      
    r11 = (c11 >> gPixelFormat->Rshift) & 0xFF;   
    g11 = (c11 >> gPixelFormat->Gshift) & 0xFF;   
    b11 = (c11 >> gPixelFormat->Bshift) & 0xFF;  
}
  
    // Interpolación bilineal mejorada  
    float r0 = r00 + fx * (r10 - r00);  
    float r1 = r01 + fx * (r11 - r01);  
    float rf = r0 + fy * (r1 - r0);  
  
    float g0 = g00 + fx * (g10 - g00);  
    float g1 = g01 + fx * (g11 - g01);  
    float gf = g0 + fy * (g1 - g0);  
  
    float b0 = b00 + fx * (b10 - b00);  
    float b1 = b01 + fx * (b11 - b01);  
    float bf = b0 + fy * (b1 - b0);  
  
    float a0 = a00 + fx * (a10 - a00);  
    float a1 = a01 + fx * (a11 - a01);  
    float af = a0 + fy * (a1 - a0);  
  
    // Clamp values  
    rf = fmaxf(0, fminf(255, rf));  
    gf = fmaxf(0, fminf(255, gf));  
    bf = fmaxf(0, fminf(255, bf));  
    af = fmaxf(0, fminf(255, af));  
  
    // Retornar el color apropiado según el formato  
    if (gPixelFormat->BytesPerPixel == 4) {  
        return SDL_MapRGBA(gPixelFormat, (Uint8)rf, (Uint8)gf, (Uint8)bf, (Uint8)af);  
    } else {  
        return SDL_MapRGB(gPixelFormat, (Uint8)rf, (Uint8)gf, (Uint8)bf);  
    }  
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

    camera.z = terrain_height + 80.0f;


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

    if (slot == -1) {  
        fprintf(stderr, "Error: MAX_HEIGHTMAPS (%d) alcanzado\n", MAX_HEIGHTMAPS);  
        return 0;  
    }  
      
    // Verificar que next_heightmap_id no desborde  
    if (next_heightmap_id >= INT64_MAX - 1) {  
        fprintf(stderr, "Error: next_heightmap_id overflow\n");  
        return 0;  
    }

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

    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);

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
    float world_x = (float)sprite_x / WORLD_TO_SPRITE_SCALE;  
    float world_y = (float)sprite_y / WORLD_TO_SPRITE_SCALE;

    float terrain_height = get_height_at(hm, world_x, world_y);
    return (int64_t)(terrain_height * WORLD_TO_SPRITE_SCALE);// Devolver en coordenadas de sprite
}

/* Verificar si un sprite puede moverse a una posición sin atravesar terreno */
int64_t libmod_heightmap_can_sprite_move_to(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    int64_t new_x = params[1];
    int64_t new_y = params[2];
    int64_t sprite_height = params[3]; // Altura del sprite sobre el suelo

    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);

    if (!hm || !hm->cache_valid)
        return 1; // Permitir movimiento si no hay heightmap

    // Convertir a coordenadas del heightmap
    float world_x = (float)new_x / WORLD_TO_SPRITE_SCALE;  
    float world_y = (float)new_y / WORLD_TO_SPRITE_SCALE;

    // Verificar límites del mapa
    if (world_x < 0 || world_x >= hm->width - 1 ||
        world_y < 0 || world_y >= hm->height - 1)
        return 0; // No permitir movimiento fuera del mapa

    float terrain_height = get_height_at(hm, world_x, world_y);
    float required_height = terrain_height + (float)sprite_height / WORLD_TO_SPRITE_SCALE;

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
          
    float dx = world_x - camera.x;      
    float dy = world_y - camera.y;      
    float dz = world_z - camera.z;      
          
    float distance_3d = sqrtf(dx * dx + dy * dy + dz * dz);    
        
    float cos_a = cosf(camera.angle);      
    float sin_a = sinf(camera.angle);      
          
    float forward = dx * cos_a + dy * sin_a;      
    float right = -dx * sin_a + dy * cos_a;      
          
    if (forward > 1.0f && forward < max_render_distance) {      
        float perspective_factor = 100.0f / distance_3d;  
          
        // CORREGIDO: Usar dimensiones dinámicas  
        float half_width = current_render_width / 2.0f;  
        float half_height = current_render_height / 2.0f;  
          
        float projected_x = half_width + (right * perspective_factor);      
        float projected_y = half_height - (dz * perspective_factor) + (camera.pitch * 30.0f);      
          
        // CORREGIDO: Límites dinámicos extendidos  
        float margin = 100.0f;  
        if (projected_x >= -margin && projected_x <= current_render_width + margin &&      
            projected_y >= -margin && projected_y <= current_render_height + margin) {      
                  
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

HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
  
if (!hm || !hm->cache_valid)  
    return 255;  
  
float world_x = (float)sprite_x / WORLD_TO_SPRITE_SCALE;  
float world_y = (float)sprite_y / WORLD_TO_SPRITE_SCALE;  
  
// Validar coordenadas  
if (world_x < 0 || world_x >= hm->width ||   
    world_y < 0 || world_y >= hm->height) {  
    return 255; // Fuera de límites, iluminación completa  
}  
  
// Calcular distancia a la cámara para fog  
float dx = world_x - camera.x;  
float dy = world_y - camera.y;  
float distance = sqrtf(dx * dx + dy * dy);  
  
float fog = 1.0f - (distance / FOG_MAX_DISTANCE);  
if (fog < FOG_MIN_VISIBILITY)  
    fog = FOG_MIN_VISIBILITY;

    float total_light = (light_intensity / 255.0f) * fog;

    return (int64_t)(total_light * 255.0f);
}

/* Asignar sprite para que la cámara lo siga */
int64_t libmod_heightmap_update_camera_follow(INSTANCE *my, int64_t *params)  

{  

    int64_t hm_id = params[0];  

    int64_t sprite_x = params[1];  

    int64_t sprite_y = params[2];  

    int64_t sprite_z = params[3];  

  

    if (camera_follow_sprite_id == 0)  

        return 0;  

  

    // NO dividir por 10.0f - usar coordenadas directas del sprite  

    float world_x = (float)sprite_x;  

    float world_y = (float)sprite_y;  

    float world_z = (float)sprite_z;  

  

    // Los offsets también deben estar en la misma escala  

  camera.x = world_x + camera_follow_offset_x;  // Sin multiplicar por 10  

camera.y = world_y + camera_follow_offset_y;  // Sin multiplicar por 10    

camera.z = world_z + camera_follow_offset_z;  // Sin multiplicar por 10

  

    // Calcular ángulo para que la cámara mire hacia la nave  

    float dx = world_x - camera.x;  

    float dy = world_y - camera.y;  

      

    if (dx != 0.0f || dy != 0.0f) {  

        camera.angle = atan2f(dy, dx);  

    }  

  

    // Pitch para tercera persona  

    camera.pitch = -0.3f;  

  

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
    float requested = (float)params[0];  
      
    if (requested < 0.0f) {  
        fprintf(stderr, "Error: render_distance no puede ser negativa\n");  
        return 0;  
    }  
      
    max_render_distance = requested;  
    if (max_render_distance < 100.0f)  
        max_render_distance = 100.0f;  
    if (max_render_distance > 2000.0f)  
        max_render_distance = 2000.0f;  
      
    return 1;  
}

// Nueva función para configurar chunks
int64_t libmod_heightmap_set_chunk_config(INSTANCE *my, int64_t *params)  
{  
    int requested_size = (int)params[0];  
    int requested_radius = (int)params[1];  
      
    if (requested_size <= 0 || requested_radius <= 0) {  
        fprintf(stderr, "Error: chunk_size y chunk_radius deben ser positivos\n");  
        return 0;  
    }  
      
    chunk_size = requested_size;  
    chunk_radius = requested_radius;  
  
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
        return 1;    
    }   
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
    float wx = *(float*)&params[1];  
    float wy = *(float*)&params[2];    
    float wz = *(float*)&params[3];    
    
    if (heightmap_id < 0 || heightmap_id >= MAX_HEIGHTMAPS)    
        return 0;    
    
    float dx = wx - camera.x;    
    float dy = wy - camera.y;    
    
    float angle_rad = -camera.angle * M_PI / 180.0f;    
    float x_cam = dx * cosf(angle_rad) - dy * sinf(angle_rad);    
    float z_cam = dx * sinf(angle_rad) + dy * cosf(angle_rad);    
    
    if (z_cam < 1.0f)    
        return 0;    
    
    // CORREGIDO: Usar dimensiones dinámicas  
    float half_width = current_render_width / 2.0f;  
    float half_height = current_render_height / 2.0f;  
      
    float fov_scale = (current_render_height * 0.5f) / tanf(camera.fov * 0.5f * M_PI / 180.0f);    
    int screen_x = (int)(half_width + x_cam * fov_scale / z_cam);  
    int screen_y = (int)(half_height - (wz - camera.z) * fov_scale / z_cam);  
    
    // CORREGIDO: Límites dinámicos  
    float margin = 50.0f;  
    if (screen_x < -margin || screen_x > current_render_width + margin ||   
        screen_y < -margin || screen_y > current_render_height + margin)    
        return 0;    
    
    uint32_t z_as_uint = *(uint32_t*)&z_cam;    
    return (int64_t)(((uint64_t)(uint16_t)screen_x << 48) |    
                     ((uint64_t)(uint16_t)screen_y << 32) |    
                     ((uint64_t)z_as_uint));    
}


// Función para añadir billboards directamente al sistema voxelspace  
int64_t libmod_heightmap_add_voxel_billboard(INSTANCE *my, int64_t *params) {      
    float world_x = *(float*)&params[0];      
    float world_y = *(float*)&params[1];         
    float height_offset = *(float*)&params[2];      
    int graph_id = params[3];      
    float scale = *(float*)&params[4];    
          
    // Buscar el primer heightmap válido disponible      
    HEIGHTMAP *hm = NULL;      
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {      
        if (heightmaps[i].cache_valid && heightmaps[i].width > 0 && heightmaps[i].height > 0) {      
            hm = &heightmaps[i];      
            break;      
        }      
    }      
      
    float terrain_height = 0.0f;      
    if (hm) {      
        terrain_height = get_height_at(hm, world_x, world_y);      
    }      
          
    float final_world_z = terrain_height + height_offset + 10.0f;      
          
 // USAR EL ARRAY static_billboards      
if (static_billboard_count < MAX_STATIC_BILLBOARDS) {          
    static_billboards[static_billboard_count].world_x = world_x;          
    static_billboards[static_billboard_count].world_y = world_y;          
    static_billboards[static_billboard_count].world_z = final_world_z;          
    static_billboards[static_billboard_count].graph_id = graph_id;          
    static_billboards[static_billboard_count].scale = scale;      
    static_billboards[static_billboard_count].active = 1;        
    static_billboards[static_billboard_count].process_id = 0;      
    // AGREGAR ESTA LÍNEA CRÍTICA:    
    static_billboards[static_billboard_count].billboard_type = 0; // Tipo por defecto (estático)    
              
    return static_billboard_count++;      
}          
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
   
int64_t libmod_heightmap_register_billboard(INSTANCE *my, int64_t *params) {  
    int64_t process_id = params[0];  
    float world_x = *(float*)&params[1];  
    float world_y = *(float*)&params[2];  
    float world_z = *(float*)&params[3];  
    int64_t graph_id = params[4];  
    int64_t billboard_type = params[5];  // NUEVO: tipo de billboard  
      
    // Buscar en billboards dinámicos  
    for (int i = 0; i < MAX_DYNAMIC_BILLBOARDS; i++) {  
        if (!dynamic_billboards[i].active) {  
            dynamic_billboards[i].active = 1;  
            dynamic_billboards[i].process_id = process_id;  
            dynamic_billboards[i].world_x = world_x;  
            dynamic_billboards[i].world_y = world_y;  
            dynamic_billboards[i].world_z = world_z;  
            dynamic_billboards[i].graph_id = graph_id;  
            dynamic_billboards[i].billboard_type = billboard_type;  // Almacenar tipo  
            return i + MAX_STATIC_BILLBOARDS;  
        }  
    }  
    return -1;  
}

int64_t libmod_heightmap_update_billboard(INSTANCE *my, int64_t *params) {      
    int64_t process_id = params[0];      
    float world_x = *(float*)&params[1];      
    float world_y = *(float*)&params[2];      
    float world_z = *(float*)&params[3];      
      
    for (int i = 0; i < MAX_DYNAMIC_BILLBOARDS; i++) {      
        if (dynamic_billboards[i].active && dynamic_billboards[i].process_id == process_id) {      
            dynamic_billboards[i].world_x = world_x;      
            dynamic_billboards[i].world_y = world_y;      
              
            if (dynamic_billboards[i].billboard_type > 0) {  
                HEIGHTMAP *hm = NULL;  
                for (int j = 0; j < MAX_HEIGHTMAPS; j++) {  
                    if (heightmaps[j].cache_valid && heightmaps[j].width > 0) {  
                        hm = &heightmaps[j];  
                        break;  
                    }  
                }  
                  
                if (hm) {  
                    float terrain_height = get_height_at(hm, world_x, world_y);  
                      
                    float height_offset;  
                    switch(dynamic_billboards[i].billboard_type) {  
                        case 1: height_offset = 10.0f; break;  
                        case 2: height_offset = 10.0f; break;  
                        case 3: height_offset = 5.0f; break;  
                        default: height_offset = 10.0f;  
                    }  
                      
                    float min_z = terrain_height + height_offset;  
                    if (world_z < min_z) {  
                        world_z = min_z;  
                    }  
                }  
            }  
              
            dynamic_billboards[i].world_z = world_z;  
              
            // NUEVO: Retornar el Z ajustado como entero (multiplicado por 1000 para precisión)  
            return (int64_t)(world_z * 1000.0f);  
        }      
    }      
    return 0;    
}

int64_t libmod_heightmap_update_billboard_graph(INSTANCE *my, int64_t *params) {  
    int64_t process_id = params[0];  
    int64_t new_graph_id = params[1];  
      
    for (int i = 0; i < MAX_DYNAMIC_BILLBOARDS; i++) {  
        if (dynamic_billboards[i].active && dynamic_billboards[i].process_id == process_id) {  
            dynamic_billboards[i].graph_id = new_graph_id;  
            return 1;  
        }  
    }  
    return 0;  
}

int64_t libmod_heightmap_unregister_billboard(INSTANCE *my, int64_t *params) {      
    int64_t process_id = params[0];      
          
    for (int i = 0; i < MAX_DYNAMIC_BILLBOARDS; i++) {      
        if (dynamic_billboards[i].active && dynamic_billboards[i].process_id == process_id) {      
            dynamic_billboards[i].active = 0;      
            dynamic_billboards[i].process_id = 0; // Limpiar process_id    
            return 1; // Éxito      
        }      
    }      
    return 0; // No encontrado      
}

int64_t libmod_heightmap_set_wave_amplitude(INSTANCE *my, int64_t *params)    
{    
    float amplitude = *(float*)&params[0];    
    if (amplitude < 0.0f) amplitude = 0.0f;    
    if (amplitude > 50.0f) amplitude = 150.0f; // CAMBIO: Límite ampliado para tsunamis  
    wave_amplitude = amplitude;    
    return 1;    
}

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

int64_t libmod_heightmap_set_fog_color(INSTANCE *my, int64_t *params)  
{  
    Uint8 new_r = (Uint8)params[0];  
    Uint8 new_g = (Uint8)params[1];  
    Uint8 new_b = (Uint8)params[2];  
    float new_intensity = (float)params[3] / 1000.0f;  
      
    // Invalidar tabla si los parámetros cambiaron  
    if (new_r != fog_color_r || new_g != fog_color_g ||   
        new_b != fog_color_b || new_intensity != fog_intensity) {  
        fog_table_initialized = 0;  
    }  
      
    fog_color_r = new_r;  
    fog_color_g = new_g;  
    fog_color_b = new_b;  
    fog_intensity = new_intensity;  
      
    return 1;  
}


int64_t libmod_heightmap_set_billboard_fov(INSTANCE *my, int64_t *params) {  
    billboard_render_fov = (float)params[0] / 1000.0f;  // Convertir de milésimas  
    if (billboard_render_fov < 0.5f) billboard_render_fov = 0.5f;  
    if (billboard_render_fov > 3.0f) billboard_render_fov = 3.0f;  
    return 1;  
}


// Funciones helper internas - NO exportadas  
static void move_camera_direction(float angle_offset, float speed_factor, float speed) {  
    float final_speed = (speed > 0) ? speed : move_speed;  
    camera.x += cosf(camera.angle + angle_offset) * final_speed / speed_factor;  
    camera.y += sinf(camera.angle + angle_offset) * final_speed / speed_factor;  
}  
  
// Funciones exportadas simplificadas  
int64_t libmod_heightmap_move_forward(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_direction(0.0f, 1.5f, speed);  
    return 1;  
}  
  
int64_t libmod_heightmap_move_backward(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_direction(M_PI, 1.5f, speed);  
    return 1;  
}  
  
int64_t libmod_heightmap_strafe_left(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_direction(-M_PI_2, 2.0f, speed);  
    return 1;  
}  
  
int64_t libmod_heightmap_strafe_right(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_direction(M_PI_2, 2.0f, speed);  
    return 1;  
}


// Helper interno para movimiento con colisión - NO exportado  
static int move_camera_with_collision(int64_t hm_id, float angle_offset, float speed_factor, float speed) {  
    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
    if (!hm || !hm->cache_valid) return 0;  
      
    float new_x = camera.x + cosf(camera.angle + angle_offset) * speed / speed_factor;  
    float new_y = camera.y + sinf(camera.angle + angle_offset) * speed / speed_factor;  
      
    return apply_terrain_collision(hm, new_x, new_y);  
}  
  
// Helper para búsqueda de heightmap  
static HEIGHTMAP* find_heightmap_by_id(int64_t hm_id) {  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == hm_id) {  
            return &heightmaps[i];  
        }  
    }  
    return NULL;  
}  
  
// Helper para aplicar colisión con terreno  
static int apply_terrain_collision(HEIGHTMAP *hm, float new_x, float new_y) {  
    if (new_x >= 2.0f && new_x < hm->width - 2.0f &&  
        new_y >= 2.0f && new_y < hm->height - 2.0f) {  
          
        float terrain_height = get_height_at(hm, new_x, new_y);  
        float current_terrain_height = get_height_at(hm, camera.x, camera.y);  
        float height_diff = fabs(terrain_height - current_terrain_height);  
          
        if (height_diff < 20.0f) {  
            camera.x = new_x;  
            camera.y = new_y;  
              
            float min_height = terrain_height + 5.0f;  
            if (camera.z < min_height) {  
                camera.z = min_height;  
            }  
        }  
    }  
    return 1;  
}

int64_t libmod_heightmap_move_forward_with_collision(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;  
    return move_camera_with_collision(hm_id, 0.0f, 1.5f, speed);  
}  
  
int64_t libmod_heightmap_move_backward_with_collision(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;  
    return move_camera_with_collision(hm_id, M_PI, 1.5f, speed);  
}  
  
int64_t libmod_heightmap_strafe_left_with_collision(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;  
    return move_camera_with_collision(hm_id, -M_PI_2, 2.0f, speed);  
}  
  
int64_t libmod_heightmap_strafe_right_with_collision(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;  
    return move_camera_with_collision(hm_id, M_PI_2, 2.0f, speed);  
}

static float convert_screen_to_world_coordinate(int heightmap_id, float screen_coord, int is_x_axis) {  
    HEIGHTMAP *hm = find_heightmap_by_id(heightmap_id);  
    if (!hm) return 0.0f;  
      
    float reference_size = is_x_axis ? 640.0f : 480.0f;  
    float map_size = is_x_axis ? hm->width : hm->height;  
    float factor = map_size / reference_size;  
      
    return screen_coord / factor;  
}

float libmod_heightmap_convert_screen_to_world_x(INSTANCE *my, int64_t *params) {  
    int heightmap_id = params[0];  
    float screen_x = *(float*)&params[1];  
    return convert_screen_to_world_coordinate(heightmap_id, screen_x, 1); // 1 = eje X  
}  
  
float libmod_heightmap_convert_screen_to_world_y(INSTANCE *my, int64_t *params) {  
    int heightmap_id = params[0];  
    float screen_y = *(float*)&params[1];  
    return convert_screen_to_world_coordinate(heightmap_id, screen_y, 0); // 0 = eje Y  
}

static void collect_visible_billboards_from_array(VOXEL_BILLBOARD *billboard_array, int array_size,   
                                                  BILLBOARD_RENDER_DATA *visible_billboards,   
                                                  int *visible_count, float terrain_fov) {  
    for (int i = 0; i < array_size; i++) {  
        if (!billboard_array[i].active) continue;  
          
        VOXEL_BILLBOARD *bb = &billboard_array[i];  
        GRAPH *billboard_graph = bitmap_get(0, bb->graph_id);  
          
        if (!billboard_graph) continue;  
          
        BILLBOARD_PROJECTION proj = calculate_proyection(bb, billboard_graph, terrain_fov);  
        if (!proj.valid) continue;  
          
        visible_billboards[*visible_count].billboard = bb;  
        visible_billboards[*visible_count].projection = proj;  
        visible_billboards[*visible_count].graph = billboard_graph;  
        visible_billboards[*visible_count].distance = proj.distance;  
        (*visible_count)++;  
    }  
}

int64_t libmod_heightmap_set_render_resolution(INSTANCE *my, int64_t *params) {  
    int64_t width = params[0];  
    int64_t height = params[1];  
      
    if (width < 160 || width > 1920) {  
        fprintf(stderr, "Error: render_width debe estar entre 160 y 1920\n");  
        return 0;  
    }  
      
    if (height < 120 || height > 1080) {  
        fprintf(stderr, "Error: render_height debe estar entre 120 y 1080\n");  
        return 0;  
    }  
      
    current_render_width = width;  
    current_render_height = height;  
      
    // Forzar recreación del render_buffer en el próximo frame  
    if (render_buffer) {  
        bitmap_destroy(render_buffer);  
        render_buffer = NULL;  
    }  
      
    return 1;  
}

/* Obtener el tipo de mapa (heightmap o sector) */  
int64_t libmod_heightmap_get_map_type(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
      
    // Buscar el mapa  
    HEIGHTMAP *hm = NULL;  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == hm_id) {  
            hm = &heightmaps[i];  
            break;  
        }  
    }  
      
    if (!hm) {  
        fprintf(stderr, "Error: Mapa con ID %" PRId64 " no encontrado\n", hm_id);  
        return -1;  
    }  
      
    return (int64_t)hm->type;  
}

///    MAPAS POR SECTORES    ///

/* Cargar archivo .dmap binario (mapas estilo Doom) con texturas empaquetadas */      
int64_t libmod_heightmap_load_dmap(INSTANCE *my, int64_t *params) {        
    const char *filename = string_get(params[0]);        
    FILE *file = fopen(filename, "rb");        
            
    if (!file) {        
        fprintf(stderr, "Error: No se pudo abrir archivo .dmap: %s\n", filename);        
        string_discard(params[0]);        
        return 0;        
    }        
            
    // Leer y validar header DMAP v2    
    DMAP_HEADER_V2 header;    
    memset(&header, 0, sizeof(DMAP_HEADER_V2));    
        
    if (fread(&header, sizeof(DMAP_HEADER_V2), 1, file) != 1) {    
        fprintf(stderr, "Error: No se pudo leer header de .dmap\n");    
        fclose(file);    
        string_discard(params[0]);    
        return 0;    
    }    
          
    // DEBUG: Verificar header      
    fprintf(stderr, "DEBUG LOAD: Header leído - magic=%.4s version=%u sectors=%u walls=%u textures=%u\n",      
            header.magic, header.version, header.num_sectors, header.num_walls, header.num_textures);    
        
    fprintf(stderr, "DEBUG LOAD: Header v2 - vertices=%u sidedefs=%u things=%u bsp_nodes=%u blockmap=%ux%u\n",    
            header.num_vertices, header.num_sidedefs, header.num_things,     
            header.num_bsp_nodes, header.blockmap_width, header.blockmap_height);    
            
    // Validar magic number        
    if (memcmp(header.magic, "DMAP", 4) != 0) {        
        fprintf(stderr, "Error: Archivo no es un .dmap válido (magic incorrecto)\n");        
        fclose(file);        
        string_discard(params[0]);        
        return 0;        
    }        
            
    // Validar versión (solo v2)  
    if (header.version != 2) {        
        fprintf(stderr, "Error: Solo se soporta DMAP v2. Versión encontrada: %u\n", header.version);        
        fclose(file);        
        string_discard(params[0]);        
        return 0;        
    }        
            
    // Buscar slot libre en el array de heightmaps        
    HEIGHTMAP *map = NULL;        
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {        
        if (heightmaps[i].id == 0) {        
            map = &heightmaps[i];        
            map->id = next_heightmap_id++;        
            break;        
        }        
    }        
            
    if (!map) {        
        fprintf(stderr, "Error: No hay slots disponibles para mapas\n");        
        fclose(file);        
        string_discard(params[0]);        
        return 0;        
    }        
            
    // Inicializar como mapa de sectores        
    map->type = MAP_TYPE_SECTOR;        
    map->num_sectors = header.num_sectors;        
    map->num_walls = header.num_walls;        
    map->num_textures = header.num_textures;    
        
    // Inicializar campos DMAP v2    
    map->vertices = NULL;    
    map->num_vertices = 0;    
    map->sidedefs = NULL;    
    map->num_sidedefs = 0;    
    map->things = NULL;    
    map->num_things = 0;    
    map->bsp_nodes = NULL;    
    map->num_bsp_nodes = 0;    
    map->subsectors = NULL;    
    map->num_subsectors = 0;    
    map->segs = NULL;    
    map->num_segs = 0;    
    map->blockmap_grid = NULL;    
    map->blockmap_width = 0;    
    map->blockmap_height = 0;    
    map->blockmap_cell_size = 0;


    // ========================================    
    // LEER TEXTURAS (REFERENCIAS A ARCHIVOS)    
    // ========================================    
        
    map->textures = malloc(sizeof(TEXTURE_ENTRY) * header.num_textures);        
    if (!map->textures) {        
        fprintf(stderr, "Error: No se pudo asignar memoria para texturas\n");        
        fclose(file);        
        string_discard(params[0]);        
        return 0;        
    }        
          
    fprintf(stderr, "DEBUG LOAD: Leyendo %u referencias de texturas...\n", header.num_textures);    
        
    // DMAP v2: Texturas como nombres de archivo (sin empaquetar)    
    for (uint32_t i = 0; i < header.num_textures; i++) {    
        // Leer header de textura v2 (solo filename)    
        TEXTURE_ENTRY_V2 tex_header;    
        if (fread(&tex_header, sizeof(TEXTURE_ENTRY_V2), 1, file) != 1) {    
            fprintf(stderr, "Error: No se pudo leer textura %u\n", i);    
            free(map->textures);    
            fclose(file);    
            string_discard(params[0]);    
            return 0;    
        }    
            
        // Cargar el GRAPH de BennuGD2 desde archivo externo    
        strncpy(map->textures[i].filename, tex_header.filename, 255);    
        map->textures[i].filename[255] = '\0';    
        map->textures[i].graph_id = gr_load_img(map->textures[i].filename);    
            
        if (map->textures[i].graph_id == 0) {    
            fprintf(stderr, "Advertencia: No se pudo cargar textura: %s\n",    
                    map->textures[i].filename);    
        } else {    
            fprintf(stderr, "Textura %u: %s -> graph_id=%d\n",    
                    i, map->textures[i].filename, map->textures[i].graph_id);    
        }    
    }
      // ========================================    
    // LEER VÉRTICES COMPARTIDOS    
    // ========================================    
        
    map->num_vertices = header.num_vertices;    
    map->vertices = malloc(sizeof(VERTEX) * header.num_vertices);    
    if (!map->vertices) {    
        fprintf(stderr, "Error: No se pudo asignar memoria para vértices\n");    
        // Liberar texturas ya cargadas    
        for (uint32_t i = 0; i < header.num_textures; i++) {    
            if (map->textures[i].graph_id > 0) {    
                bitmap_destroy(bitmap_get(0, map->textures[i].graph_id));    
            }    
        }    
        free(map->textures);    
        fclose(file);    
        string_discard(params[0]);    
        return 0;    
    }    
        
    for (uint32_t i = 0; i < header.num_vertices; i++) {    
        if (fread(&map->vertices[i], sizeof(VERTEX), 1, file) != 1) {    
            fprintf(stderr, "Error: No se pudo leer vértice %u\n", i);    
            free(map->vertices);    
            for (uint32_t j = 0; j < header.num_textures; j++) {    
                if (map->textures[j].graph_id > 0) {    
                    bitmap_destroy(bitmap_get(0, map->textures[j].graph_id));    
                }    
            }    
            free(map->textures);    
            fclose(file);    
            string_discard(params[0]);    
            return 0;    
        }    
    }    
    fprintf(stderr, "DEBUG LOAD: %u vértices compartidos leídos\n", header.num_vertices);
     // ========================================    
    // LEER SECTORES    
    // ========================================    
        
    map->sectors = malloc(sizeof(SECTOR_V2) * header.num_sectors);    
    if (!map->sectors) {    
        fprintf(stderr, "Error: No se pudo asignar memoria para sectores v2\n");    
        if (map->vertices) free(map->vertices);    
        for (uint32_t i = 0; i < header.num_textures; i++) {    
            if (map->textures[i].graph_id > 0) {    
                bitmap_destroy(bitmap_get(0, map->textures[i].graph_id));    
            }    
        }    
        free(map->textures);    
        fclose(file);    
        string_discard(params[0]);    
        return 0;    
    }    
        
    for (uint32_t i = 0; i < header.num_sectors; i++) {    
        SECTOR_V2 *sector = &((SECTOR_V2*)map->sectors)[i];    
            
        // Leer campos básicos del sector    
        fread(&sector->id, sizeof(uint32_t), 1, file);    
        fread(&sector->floor_height, sizeof(float), 1, file);    
        fread(&sector->ceiling_height, sizeof(float), 1, file);    
        fread(&sector->floor_slope_x, sizeof(float), 1, file);    
        fread(&sector->floor_slope_y, sizeof(float), 1, file);    
        fread(&sector->ceiling_slope_x, sizeof(float), 1, file);    
        fread(&sector->ceiling_slope_y, sizeof(float), 1, file);    
        fread(&sector->is_dynamic, sizeof(uint8_t), 1, file);    
        fread(&sector->target_floor_height, sizeof(float), 1, file);    
        fread(&sector->target_ceiling_height, sizeof(float), 1, file);    
        fread(&sector->move_speed, sizeof(float), 1, file);    
        fread(&sector->move_state, sizeof(uint8_t), 1, file);    
        fread(&sector->sector_type, sizeof(uint32_t), 1, file);    
        fread(&sector->floor_texture_id, sizeof(uint32_t), 1, file);    
        fread(&sector->ceiling_texture_id, sizeof(uint32_t), 1, file);    
        fread(&sector->wall_texture_id, sizeof(uint32_t), 1, file);    
        fread(&sector->num_vertices, sizeof(uint32_t), 1, file);    
        fread(&sector->light_level, sizeof(uint8_t), 1, file);    
            
        fprintf(stderr, "Sector v2 %u: floor=%.2f ceiling=%.2f type=%u dynamic=%u vertices=%u\n",    
                i, sector->floor_height, sector->ceiling_height,     
                sector->sector_type, sector->is_dynamic, sector->num_vertices);    
            
        // Leer índices de vértices    
        sector->vertex_indices = malloc(sizeof(uint32_t) * sector->num_vertices);    
        if (!sector->vertex_indices) {    
            fprintf(stderr, "Error: No se pudo asignar memoria para índices de vértices del sector %u\n", i);    
            for (uint32_t j = 0; j < i; j++) {    
                free(((SECTOR_V2*)map->sectors)[j].vertex_indices);    
            }    
            free(map->sectors);    
            if (map->vertices) free(map->vertices);    
            for (uint32_t j = 0; j < header.num_textures; j++) {    
                if (map->textures[j].graph_id > 0) {    
                    bitmap_destroy(bitmap_get(0, map->textures[j].graph_id));    
                }    
            }    
            free(map->textures);    
            fclose(file);    
            string_discard(params[0]);    
            return 0;    
        }    
            
        for (uint32_t v = 0; v < sector->num_vertices; v++) {    
            fread(&sector->vertex_indices[v], sizeof(uint32_t), 1, file);    
        }    
    }
    // ========================================    
    // LEER PAREDES    
    // ========================================    
        
    map->walls = malloc(sizeof(WALL_V2) * header.num_walls);    
    if (!map->walls) {    
        fprintf(stderr, "Error: No se pudo asignar memoria para paredes\n");    
        for (uint32_t i = 0; i < header.num_sectors; i++) {    
            free(((SECTOR_V2*)map->sectors)[i].vertex_indices);    
        }    
        free(map->sectors);    
        if (map->vertices) free(map->vertices);    
        for (uint32_t i = 0; i < header.num_textures; i++) {    
            if (map->textures[i].graph_id > 0) {    
                bitmap_destroy(bitmap_get(0, map->textures[i].graph_id));    
            }    
        }    
        free(map->textures);    
        fclose(file);    
        string_discard(params[0]);    
        return 0;    
    }    
        
    // Leer paredes v2 con campos extendidos    
    for (uint32_t i = 0; i < header.num_walls; i++) {    
        WALL_V2 *wall = &((WALL_V2*)map->walls)[i];    
            
        fread(&wall->sector1_id, sizeof(uint32_t), 1, file);    
        fread(&wall->sector2_id, sizeof(uint32_t), 1, file);    
        fread(&wall->vertex1_index, sizeof(uint32_t), 1, file);    
        fread(&wall->vertex2_index, sizeof(uint32_t), 1, file);    
        fread(&wall->x1, sizeof(float), 1, file);    
        fread(&wall->y1, sizeof(float), 1, file);    
        fread(&wall->x2, sizeof(float), 1, file);    
        fread(&wall->y2, sizeof(float), 1, file);    
        fread(&wall->sidedef1_id, sizeof(uint32_t), 1, file);    
        fread(&wall->sidedef2_id, sizeof(uint32_t), 1, file);    
        fread(&wall->flags, sizeof(uint8_t), 1, file);    
        fread(&wall->action_type, sizeof(uint32_t), 1, file);    
    }    
    fprintf(stderr, "DEBUG LOAD: %u paredes leídas\n", header.num_walls);
    // ========================================    
    // LEER SIDEDEFS    
    // ========================================    
        
    map->num_sidedefs = header.num_sidedefs;    
    map->sidedefs = malloc(sizeof(SIDEDEF) * header.num_sidedefs);    
    if (!map->sidedefs) {    
        fprintf(stderr, "Error: No se pudo asignar memoria para sidedefs\n");    
        free(map->walls);    
        for (uint32_t i = 0; i < header.num_sectors; i++) {    
            free(((SECTOR_V2*)map->sectors)[i].vertex_indices);    
        }    
        free(map->sectors);    
        if (map->vertices) free(map->vertices);    
        for (uint32_t i = 0; i < header.num_textures; i++) {    
            if (map->textures[i].graph_id > 0) {    
                bitmap_destroy(bitmap_get(0, map->textures[i].graph_id));    
            }    
        }    
        free(map->textures);    
        fclose(file);    
        string_discard(params[0]);    
        return 0;    
    }    
        
    for (uint32_t i = 0; i < header.num_sidedefs; i++) {    
        fread(&map->sidedefs[i], sizeof(SIDEDEF), 1, file);    
    }    
    fprintf(stderr, "DEBUG LOAD: %u sidedefs leídos\n", header.num_sidedefs);    
        
    // ========================================    
    // LEER THINGS    
    // ========================================    
        
    map->num_things = header.num_things;    
    map->things = malloc(sizeof(THING) * header.num_things);    
    if (!map->things) {    
        fprintf(stderr, "Error: No se pudo asignar memoria para things\n");    
        if (map->sidedefs) free(map->sidedefs);    
        free(map->walls);    
        for (uint32_t i = 0; i < header.num_sectors; i++) {    
            free(((SECTOR_V2*)map->sectors)[i].vertex_indices);    
        }    
        free(map->sectors);    
        if (map->vertices) free(map->vertices);    
        for (uint32_t i = 0; i < header.num_textures; i++) {    
            if (map->textures[i].graph_id > 0) {    
                bitmap_destroy(bitmap_get(0, map->textures[i].graph_id));    
            }    
        }    
        free(map->textures);    
        fclose(file);    
        string_discard(params[0]);    
        return 0;    
    }    
        
    for (uint32_t i = 0; i < header.num_things; i++) {    
        fread(&map->things[i], sizeof(THING), 1, file);    
    }    
    fprintf(stderr, "DEBUG LOAD: %u things leídos\n", header.num_things);    
        
    // ========================================    
    // LEER BSP NODES    
    // ========================================    
        
    map->num_bsp_nodes = header.num_bsp_nodes;    
    map->bsp_nodes = malloc(sizeof(BSP_NODE) * header.num_bsp_nodes);    
    if (!map->bsp_nodes) {    
        fprintf(stderr, "Error: No se pudo asignar memoria para BSP nodes\n");    
        if (map->things) free(map->things);    
        if (map->sidedefs) free(map->sidedefs);    
        free(map->walls);    
        for (uint32_t i = 0; i < header.num_sectors; i++) {    
            free(((SECTOR_V2*)map->sectors)[i].vertex_indices);    
        }    
        free(map->sectors);    
        if (map->vertices) free(map->vertices);    
        for (uint32_t i = 0; i < header.num_textures; i++) {    
            if (map->textures[i].graph_id > 0) {    
                bitmap_destroy(bitmap_get(0, map->textures[i].graph_id));    
            }    
        }    
        free(map->textures);    
        fclose(file);    
        string_discard(params[0]);    
        return 0;    
    }    
        
    for (uint32_t i = 0; i < header.num_bsp_nodes; i++) {    
        fread(&map->bsp_nodes[i], sizeof(BSP_NODE), 1, file);    
    }    
    fprintf(stderr, "DEBUG LOAD: %u BSP nodes leídos\n", header.num_bsp_nodes);   
    for (uint32_t i = 0; i < header.num_bsp_nodes; i++) {    
    fread(&map->bsp_nodes[i], sizeof(BSP_NODE), 1, file);  
    fprintf(stderr, "DMAP READ: BSP Node %u - front_child=%d back_child=%d\n",  
            i, map->bsp_nodes[i].front_child, map->bsp_nodes[i].back_child);  
    fflush(stderr);  
}
    // ========================================      
    // SUBSECTORS (solo DMAP v2)      
    // ========================================      
          
    if (header.num_subsectors > 0) {      
        map->num_subsectors = header.num_subsectors;      
        map->subsectors = malloc(sizeof(SUBSECTOR) * header.num_subsectors);      
        if (!map->subsectors) {      
            fprintf(stderr, "Error: No se pudo asignar memoria para subsectors\n");      
            if (map->bsp_nodes) free(map->bsp_nodes);      
            if (map->things) free(map->things);      
            if (map->sidedefs) free(map->sidedefs);      
            free(map->walls);      
            for (uint32_t i = 0; i < header.num_sectors; i++) {      
                free(((SECTOR_V2*)map->sectors)[i].vertex_indices);      
            }      
            free(map->sectors);      
            if (map->vertices) free(map->vertices);      
            for (uint32_t i = 0; i < header.num_textures; i++) {      
                if (map->textures[i].graph_id > 0) {      
                    bitmap_destroy(bitmap_get(0, map->textures[i].graph_id));      
                }      
            }      
            free(map->textures);      
            fclose(file);      
            string_discard(params[0]);      
            return 0;      
        }      
              
        for (uint32_t i = 0; i < header.num_subsectors; i++) {      
            fread(&map->subsectors[i], sizeof(SUBSECTOR), 1, file);      
        }      
        fprintf(stderr, "DEBUG LOAD: %u subsectors leídos\n", header.num_subsectors);      
        fflush(stderr);  
    }  
  
    // ========================================      
    // SEGS (solo DMAP v2)      
    // ========================================      
          
    if (header.num_segs > 0) {      
        map->num_segs = header.num_segs;      
        map->segs = malloc(sizeof(SEG) * header.num_segs);      
        if (!map->segs) {      
            fprintf(stderr, "Error: No se pudo asignar memoria para segs\n");      
            if (map->subsectors) free(map->subsectors);      
            if (map->bsp_nodes) free(map->bsp_nodes);      
            if (map->things) free(map->things);      
            if (map->sidedefs) free(map->sidedefs);      
            free(map->walls);      
            for (uint32_t i = 0; i < header.num_sectors; i++) {      
                free(((SECTOR_V2*)map->sectors)[i].vertex_indices);      
            }      
            free(map->sectors);      
            if (map->vertices) free(map->vertices);      
            for (uint32_t i = 0; i < header.num_textures; i++) {      
                if (map->textures[i].graph_id > 0) {      
                    bitmap_destroy(bitmap_get(0, map->textures[i].graph_id));      
                }      
            }      
            free(map->textures);      
            fclose(file);      
            string_discard(params[0]);      
            return 0;      
        }      
              
        for (uint32_t i = 0; i < header.num_segs; i++) {      
            fread(&map->segs[i], sizeof(SEG), 1, file);      
        }      
        fprintf(stderr, "DEBUG LOAD: %u segs leídos\n", header.num_segs);      
        fflush(stderr);  
    }  

    
        
    // ========================================    
    // LEER BLOCKMAP    
    // ========================================    
        
    map->blockmap_width = header.blockmap_width;    
    map->blockmap_height = header.blockmap_height;    
        
    uint32_t blockmap_size = header.blockmap_width * header.blockmap_height;    
    map->blockmap_grid = malloc(sizeof(BLOCKMAP_CELL) * blockmap_size);    
    if (!map->blockmap_grid) {    
        fprintf(stderr, "Error: No se pudo asignar memoria para blockmap\n");    
        if (map->bsp_nodes) free(map->bsp_nodes);    
        if (map->things) free(map->things);    
        if (map->sidedefs) free(map->sidedefs);    
        free(map->walls);    
        for (uint32_t i = 0; i < header.num_sectors; i++) {    
            free(((SECTOR_V2*)map->sectors)[i].vertex_indices);    
        }    
        free(map->sectors);    
        if (map->vertices) free(map->vertices);    
        for (uint32_t i = 0; i < header.num_textures; i++) {    
            if (map->textures[i].graph_id > 0) {    
                bitmap_destroy(bitmap_get(0, map->textures[i].graph_id));    
            }    
        }    
        free(map->textures);    
        fclose(file);    
        string_discard(params[0]);    
        return 0;    
    }    
        
    for (uint32_t i = 0; i < blockmap_size; i++) {    
        fread(&map->blockmap_grid[i].num_walls, sizeof(uint32_t), 1, file);    
            
        if (map->blockmap_grid[i].num_walls > 0) {    
            map->blockmap_grid[i].wall_indices = malloc(sizeof(uint32_t) * map->blockmap_grid[i].num_walls);    
            if (!map->blockmap_grid[i].wall_indices) {    
                fprintf(stderr, "Error: No se pudo asignar memoria para wall_indices en blockmap cell %u\n", i);    
                // Liberar celdas anteriores    
                for (uint32_t j = 0; j < i; j++) {    
                    if (map->blockmap_grid[j].wall_indices) {    
                        free(map->blockmap_grid[j].wall_indices);    
                    }    
                }    
                free(map->blockmap_grid);    
                if (map->bsp_nodes) free(map->bsp_nodes);    
                if (map->things) free(map->things);    
                if (map->sidedefs) free(map->sidedefs);    
                free(map->walls);    
                for (uint32_t k = 0; k < header.num_sectors; k++) {    
                    free(((SECTOR_V2*)map->sectors)[k].vertex_indices);    
                }    
                free(map->sectors);    
                if (map->vertices) free(map->vertices);    
                for (uint32_t k = 0; k < header.num_textures; k++) {    
                    if (map->textures[k].graph_id > 0) {    
                        bitmap_destroy(bitmap_get(0, map->textures[k].graph_id));    
                    }    
                }    
                free(map->textures);    
                fclose(file);    
                string_discard(params[0]);    
                return 0;    
            }    
                
            fread(map->blockmap_grid[i].wall_indices, sizeof(uint32_t),     
                  map->blockmap_grid[i].num_walls, file);    
        } else {    
            map->blockmap_grid[i].wall_indices = NULL;    
        }    
    }    
    fprintf(stderr, "DEBUG LOAD: Blockmap %ux%u leído\n",     
            header.blockmap_width, header.blockmap_height);
    // ========================================    
    // CIERRE Y MENSAJE DE ÉXITO    
    // ========================================    
        
    fclose(file);    
    string_discard(params[0]);    
        
    printf("Mapa DMAP v2 cargado exitosamente:\n");    
    printf("  - %u sectores\n", header.num_sectors);    
    printf("  - %u paredes\n", header.num_walls);    
    printf("  - %u texturas\n", header.num_textures);    
    printf("  - %u vértices compartidos\n", header.num_vertices);    
    printf("  - %u sidedefs\n", header.num_sidedefs);    
    printf("  - %u things\n", header.num_things);    
    printf("  - %u nodos BSP\n", header.num_bsp_nodes);    
        
    return map->id;    
}

// Proyectar un segmento de pared desde coordenadas mundo a pantalla  
static WALL_PROJECTION project_wall_segment(float x1, float y1, float x2, float y2,   
                                             float floor_height, float ceiling_height) {  
    WALL_PROJECTION proj = {0};  
      
    // Transformar vértices a espacio de cámara  
    float dx1 = x1 - camera.x;  
    float dy1 = y1 - camera.y;  
    float dx2 = x2 - camera.x;  
    float dy2 = y2 - camera.y;  
      
    // Rotar según el ángulo de la cámara  
    float cos_angle = cosf(-camera.angle);  
    float sin_angle = sinf(-camera.angle);  
      
    float tx1 = dx1 * cos_angle - dy1 * sin_angle;  
    float tz1 = dx1 * sin_angle + dy1 * cos_angle;  
    float tx2 = dx2 * cos_angle - dy2 * sin_angle;  
    float tz2 = dx2 * sin_angle + dy2 * cos_angle;  
      
    // Culling: descartar si ambos puntos están detrás de la cámara  
    if (tz1 <= 0.1f && tz2 <= 0.1f) {  
        proj.valid = 0;  
        return proj;  
    }  
      
    // Clipping: si un punto está detrás, clipear al near plane  
    if (tz1 <= 0.1f) {  
        float t = (0.1f - tz1) / (tz2 - tz1);  
        tx1 = tx1 + t * (tx2 - tx1);  
        tz1 = 0.1f;  
    }  
    if (tz2 <= 0.1f) {  
        float t = (0.1f - tz2) / (tz1 - tz2);  
        tx2 = tx2 + t * (tx1 - tx2);  
        tz2 = 0.1f;  
    }  
      
    // Proyectar a coordenadas de pantalla  
    float fov = 0.7f;  // Campo de visión  
    int screen_width = current_render_width;  
    int half_width = screen_width / 2;  
      
    proj.x1_screen = half_width + (int)((tx1 / tz1) * half_width / tanf(fov * 0.5f));  
    proj.x2_screen = half_width + (int)((tx2 / tz2) * half_width / tanf(fov * 0.5f));  
      
    // Clamping a límites de pantalla  
    if (proj.x1_screen < 0) proj.x1_screen = 0;  
    if (proj.x2_screen >= screen_width) proj.x2_screen = screen_width - 1;  
      
    // Descartar si el segmento está fuera de pantalla  
    if (proj.x1_screen >= screen_width || proj.x2_screen < 0) {  
        proj.valid = 0;  
        return proj;  
    }  
      
    // Guardar distancias y altura  
    proj.z1 = tz1;  
    proj.z2 = tz2;  
    proj.wall_height = ceiling_height - floor_height;  
    proj.valid = 1;  
      
    return proj;  
}

static void render_wall_column(int screen_x, float distance, float wall_height,   
                                float floor_height, uint32_t wall_color,   
                                float *depth_buffer) {  
    int screen_height = current_render_height;  
    int half_height = screen_height / 2;  
      
    // Calcular altura proyectada de la pared en pantalla  
    float projected_height = (wall_height / distance) * PROJECTION_HEIGHT_SCALE;  
      
    // Calcular posiciones Y de techo y piso  
    float wall_top_y = half_height - (camera.z - (floor_height + wall_height)) / distance * PROJECTION_HEIGHT_SCALE;  
    float wall_bottom_y = half_height - (camera.z - floor_height) / distance * PROJECTION_HEIGHT_SCALE;  
      
    // Aplicar pitch de cámara  
    wall_top_y += camera.pitch * 40.0f;  
    wall_bottom_y += camera.pitch * 40.0f;  
      
    int y_start = (int)wall_top_y;  
    int y_end = (int)wall_bottom_y;  
      
    // Clamping  
    if (y_start < 0) y_start = 0;  
    if (y_end >= screen_height) y_end = screen_height - 1;  
      
    // Calcular fog  
    float fog = 1.0f - (distance / max_render_distance);  
    if (fog < 0.3f) fog = 0.3f;  
      
    // Aplicar lighting  
    float light_factor = light_intensity / 255.0f;  
    uint8_t r = ((wall_color >> 16) & 0xFF) * fog * light_factor;  
    uint8_t g = ((wall_color >> 8) & 0xFF) * fog * light_factor;  
    uint8_t b = (wall_color & 0xFF) * fog * light_factor;  
      
    uint32_t final_color = SDL_MapRGBA(gPixelFormat, r, g, b, 255);  
      
    // Renderizar columna con depth testing  
    for (int y = y_start; y <= y_end; y++) {  
        int depth_index = y * current_render_width + screen_x;  
          
        if (distance < depth_buffer[depth_index]) {  
            gr_put_pixel(render_buffer, screen_x, y, final_color);  
            depth_buffer[depth_index] = distance;  
        }  
    }  
}

int64_t libmod_heightmap_render_sector_cpu(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    int64_t render_width = params[1];  
    int64_t render_height = params[2];  
      
    float fov = 0.7f;  // FOV en radianes (~40 grados)  
      
    // Buscar heightmap  
    HEIGHTMAP *hm = NULL;  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == hm_id) {  
            hm = &heightmaps[i];  
            break;  
        }  
    }  
      
    if (!hm || hm->type != MAP_TYPE_SECTOR) {  
        return 0;  
    }  
      
    // Buffer estático local para evitar conflictos con GPU renderer  
    static GRAPH *cpu_render_buffer = NULL;  
    static int64_t last_width = 0;  
    static int64_t last_height = 0;  
      
    // Crear o redimensionar buffer si es necesario  
    if (!cpu_render_buffer || last_width != render_width || last_height != render_height) {  
        if (cpu_render_buffer) {  
            bitmap_destroy(cpu_render_buffer);  
        }  
        cpu_render_buffer = bitmap_new_syslib(render_width, render_height);  
        if (!cpu_render_buffer) {  
            return 0;  
        }  
        last_width = render_width;  
        last_height = render_height;  
    }  
      
    // Depth buffer estático con gestión dinámica  
    static float *depth_buffer = NULL;  
    static size_t depth_buffer_size = 0;  
    size_t required_size = (size_t)render_width * (size_t)render_height;  
      
    if (depth_buffer_size < required_size) {  
        if (depth_buffer) {  
            free(depth_buffer);  
        }  
        depth_buffer = (float*)malloc(required_size * sizeof(float));  
        if (!depth_buffer) {  
            return 0;  
        }  
        depth_buffer_size = required_size;  
    }  
      
    // Inicializar depth buffer  
    for (size_t i = 0; i < required_size; i++) {  
        depth_buffer[i] = max_render_distance;  
    }  
      
    // Variables declaradas UNA SOLA VEZ aquí  
    size_t buffer_index;  
    size_t buffer_size = required_size;  
      
    // Limpiar buffer con color de cielo  
    uint32_t sky_color = SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, 255);  
    gr_clear_as(cpu_render_buffer, sky_color);  
      
    // Obtener texturas EMPAQUETADAS (ya cargadas en memoria como GRAPHs)  
    GRAPH *wall_texture = NULL;  
    GRAPH *floor_texture = NULL;  
    GRAPH *ceiling_texture = NULL;  
      
    if (hm->num_textures > 0 && hm->textures) {  
        // Las texturas ya están cargadas como GRAPHs desde datos empaquetados  
        if (hm->textures[0].graph_id > 0) {  
            wall_texture = bitmap_get(0, hm->textures[0].graph_id);  
        }  
        if (hm->num_textures > 1 && hm->textures[1].graph_id > 0) {  
            floor_texture = bitmap_get(0, hm->textures[1].graph_id);  
        }  
        if (hm->num_textures > 2 && hm->textures[2].graph_id > 0) {  
            ceiling_texture = bitmap_get(0, hm->textures[2].graph_id);  
        }  
    }  
  
    // Precalcular valores de rotación de cámara (UNA SOLA VEZ)  
    float cos_cam = cosf(-camera.angle);  
    float sin_cam = sinf(-camera.angle);  
      
    // Calcular projection_scale basado en FOV  
    float projection_scale = (render_width / 2.0f) / tanf(fov / 2.0f);
    // ============================================================  
    // PARTE 2: RENDERIZADO DE PAREDES CON TEXTURAS EMPAQUETADAS  
    // ============================================================  
      
    int walls_rendered = 0;  
      
    // Si hay árbol BSP, usarlo para renderizado ordenado  
    if (hm->num_bsp_nodes > 0 && hm->bsp_nodes) {  
        render_bsp_node(hm, 0, camera.x, camera.y,  
                       render_width, render_height,  
                       projection_scale, fov,  
                       cos_cam, sin_cam,  
                       depth_buffer, cpu_render_buffer,  
                       &walls_rendered);  
    } else {  
        // Fallback: renderizar sectores directamente sin BSP  
        for (int s = 0; s < hm->num_sectors; s++) {  
            SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[s];  
              
            if (sector->num_vertices < 2) {  
                continue;  
            }  
              
            // Renderizar cada pared del sector  
            for (uint32_t i = 0; i < sector->num_vertices; i++) {  
                uint32_t next_i = (i + 1) % sector->num_vertices;  
                  
                // Obtener vértices desde el pool global usando índices  
                VERTEX *v1 = &hm->vertices[sector->vertex_indices[i]];  
                VERTEX *v2 = &hm->vertices[sector->vertex_indices[next_i]];  
                  
                float x1 = v1->x;  
                float y1 = v1->y;  
                float x2 = v2->x;  
                float y2 = v2->y;  
                  
                // Transformar a espacio de cámara  
                float dx1 = x1 - camera.x;  
                float dy1 = y1 - camera.y;  
                float dx2 = x2 - camera.x;  
                float dy2 = y2 - camera.y;  
                  
                float cam_x1 = dx1 * cos_cam - dy1 * sin_cam;  
                float cam_z1 = dx1 * sin_cam + dy1 * cos_cam;  
                float cam_x2 = dx2 * cos_cam - dy2 * sin_cam;  
                float cam_z2 = dx2 * sin_cam + dy2 * cos_cam;  
                  
                // Clipping contra near plane  
                float near_plane = 0.1f;  
                if (cam_z1 < near_plane && cam_z2 < near_plane) {  
                    continue;  
                }  
                  
                // Proyectar a coordenadas de pantalla  
                int screen_x1, screen_x2;  
                  
                if (cam_z1 > near_plane) {  
                    screen_x1 = (int)(render_width / 2.0f + (cam_x1 / cam_z1) * projection_scale);  
                } else {  
                    screen_x1 = (cam_x1 < 0) ? -1 : render_width;  
                }  
                  
                if (cam_z2 > near_plane) {  
                    screen_x2 = (int)(render_width / 2.0f + (cam_x2 / cam_z2) * projection_scale);  
                } else {  
                    screen_x2 = (cam_x2 < 0) ? -1 : render_width;  
                }  
                  
                // Verificar que el segmento esté dentro de la pantalla  
                if (screen_x1 < 0 && screen_x2 < 0) continue;  
                if (screen_x1 >= render_width && screen_x2 >= render_width) continue;  
                  
                // Clamp a límites de pantalla  
                if (screen_x1 < 0) screen_x1 = 0;  
                if (screen_x2 >= render_width) screen_x2 = render_width - 1;  
                if (screen_x1 > screen_x2) {  
                    int temp = screen_x1;  
                    screen_x1 = screen_x2;  
                    screen_x2 = temp;  
                }  
                  
                // Renderizar columnas del segmento  
                for (int screen_x = screen_x1; screen_x <= screen_x2; screen_x++) {  
                    if (screen_x < 0 || screen_x >= render_width) continue;  
                      
                    // Interpolación lineal de profundidad  
                    float t = (screen_x2 != screen_x1) ?   
                             (float)(screen_x - screen_x1) / (float)(screen_x2 - screen_x1) : 0.0f;  
                    float depth = cam_z1 + t * (cam_z2 - cam_z1);  
                      
                    if (depth < near_plane) continue;  
                      
                    // Calcular altura de pared en pantalla  
                    float wall_height = sector->ceiling_height - sector->floor_height;  
                    float wall_top_screen = render_height / 2.0f -   
                                           ((sector->ceiling_height - camera.z) / depth) * projection_scale;  
                    float wall_bottom_screen = render_height / 2.0f -   
                                              ((sector->floor_height - camera.z) / depth) * projection_scale;  
                      
                    int wall_top = (int)wall_top_screen;  
                    int wall_bottom = (int)wall_bottom_screen;  
                      
                    // Clamp a límites de pantalla  
                    if (wall_top < 0) wall_top = 0;  
                    if (wall_bottom >= render_height) wall_bottom = render_height - 1;  
                      
                    if (wall_top >= wall_bottom) continue;  
                      
                    // Obtener textura de pared  
                    GRAPH *current_wall_texture = NULL;  
                    if (wall_texture && wall_texture->width > 0 && wall_texture->height > 0) {  
                        current_wall_texture = wall_texture;  
                    }  
                      
                    // Renderizar píxeles de la pared  
                    for (int screen_y = wall_top; screen_y < wall_bottom; screen_y++) {  
                        buffer_index = (size_t)screen_y * (size_t)render_width + (size_t)screen_x;  
                          
                        if (buffer_index >= buffer_size) continue;  
                        if (depth >= depth_buffer[buffer_index]) continue;  
                          
                        uint32_t color;  
                          
                        if (current_wall_texture) {  
                            // Calcular coordenadas de textura  
                            float wall_length = sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));  
                            float u = (t * wall_length) / 64.0f;  
                            u = u - floorf(u);  
                              
                            float v = (float)(screen_y - wall_top) / (float)(wall_bottom - wall_top);  
                              
                            int tex_x = (int)(u * current_wall_texture->width) % current_wall_texture->width;  
                            int tex_y = (int)(v * current_wall_texture->height) % current_wall_texture->height;  
                              
                            if (tex_x < 0) tex_x = 0;  
                            if (tex_x >= current_wall_texture->width) tex_x = current_wall_texture->width - 1;  
                            if (tex_y < 0) tex_y = 0;  
                            if (tex_y >= current_wall_texture->height) tex_y = current_wall_texture->height - 1;  
                              
                            color = gr_get_pixel(current_wall_texture, tex_x, tex_y);  
                              
                            // Extraer componentes RGB  
                            uint8_t r, g, b, a;  
                            SDL_GetRGBA(color, gPixelFormat, &r, &g, &b, &a);  
                              
                            // Aplicar fog  
                            float fog = 1.0f - (depth / max_render_distance);  
                            if (fog < 0.3f) fog = 0.3f;  
                            if (fog > 1.0f) fog = 1.0f;  
                              
                            r = (uint8_t)(r * fog);  
                            g = (uint8_t)(g * fog);  
                            b = (uint8_t)(b * fog);  
                              
                            color = SDL_MapRGBA(gPixelFormat, r, g, b, 255);  
                        } else {  
                            // Color sólido si no hay textura  
                            color = SDL_MapRGBA(gPixelFormat, 150, 120, 100, 255);  
                        }  
                          
                        gr_put_pixel(cpu_render_buffer, screen_x, screen_y, color);  
                        depth_buffer[buffer_index] = depth;  
                        walls_rendered++;  
                    }  
                }  
            }  
        }  
    }
    // ============================================================  
    // PARTE 3: RENDERIZADO DE SUELOS Y TECHOS CON TEXTURAS EMPAQUETADAS  
    // ============================================================  
      
    int floor_pixels_rendered = 0;  
    int ceiling_pixels_rendered = 0;  
      
    // Renderizar suelos y techos para píxeles no cubiertos por paredes  
    for (int screen_y = 0; screen_y < render_height; screen_y++) {  
        for (int screen_x = 0; screen_x < render_width; screen_x++) {  
            buffer_index = (size_t)screen_y * (size_t)render_width + (size_t)screen_x;  
              
            if (buffer_index >= buffer_size) continue;  
              
            // Solo renderizar si no hay pared en este píxel  
            if (depth_buffer[buffer_index] >= max_render_distance * 0.99f) {  
                // Calcular ángulo del rayo para esta columna  
                float column_angle = camera.angle - fov * 0.5f + (screen_x / (float)render_width) * fov;  
                float ray_dir_x = cosf(column_angle);  
                float ray_dir_y = sinf(column_angle);  
                  
                // RENDERIZADO DE SUELO  
                if (screen_y > render_height / 2) {  
                    if (floor_texture && floor_texture->width > 0 && floor_texture->height > 0) {  
                        // Calcular pitch del rayo para esta fila  
                        float screen_y_normalized = (screen_y / (float)render_height) - 0.5f;  
                        float vertical_fov = fov * 0.75f;  
                        float tan_half_vfov = tanf(vertical_fov * 0.5f);  
                        float ray_pitch_offset = atanf(screen_y_normalized * 2.0f * tan_half_vfov);  
                        float ray_pitch = camera.pitch + ray_pitch_offset;  
                        float sin_pitch = sinf(ray_pitch);  
                          
                        if (fabsf(sin_pitch) > 0.001f) {  
                            // Iterar sobre sectores para encontrar intersección con suelo  
                            for (int s = 0; s < hm->num_sectors; s++) {  
                                SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[s];  
                                  
                                float floor_height = sector->floor_height;  
                                float height_diff = camera.z - floor_height;  
                                float distance_to_floor = height_diff / sin_pitch;  
                                  
                                if (distance_to_floor > 0.0f && distance_to_floor < max_render_distance) {  
                                    // Calcular punto de intersección en el mundo  
                                    float world_x = camera.x + ray_dir_x * distance_to_floor;  
                                    float world_y = camera.y + ray_dir_y * distance_to_floor;  
                                      
                                    // Verificar si el punto está dentro del sector (point-in-polygon)  
                                    int inside = 0;  
                                    for (uint32_t v = 0; v < sector->num_vertices; v++) {  
                                        uint32_t next_v = (v + 1) % sector->num_vertices;  
                                        VERTEX *v1 = &hm->vertices[sector->vertex_indices[v]];  
                                        VERTEX *v2 = &hm->vertices[sector->vertex_indices[next_v]];  
                                        float v1_x = v1->x;  
                                        float v1_y = v1->y;  
                                        float v2_x = v2->x;  
                                        float v2_y = v2->y;  
                                          
                                        if (((v1_y > world_y) != (v2_y > world_y)) &&  
                                            (world_x < (v2_x - v1_x) * (world_y - v1_y) / (v2_y - v1_y) + v1_x)) {  
                                            inside = !inside;  
                                        }  
                                    }  
                                      
                                    if (inside) {  
                                        // Samplear textura de suelo  
                                        float tex_u = fmodf(world_x / 100.0f, 1.0f);  
                                        float tex_v = fmodf(world_y / 100.0f, 1.0f);  
                                        if (tex_u < 0.0f) tex_u += 1.0f;  
                                        if (tex_v < 0.0f) tex_v += 1.0f;  
                                          
                                        int tex_x = (int)(tex_u * (floor_texture->width - 1));  
                                        int tex_y = (int)(tex_v * (floor_texture->height - 1));  
                                          
                                        if (tex_x >= 0 && tex_x < floor_texture->width &&  
                                            tex_y >= 0 && tex_y < floor_texture->height) {  
                                              
                                            uint32_t pixel = gr_get_pixel(floor_texture, tex_x, tex_y);  
                                              
                                            uint8_t r, g, b, a;  
                                            SDL_GetRGBA(pixel, gPixelFormat, &r, &g, &b, &a);  
                                              
                                            // Aplicar iluminación  
                                            r = (uint8_t)(r * (light_intensity / 255.0f));  
                                            g = (uint8_t)(g * (light_intensity / 255.0f));  
                                            b = (uint8_t)(b * (light_intensity / 255.0f));  
                                              
                                            // Aplicar fog  
                                            float fog = 1.0f - (distance_to_floor / max_render_distance);  
                                            fog = fmaxf(0.3f, fminf(1.0f, fog));  
                                              
                                            r = (uint8_t)(r * fog + sky_color_r * (1.0f - fog));  
                                            g = (uint8_t)(g * fog + sky_color_g * (1.0f - fog));  
                                            b = (uint8_t)(b * fog + sky_color_b * (1.0f - fog));  
                                              
                                            uint32_t final_color = SDL_MapRGBA(gPixelFormat, r, g, b, 255);  
                                            gr_put_pixel(cpu_render_buffer, screen_x, screen_y, final_color);  
                                              
                                            depth_buffer[buffer_index] = distance_to_floor;  
                                            floor_pixels_rendered++;  
                                        }  
                                    }  
                                }  
                            }  
                        }  
                    }  
                }  
                  
                // RENDERIZADO DE TECHO  
                else if (screen_y < render_height / 2) {  
                    if (ceiling_texture && ceiling_texture->width > 0 && ceiling_texture->height > 0) {  
                        // Calcular pitch del rayo para esta fila  
                        float screen_y_normalized = (screen_y / (float)render_height) - 0.5f;  
                        float vertical_fov = fov * 0.75f;  
                        float tan_half_vfov = tanf(vertical_fov * 0.5f);  
                        float ray_pitch_offset = atanf(screen_y_normalized * 2.0f * tan_half_vfov);  
                        float ray_pitch = camera.pitch + ray_pitch_offset;  
                        float sin_pitch = sinf(ray_pitch);  
                          
                        if (fabsf(sin_pitch) > 0.001f) {  
                            // Iterar sobre sectores para encontrar intersección con techo  
                            for (int s = 0; s < hm->num_sectors; s++) {  
                                SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[s];  
                                  
                                float ceiling_height = sector->ceiling_height;  
                                float height_diff = camera.z - ceiling_height;  
                                float distance_to_ceiling = height_diff / sin_pitch;  
                                  
                                if (distance_to_ceiling > 0.0f && distance_to_ceiling < max_render_distance) {  
                                    // Calcular punto de intersección en el mundo  
                                    float world_x = camera.x + ray_dir_x * distance_to_ceiling;  
                                    float world_y = camera.y + ray_dir_y * distance_to_ceiling;  
                                      
                                    // Verificar si el punto está dentro del sector (point-in-polygon)  
                                    int inside = 0;  
                                    for (uint32_t v = 0; v < sector->num_vertices; v++) {  
                                        uint32_t next_v = (v + 1) % sector->num_vertices;  
                                        VERTEX *v1 = &hm->vertices[sector->vertex_indices[v]];  
                                        VERTEX *v2 = &hm->vertices[sector->vertex_indices[next_v]];  
                                        float v1_x = v1->x;  
                                        float v1_y = v1->y;  
                                        float v2_x = v2->x;  
                                        float v2_y = v2->y;  
                                          
                                        if (((v1_y > world_y) != (v2_y > world_y)) &&  
                                            (world_x < (v2_x - v1_x) * (world_y - v1_y) / (v2_y - v1_y) + v1_x)) {  
                                            inside = !inside;  
                                        }  
                                    }  
                                      
                                    if (inside) {  
                                        // Samplear textura de techo  
                                        float tex_u = fmodf(world_x / 100.0f, 1.0f);  
                                        float tex_v = fmodf(world_y / 100.0f, 1.0f);  
                                        if (tex_u < 0.0f) tex_u += 1.0f;  
                                        if (tex_v < 0.0f) tex_v += 1.0f;  
                                          
                                        int tex_x = (int)(tex_u * (ceiling_texture->width - 1));  
                                        int tex_y = (int)(tex_v * (ceiling_texture->height - 1));  
                                          
                                        if (tex_x >= 0 && tex_x < ceiling_texture->width &&  
                                            tex_y >= 0 && tex_y < ceiling_texture->height) {  
                                              
                                            uint32_t pixel = gr_get_pixel(ceiling_texture, tex_x, tex_y);  
                                              
                                            uint8_t r, g, b, a;  
                                            SDL_GetRGBA(pixel, gPixelFormat, &r, &g, &b, &a);  
                                              
                                            // Aplicar iluminación  
                                            r = (uint8_t)(r * (light_intensity / 255.0f));  
                                            g = (uint8_t)(g * (light_intensity / 255.0f));  
                                            b = (uint8_t)(b * (light_intensity / 255.0f));  
                                              
                                            // Aplicar fog  
                                            float fog = 1.0f - (distance_to_ceiling / max_render_distance);  
                                            fog = fmaxf(0.3f, fminf(1.0f, fog));  
                                              
                                            r = (uint8_t)(r * fog + sky_color_r * (1.0f - fog));  
                                            g = (uint8_t)(g * fog + sky_color_g * (1.0f - fog));  
                                            b = (uint8_t)(b * fog + sky_color_b * (1.0f - fog));  
                                              
                                            uint32_t final_color = SDL_MapRGBA(gPixelFormat, r, g, b, 255);  
                                            gr_put_pixel(cpu_render_buffer, screen_x, screen_y, final_color);  
                                              
                                            depth_buffer[buffer_index] = distance_to_ceiling;  
                                            ceiling_pixels_rendered++;  
                                        }  
                                    }  
                                }  
                            }  
                        }  
                    }  
                }  
            }  
        }  
    }  
      
    // Debug: Mostrar estadísticas de renderizado  
    fprintf(stderr, "Renderizado completado: %d paredes, %d píxeles de suelo, %d píxeles de techo\n",  
            walls_rendered, floor_pixels_rendered, ceiling_pixels_rendered);
    // ============================================================  
    // PARTE 4: RENDERIZADO DE BILLBOARDS Y RETORNO  
    // ============================================================  
      
    // Renderizar billboards sobre el buffer de sectores  
    render_billboards_to_buffer(cpu_render_buffer, depth_buffer, render_width, render_height);  
      
    // Retornar el graph_id del buffer renderizado  
    return cpu_render_buffer->code;  
}  
//---------------------------------------------------------------------------------------//
//                          FIN MAPAS POR SECTORES                                       //
//---------------------------------------------------------------------------------------//

    void render_billboards_to_buffer(GRAPH *target_buffer) {  
    // Reutilizar la lógica de billboards del renderizado CPU  
    BILLBOARD_RENDER_DATA visible_billboards[MAX_STATIC_BILLBOARDS + MAX_DYNAMIC_BILLBOARDS];  
    int visible_count = 0;  
      
    float terrain_fov = 0.7f;  
      
    collect_visible_billboards_from_array(static_billboards, static_billboard_count,  
                                         visible_billboards, &visible_count, terrain_fov);  
      
    collect_visible_billboards_from_array(dynamic_billboards, MAX_DYNAMIC_BILLBOARDS,  
                                         visible_billboards, &visible_count, terrain_fov);  
      
    qsort(visible_billboards, visible_count, sizeof(BILLBOARD_RENDER_DATA), compare_billboards_by_distance);  
      
    for (int i = 0; i < visible_count; i++) {  
        BILLBOARD_RENDER_DATA *render_data = &visible_billboards[i];  
        BILLBOARD_PROJECTION proj = render_data->projection;  
          
        gr_blit(target_buffer, NULL,  
               proj.screen_x - proj.scaled_width/2,  
               proj.screen_y - proj.scaled_height/2,  
               0, 0, proj.scaled_width, proj.scaled_height,  
               render_data->graph->width/2, render_data->graph->height/2,  
               render_data->graph, NULL, 255, 255, 255, proj.alpha, 0, NULL);  
    }  
}

// Función helper para movimiento en sectores (sin colisión de terreno)  
static void move_camera_in_sector(float angle_offset, float speed) {  
    float movement_angle = camera.angle + angle_offset;  
    camera.x += cosf(movement_angle) * speed;  
    camera.y += sinf(movement_angle) * speed;  
}  
  
// Funciones exportadas para movimiento en sectores  
int64_t libmod_heightmap_sector_move_forward(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_in_sector(0.0f, speed);  
    return 1;  
}  
  
int64_t libmod_heightmap_sector_move_backward(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_in_sector(M_PI, speed);  
    return 1;  
}  
  
int64_t libmod_heightmap_sector_strafe_left(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_in_sector(-M_PI_2, speed);  
    return 1;  
}  
  
int64_t libmod_heightmap_sector_strafe_right(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_in_sector(M_PI_2, speed);  
    return 1;  
}

/* Actualizar sectores dinámicos (ascensores, plataformas móviles) */  
void update_dynamic_sectors(HEIGHTMAP *hm, float delta_time) {  
    if (!hm || hm->type != MAP_TYPE_SECTOR) return;  
      
    for (int i = 0; i < hm->num_sectors; i++) {  
        SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[i];  
          
        // Solo procesar sectores dinámicos en movimiento  
        if (!sector->is_dynamic || sector->move_state == 0) continue;  
          
        float move_amount = sector->move_speed * delta_time;  
          
        if (sector->move_state == 1) { // Subiendo  
            sector->floor_height += move_amount;  
            sector->ceiling_height += move_amount;  
              
            // Verificar si alcanzó el objetivo  
            if (sector->floor_height >= sector->target_floor_height) {  
                sector->floor_height = sector->target_floor_height;  
                sector->ceiling_height = sector->target_ceiling_height;  
                sector->move_state = 0; // Detener  
                  
                fprintf(stderr, "Sector %u alcanzó altura objetivo: %.2f\n",   
                        sector->id, sector->floor_height);  
            }  
        } else if (sector->move_state == 2) { // Bajando  
            sector->floor_height -= move_amount;  
            sector->ceiling_height -= move_amount;  
              
            // Verificar si alcanzó el objetivo  
            if (sector->floor_height <= sector->target_floor_height) {  
                sector->floor_height = sector->target_floor_height;  
                sector->ceiling_height = sector->target_ceiling_height;  
                sector->move_state = 0; // Detener  
                  
                fprintf(stderr, "Sector %u alcanzó altura objetivo: %.2f\n",   
                        sector->id, sector->floor_height);  
            }  
        }  
    }  
}

/* Calcular altura del suelo en un punto, considerando pendientes */  
float floor_height_at_point(SECTOR_V2 *sector, float world_x, float world_y) {  
    if (!sector) return 0.0f;  
      
    // Altura base del sector  
    float base_height = sector->floor_height;  
      
    // Si no hay pendiente, retornar altura constante  
    if (sector->floor_slope_x == 0.0f && sector->floor_slope_y == 0.0f) {  
        return base_height;  
    }  
      
    // Calcular punto de referencia (primer vértice del sector)  
    if (sector->num_vertices == 0 || !sector->vertex_indices) {  
        return base_height;  
    }  
      
    // Obtener primer vértice como origen  
    VERTEX *first_vertex = &((HEIGHTMAP*)sector)->vertices[sector->vertex_indices[0]];  
      
    // Calcular desplazamiento desde el origen  
    float dx = world_x - first_vertex->x;  
    float dy = world_y - first_vertex->y;  
      
    // Aplicar pendientes  
    float height = base_height + (dx * sector->floor_slope_x) + (dy * sector->floor_slope_y);  
      
    return height;  
}

/* Verificar colisión con paredes usando blockmap */  
int check_wall_collision(float x, float y, HEIGHTMAP *hm) {  
    if (!hm || hm->type != MAP_TYPE_SECTOR) return 0;  
      
    // Si no hay blockmap, verificar todas las paredes (lento)  
    if (!hm->blockmap_grid || hm->blockmap_width == 0 || hm->blockmap_height == 0) {  
        for (int i = 0; i < hm->num_walls; i++) {  
            WALL_V2 *wall = &((WALL_V2*)hm->walls)[i];  
              
            // Solo verificar paredes que bloquean movimiento  
            if (!(wall->flags & 0x01)) continue;  
              
            // Obtener vértices de la pared  
            VERTEX *v1 = &hm->vertices[wall->vertex1_index];  
            VERTEX *v2 = &hm->vertices[wall->vertex2_index];  
              
            // Calcular distancia punto-línea  
            float dx = v2->x - v1->x;  
            float dy = v2->y - v1->y;  
            float length_sq = dx*dx + dy*dy;  
              
            if (length_sq < 0.001f) continue;  
              
            float t = ((x - v1->x) * dx + (y - v1->y) * dy) / length_sq;  
            t = fmaxf(0.0f, fminf(1.0f, t));  
              
            float closest_x = v1->x + t * dx;  
            float closest_y = v1->y + t * dy;  
            float dist_sq = (x - closest_x)*(x - closest_x) + (y - closest_y)*(y - closest_y);  
              
            if (dist_sq < 25.0f) {  
                return 1;  
            }  
        }  
        return 0;  
    }  
      
    // Usar blockmap para optimización  
    int cell_x = (int)(x / hm->blockmap_cell_size);  
    int cell_y = (int)(y / hm->blockmap_cell_size);  
      
    // Verificar límites del blockmap  
    if (cell_x < 0 || cell_x >= hm->blockmap_width ||  
        cell_y < 0 || cell_y >= hm->blockmap_height) {  
        return 1; // Fuera del mapa = colisión  
    }  
      
    // Obtener la celda del blockmap  
    BLOCKMAP_CELL *cell = &hm->blockmap_grid[cell_y * hm->blockmap_width + cell_x];  
      
    // Si no hay paredes en esta celda, no hay colisión  
    if (cell->num_walls == 0) {  
        return 0;  
    }  
      
    // Verificar solo las paredes indexadas en esta celda  
    for (uint32_t w = 0; w < cell->num_walls; w++) {  
        uint32_t wall_idx = cell->wall_indices[w];  
          
        if (wall_idx >= hm->num_walls) continue;  
          
        WALL_V2 *wall = &((WALL_V2*)hm->walls)[wall_idx];  
          
        // Solo verificar paredes que bloquean movimiento  
        if (!(wall->flags & 0x01)) continue;  
          
        // Obtener vértices de la pared  
        VERTEX *v1 = &hm->vertices[wall->vertex1_index];  
        VERTEX *v2 = &hm->vertices[wall->vertex2_index];  
          
        // Calcular distancia punto-línea  
        float dx = v2->x - v1->x;  
        float dy = v2->y - v1->y;  
        float length_sq = dx*dx + dy*dy;  
          
        if (length_sq < 0.001f) continue;  
          
        float t = ((x - v1->x) * dx + (y - v1->y) * dy) / length_sq;  
        t = fmaxf(0.0f, fminf(1.0f, t));  
          
        float closest_x = v1->x + t * dx;  
        float closest_y = v1->y + t * dy;  
        float dist_sq = (x - closest_x)*(x - closest_x) + (y - closest_y)*(y - closest_y);  
          
        if (dist_sq < 25.0f) {  
            return 1; // Colisión detectada  
        }  
    }  
      
    return 0;  
}

/* Encontrar el sector que contiene un punto (x, y) */  
int get_sector_at_position(HEIGHTMAP *hm, float x, float y) {  
    if (!hm || hm->type != MAP_TYPE_SECTOR) return -1;  
      
    for (int i = 0; i < hm->num_sectors; i++) {  
        SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[i];  
          
        // Verificar si el punto está dentro del polígono del sector  
        // Usando algoritmo de ray casting (point-in-polygon)  
        int inside = 0;  
          
        for (uint32_t v = 0; v < sector->num_vertices; v++) {  
            uint32_t next_v = (v + 1) % sector->num_vertices;  
              
            VERTEX *v1 = &hm->vertices[sector->vertex_indices[v]];  
            VERTEX *v2 = &hm->vertices[sector->vertex_indices[next_v]];  
              
            if (((v1->y > y) != (v2->y > y)) &&  
                (x < (v2->x - v1->x) * (y - v1->y) / (v2->y - v1->y) + v1->x)) {  
                inside = !inside;  
            }  
        }  
          
        if (inside) {  
            return i; // Retornar índice del sector  
        }  
    }  
      
    return -1; // No se encontró sector  
}

/* Activar ascensor (cambiar estado de movimiento) */  
int64_t libmod_heightmap_activate_elevator(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    int64_t sector_id = params[1];  
      
    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
    if (!hm || hm->type != MAP_TYPE_SECTOR) return 0;  
      
    // Buscar el sector  
    for (int i = 0; i < hm->num_sectors; i++) {  
        SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[i];  
          
        if (sector->id == sector_id && sector->is_dynamic) {  
            // Alternar estado: si está arriba, bajar; si está abajo, subir  
            if (sector->floor_height >= sector->target_floor_height) {  
                sector->move_state = 2; // Bajar  
            } else {  
                sector->move_state = 1; // Subir  
            }  
              
            fprintf(stderr, "Ascensor sector %u activado, estado: %u\n",   
                    sector_id, sector->move_state);  
            return 1;  
        }  
    }  
      
    return 0;  
}

/* Establecer altura objetivo de un sector dinámico */  
int64_t libmod_heightmap_set_sector_target_height(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    int64_t sector_id = params[1];  
    float target_floor = *(float*)&params[2];  
    float target_ceiling = *(float*)&params[3];  
      
    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
    if (!hm || hm->type != MAP_TYPE_SECTOR) return 0;  
      
    for (int i = 0; i < hm->num_sectors; i++) {  
        SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[i];  
          
        if (sector->id == sector_id && sector->is_dynamic) {  
            sector->target_floor_height = target_floor;  
            sector->target_ceiling_height = target_ceiling;  
            return 1;  
        }  
    }  
      
    return 0;  
}

/* Verificar colisión con paredes desde código BennuGD2 */  
int64_t libmod_heightmap_check_wall_collision(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float x = *(float*)&params[1];  
    float y = *(float*)&params[2];  
      
    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
    if (!hm) return 0;  
      
    return check_wall_collision(x, y, hm);  
}

/* Obtener ID del sector en una posición */  
int64_t libmod_heightmap_get_sector_at_position(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float x = *(float*)&params[1];  
    float y = *(float*)&params[2];  
      
    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
    if (!hm || hm->type != MAP_TYPE_SECTOR) return -1;  
      
    int sector_index = get_sector_at_position(hm, x, y);  
      
    if (sector_index >= 0 && sector_index < hm->num_sectors) {  
        SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[sector_index];  
        return sector->id;  
    }  
      
    return -1; // No se encontró sector  
}

/* Activar acción de una pared (abrir puerta, activar trigger) */  
int64_t libmod_heightmap_activate_wall_action(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    int64_t wall_id = params[1];  
      
    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
    if (!hm || hm->type != MAP_TYPE_SECTOR) return 0;  
      
    // Buscar la pared por ID  
    for (int i = 0; i < hm->num_walls; i++) {  
        WALL_V2 *wall = &((WALL_V2*)hm->walls)[i];  
          
        if (i == wall_id) { // Usar índice como ID por simplicidad  
            // Ejecutar acción según el tipo  
            switch (wall->action_type) {  
                case ACTION_NONE:  
                    return 0;  
                      
                case ACTION_OPEN_DOOR:  
                    // Lógica para abrir puerta (cambiar textura, mover sectores, etc.)  
                    fprintf(stderr, "Abriendo puerta en pared %ld\n", wall_id);  
                    // Aquí podrías modificar los sectores conectados  
                    return 1;  
                      
                case ACTION_ACTIVATE_ELEVATOR:  
                    // Activar ascensor en uno de los sectores conectados  
                    fprintf(stderr, "Activando ascensor desde pared %ld\n", wall_id);  
                    for (int s = 0; s < hm->num_sectors; s++) {  
                        SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[s];  
                        if ((sector->id == wall->sector1_id || sector->id == wall->sector2_id)   
                            && sector->is_dynamic) {  
                            // Alternar estado del ascensor  
                            if (sector->floor_height >= sector->target_floor_height) {  
                                sector->move_state = 2; // Bajar  
                            } else {  
                                sector->move_state = 1; // Subir  
                            }  
                            return 1;  
                        }  
                    }  
                    return 0;  
                      
                case ACTION_TRIGGER_SCRIPT:  
                    // Ejecutar script personalizado  
                    fprintf(stderr, "Trigger de script en pared %ld\n", wall_id);  
                    return 1;  
                      
                default:  
                    return 0;  
            }  
        }  
    }  
      
    return 0; // Pared no encontrada  
}

/* Actualizar todos los sectores dinámicos (llamar cada frame) */  
int64_t libmod_heightmap_update_dynamic_sectors(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float delta_time = *(float*)&params[1]; // Tiempo transcurrido en segundos  
      
    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
    if (!hm || hm->type != MAP_TYPE_SECTOR) return 0;  
      
    update_dynamic_sectors(hm, delta_time);  
    return 1;  
}

/* Obtener altura del suelo en un punto específico (considerando rampas) */  
int64_t libmod_heightmap_get_floor_height_at_point(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    int64_t sector_id = params[1];  
    float x = *(float*)&params[2];  
    float y = *(float*)&params[3];  
      
    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
    if (!hm || hm->type != MAP_TYPE_SECTOR) return 0;  
      
    // Buscar el sector  
    for (int i = 0; i < hm->num_sectors; i++) {  
        SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[i];  
          
        if (sector->id == sector_id) {  
            float height = floor_height_at_point(sector, x, y);  
              
            // Convertir float a int64_t para retorno  
            // (BennuGD2 puede necesitar conversión especial)  
            int32_t height_int = (int32_t)(height * 100.0f); // Escalar por 100 para precisión  
            return height_int;  
        }  
    }  
      
    return 0;  
}

/* Renderizar nodo BSP recursivamente (back-to-front ordering) */  
static void render_bsp_node(HEIGHTMAP *hm, int node_index,   
                           float cam_x, float cam_y,  
                           int render_width, int render_height,  
                           float projection_scale, float fov,  
                           float cos_cam, float sin_cam,  
                           float *depth_buffer, GRAPH *render_buffer,  
                           int *walls_rendered) {  
       fprintf(stderr, "render_bsp_node: node=%d subsectors=%d segs=%d\n",   
        node_index, hm->num_subsectors, hm->num_segs);
        fflush(stderr);
    // Caso base: si es un subsector (hoja del árbol)  
    if (node_index < 0) {  
        // Los índices negativos indican subsectores  
        // En DOOM: subsector_index = -(node_index + 1)  
        int subsector_index = -(node_index + 1);  
          
        if (subsector_index >= 0 && subsector_index < hm->num_subsectors) {  
            SUBSECTOR *subsector = &hm->subsectors[subsector_index];  
              
            // Renderizar todos los segmentos (SEGS) de este subsector  
            for (int i = 0; i < subsector->num_segs; i++) {  
                int seg_index = subsector->first_seg + i;  
                if (seg_index >= 0 && seg_index < hm->num_segs) {  
                    SEG *seg = &hm->segs[seg_index];  
                      
                    // Obtener vértices del segmento  
                    VERTEX *v1 = &hm->vertices[seg->vertex1_index];  
                    VERTEX *v2 = &hm->vertices[seg->vertex2_index];  
                      
                    // Obtener sector asociado  
                    SECTOR_V2 *sector = &((SECTOR_V2*)hm->sectors)[seg->sector_id];  
                      
                    // Renderizar este segmento de pared  
                    render_wall_segment(hm, v1->x, v1->y, v2->x, v2->y,  
                                      sector, seg->sidedef_id,  
                                      render_width, render_height,  
                                      projection_scale, fov,  
                                      cos_cam, sin_cam,  
                                      depth_buffer, render_buffer,  
                                      walls_rendered);  
                }  
            }  
        }  
       
        return;  
    }  
      
    // Caso recursivo: nodo interno del BSP  
    if (node_index >= hm->num_bsp_nodes) {  
        return; // Índice inválido  
    }  
      
    BSP_NODE *node = &hm->bsp_nodes[node_index];  
      
    // Determinar en qué lado de la línea de partición está la cámara  
    // Ecuación de la línea: (x - px) * dy - (y - py) * dx  
    float dx = cam_x - node->partition_x;  
    float dy = cam_y - node->partition_y;  
    float side = dx * node->partition_dy - dy * node->partition_dx;  
      
    // Renderizar en orden back-to-front  
    if (side >= 0.0f) {  
        // Cámara en lado frontal: renderizar back primero (más lejos)  
        render_bsp_node(hm, node->back_child, cam_x, cam_y,  
                       render_width, render_height,  
                       projection_scale, fov, cos_cam, sin_cam,  
                       depth_buffer, render_buffer, walls_rendered);  
          
        // Luego renderizar front (más cerca)  
        render_bsp_node(hm, node->front_child, cam_x, cam_y,  
                       render_width, render_height,  
                       projection_scale, fov, cos_cam, sin_cam,  
                       depth_buffer, render_buffer, walls_rendered);  
    } else {  
        // Cámara en lado trasero: renderizar front primero  
        render_bsp_node(hm, node->front_child, cam_x, cam_y,  
                       render_width, render_height,  
                       projection_scale, fov, cos_cam, sin_cam,  
                       depth_buffer, render_buffer, walls_rendered);  
          
        // Luego renderizar back  
        render_bsp_node(hm, node->back_child, cam_x, cam_y,  
                       render_width, render_height,  
                       projection_scale, fov, cos_cam, sin_cam,  
                       depth_buffer, render_buffer, walls_rendered);  
    }  
}

/* Renderizar un segmento de pared individual */  
static void render_wall_segment(HEIGHTMAP *hm,   
                                float x1, float y1, float x2, float y2,  
                                SECTOR_V2 *sector, int sidedef_id,  
                                int render_width, int render_height,  
                                float projection_scale, float fov,  
                                float cos_cam, float sin_cam,  
                                float *depth_buffer, GRAPH *render_buffer,  
                                int *walls_rendered) {  
                                    fprintf(stderr, "render_wall_segment: x1=%.1f y1=%.1f x2=%.1f y2=%.1f\n",   
        x1, y1, x2, y2);
      
    // Verificar que la cámara esté dentro del rango válido del sector  
    float current_floor = sector->floor_height;  
    float current_ceiling = sector->ceiling_height;  
      
    if (camera.z < current_floor || camera.z > current_ceiling) {  
        return;  
    }  
      
    // Transformar vértices al espacio de cámara  
    float dx1 = x1 - camera.x;  
    float dy1 = y1 - camera.y;  
    float dx2 = x2 - camera.x;  
    float dy2 = y2 - camera.y;  
      
    // Rotar al espacio de vista de la cámara  
    float cam_x1 = dx1 * cos_cam - dy1 * sin_cam;  
    float cam_z1 = dx1 * sin_cam + dy1 * cos_cam;  
    float cam_x2 = dx2 * cos_cam - dy2 * sin_cam;  
    float cam_z2 = dx2 * sin_cam + dy2 * cos_cam;  
      
    // Clipping contra near plane  
    float near_plane = 0.1f;  
    if (cam_z1 < near_plane && cam_z2 < near_plane) {  
        return; // Segmento completamente detrás de la cámara  
    }  
      
    // Proyectar a coordenadas de pantalla  
    int screen_x1, screen_x2;  
      
    if (cam_z1 > near_plane) {  
        screen_x1 = (int)(render_width / 2.0f + (cam_x1 / cam_z1) * projection_scale);  
    } else {  
        screen_x1 = (cam_x1 < 0) ? -1 : render_width;  
    }  
      
    if (cam_z2 > near_plane) {  
        screen_x2 = (int)(render_width / 2.0f + (cam_x2 / cam_z2) * projection_scale);  
    } else {  
        screen_x2 = (cam_x2 < 0) ? -1 : render_width;  
    }  
      
    // Verificar que el segmento esté dentro de la pantalla  
    if (screen_x1 < 0 && screen_x2 < 0) return;  
    if (screen_x1 >= render_width && screen_x2 >= render_width) return;  
      
    // Clamp a límites de pantalla  
    if (screen_x1 < 0) screen_x1 = 0;  
    if (screen_x2 >= render_width) screen_x2 = render_width - 1;  
    if (screen_x1 > screen_x2) {  
        int temp = screen_x1;  
        screen_x1 = screen_x2;  
        screen_x2 = temp;  
    }  
      
    // Obtener textura del sidedef  
    GRAPH *wall_texture = NULL;  
    if (sidedef_id >= 0 && sidedef_id < hm->num_sidedefs) {  
        SIDEDEF *sidedef = &hm->sidedefs[sidedef_id];  
        if (sidedef->texture_id < hm->num_textures) {  
            wall_texture = bitmap_get(0, hm->textures[sidedef->texture_id].graph_id);  
        }  
    }  
      
    // Renderizar columnas del segmento  
    for (int screen_x = screen_x1; screen_x <= screen_x2; screen_x++) {  
        if (screen_x < 0 || screen_x >= render_width) continue;  
          
        // Interpolación lineal de profundidad  
        float t = (screen_x2 != screen_x1) ?   
                 (float)(screen_x - screen_x1) / (float)(screen_x2 - screen_x1) : 0.0f;  
        float depth = cam_z1 + t * (cam_z2 - cam_z1);  
          
        if (depth < near_plane) continue;  
          
        // Calcular altura de pared en pantalla  
        float wall_height = current_ceiling - current_floor;  
        float wall_top_screen = render_height / 2.0f -   
                               ((current_ceiling - camera.z) / depth) * projection_scale;  
        float wall_bottom_screen = render_height / 2.0f -   
                                  ((current_floor - camera.z) / depth) * projection_scale;  
          
        int wall_top = (int)wall_top_screen;  
        int wall_bottom = (int)wall_bottom_screen;  
          
        // Clamp a límites de pantalla  
        if (wall_top < 0) wall_top = 0;  
        if (wall_bottom >= render_height) wall_bottom = render_height - 1;  
          
        if (wall_top >= wall_bottom) continue;  
          
        // Renderizar píxeles de la pared  
        for (int screen_y = wall_top; screen_y < wall_bottom; screen_y++) {  
            size_t buffer_index = (size_t)screen_y * (size_t)render_width + (size_t)screen_x;  
              
            // Verificar depth buffer  
            if (depth >= depth_buffer[buffer_index]) {  
                continue;  
            }  
              
            uint32_t color;  
              
            if (wall_texture && wall_texture->width > 0 && wall_texture->height > 0) {  
                // Calcular coordenadas de textura  
                float wall_length = sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));  
                float u = (t * wall_length) / 64.0f;  
                u = u - floorf(u);  
                  
                float v = (float)(screen_y - wall_top) / (float)(wall_bottom - wall_top);  
                  
                int tex_x = (int)(u * wall_texture->width) % wall_texture->width;  
                int tex_y = (int)(v * wall_texture->height) % wall_texture->height;  
                  
                if (tex_x < 0) tex_x = 0;  
                if (tex_x >= wall_texture->width) tex_x = wall_texture->width - 1;  
                if (tex_y < 0) tex_y = 0;  
                if (tex_y >= wall_texture->height) tex_y = wall_texture->height - 1;  
                  
                color = gr_get_pixel(wall_texture, tex_x, tex_y);  
                  
                // Extraer componentes RGB  
                uint8_t r, g, b, a;  
                SDL_GetRGBA(color, gPixelFormat, &r, &g, &b, &a);  
                  
                // Aplicar fog  
                float fog = 1.0f - (depth / max_render_distance);  
                if (fog < 0.3f) fog = 0.3f;  
                if (fog > 1.0f) fog = 1.0f;  
                  
                // Aplicar iluminación por sector  
                if (sector->light_level > 0) {  
                    float light_factor = sector->light_level / 255.0f;  
                    r = (uint8_t)(r * fog * light_factor);  
                    g = (uint8_t)(g * fog * light_factor);  
                    b = (uint8_t)(b * fog * light_factor);  
                } else {  
                    r = (uint8_t)(r * fog);  
                    g = (uint8_t)(g * fog);  
                    b = (uint8_t)(b * fog);  
                }  
                  
                color = SDL_MapRGBA(gPixelFormat, r, g, b, 255);  
            } else {  
                // Color sólido si no hay textura  
                color = SDL_MapRGBA(gPixelFormat, 150, 120, 100, 255);  
            }  
              
            gr_put_pixel(render_buffer, screen_x, screen_y, color);  
            depth_buffer[buffer_index] = depth;  
            (*walls_rendered)++;  
        }  
    }  
    
}
#include "libmod_heightmap_exports.h"