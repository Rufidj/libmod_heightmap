# libmod_heightmap - Módulo de terreno 3D para BennuGD2

Este módulo permite renderizar y manipular terrenos en 3D utilizando mapas de altura (`heightmaps`). Proporciona funciones para cargar mapas y texturas, controlar una cámara 3D, simular iluminación y colisiones, y proyectar objetos sobre el terreno.

## 📦 Funciones disponibles

### 📁 Carga y gestión de mapas

- `heightmap_load(filename)`
  - Carga un mapa de altura desde un archivo de imagen (formato RGB).
  - **Ejemplo:** `hm_id = heightmap_load("terrain.png");`

- `heightmap_create(width, height)`
  - Crea un mapa de altura vacío en memoria.
  - **Ejemplo:** `hm_id = heightmap_create(256, 256);`

- `heightmap_create_procedural(width, height)`
  - Genera un heightmap procedural básico.
  - **Ejemplo:** `hm_id = heightmap_create_procedural(512, 512);`

- `heightmap_unload(id)`
  - Libera recursos del mapa de altura cargado.

- `heightmap_load_texture(hm_id, filename)`
  - Asocia una textura a un mapa de altura.
  - **Ejemplo:** `heightmap_load_texture(hm_id, "terrain_texture.png");`

---

### 🎥 Cámara

- `heightmap_set_camera(x, y, z, angle, pitch, fov)`
  - Configura la posición y orientación de la cámara.
  - Ángulos en milésimas de radian.
  - **Ejemplo:** `heightmap_set_camera(100, 100, 50, 0, 0, 60);`

- `heightmap_get_camera_position(&x, &y, &z, &angle, &pitch)`
  - Obtiene la posición actual de la cámara en variables del PRG.

- `heightmap_init_camera_on_terrain(hm_id)`
  - Inicializa la cámara en el centro del mapa, justo sobre el terreno.

---

### 🎮 Controles

- `heightmap_set_control_sensitivity(mouse_sens, move_speed, height_speed)`
  - Ajusta sensibilidad del ratón y velocidades.

- `heightmap_move_forward(speed)`
- `heightmap_move_backward(speed)`
- `heightmap_strafe_left(speed)`
- `heightmap_strafe_right(speed)`
- `heightmap_look_horizontal(delta_mouse_x)`
- `heightmap_look_vertical(delta_mouse_y)`
- `heightmap_adjust_height(delta)`

---

### 🧱 Movimiento con colisiones (versión extendida)

- `heightmap_move_forward_with_collision(hm_id, speed)`
- `heightmap_move_backward_with_collision(hm_id, speed)`
- `heightmap_strafe_left_with_collision(hm_id, speed)`
- `heightmap_strafe_right_with_collision(hm_id, speed)`
- `heightmap_check_terrain_collision(hm_id)`

---

### 🌊 Agua e iluminación

- `heightmap_set_water_level(height)`
  - Establece el nivel del agua (en coordenadas del heightmap).

- `heightmap_set_light(level)`
  - Ajusta la intensidad de luz (0 a 255).

- `heightmap_get_terrain_lighting(hm_id, sprite_x, sprite_y)`
  - Devuelve un valor entre 0-255 para ajustar el brillo de un sprite.

---

### 📊 Información del terreno

- `heightmap_get_height(hm_id, x, y)`
  - Devuelve la altura del terreno en coordenadas del mapa (x, y).

- `heightmap_get_terrain_height_at_sprite(hm_id, sprite_x, sprite_y)`
  - Devuelve la altura del terreno donde está el sprite.

- `heightmap_can_sprite_move_to(hm_id, new_x, new_y, sprite_height)`
  - Verifica si un sprite puede moverse sin colisión.

- `heightmap_adjust_sprite_to_terrain(hm_id, sprite_x, sprite_y, &adjusted_y)`
  - Ajusta la posición Y del sprite para alinearse con el terreno.

---

### 🎬 Renderizado

- `heightmap_render_voxelspace(hm_id)`
  - Renderiza la escena 3D al estilo Voxel Space (OutRun, Comanche).
  - Devuelve un ID de bitmap que puede mostrarse con `graph =`.

---

### 👤 Cámara siguiendo sprites

- `heightmap_set_camera_follow(sprite_id, offset_x, offset_y, offset_z, speed)`
  - Permite que la cámara siga a un sprite.
  - Usa valores -1 para dejar un parámetro sin modificar.

- `heightmap_update_camera_follow(hm_id, sprite_x, sprite_y)`
  - Actualiza la posición de la cámara para seguir al sprite.

- `heightmap_get_camera_follow()`
  - Devuelve el ID del sprite que está siguiendo la cámara.

---

### 🔁 Utilidades

- `heightmap_get_sprite_scale(sprite_x, sprite_y)`
  - Devuelve un porcentaje (100 = 100%) para escalar sprites según distancia a la cámara.

- `heightmap_world_to_screen(world_x, world_y, world_z, &screen_x, &screen_y)`
  - Convierte coordenadas 3D del mundo a posición en pantalla.

---

## 📌 Notas

- Las coordenadas de sprites se suelen pasar en escala 10x (por ejemplo, `(100, 200)` corresponde a `(10.0, 20.0)` en el mapa).
- Se recomienda usar imágenes RGB (24 bits) para el mapa de altura y la textura.
- El canal rojo del mapa de altura define la elevación del terreno.
- El sistema de cámara es en perspectiva tipo FPS (first person shooter).

---

## ✅ Requisitos

- BennuGD2 actualizado.
- SDL2 correctamente instalado (el módulo depende internamente de `SDL_GetRGB()` y `SDL_MapRGB()` para obtener colores exactos).
- Texturas y heightmaps deben tener el mismo tamaño o estar correctamente mapeadas.

---
