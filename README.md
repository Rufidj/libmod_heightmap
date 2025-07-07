# libmod_heightmap - Módulo de terreno 3D para BennuGD2

Este módulo permite trabajar con terrenos en 3D simulados mediante heightmaps (mapas de altura) y renderizarlos con estilo voxelspace. Incluye funciones de control de cámara, detección de colisiones, iluminación, nivel de agua, sprites en el mundo y seguimiento de cámara.

---

![Imagen 1 ](/images/screen1.png)
![Imagen 2](/images/screen2.png)
![Imagen 3](/images/screen3.png)
![Imagen 4](/images/screen4.png)
![Imagen 5](/images/screen5.png)
![Imagen 6](/images/screen6.png)

---

## Tabla de Contenido

- [Constantes Exportadas](#constantes-exportadas)
- [Funciones Exportadas](#funciones-exportadas)
  - [Carga y Creación de Terrenos](#carga-y-creación-de-terrenos)
  - [Renderizado](#renderizado)
  - [Cámara](#cámara)
  - [Control de Movimiento](#control-de-movimiento)
  - [Iluminación y Cielo](#iluminación-y-cielo)
  - [Agua](#agua)
  - [Detección de Altura y Colisión](#detección-de-altura-y-colisión)
  - [Sprites en el Mundo 3D](#sprites-en-el-mundo-3d)
  - [Billboards](#billboards)
  - [Seguimiento de Cámara](#seguimiento-de-cámara)
- [Requisitos](#requisitos)
- [Créditos](#créditos)
- [Ejemplo básico de uso](#ejemplo-básico-de-uso)

---

## Constantes Exportadas

| Constante                  | Descripción                                      |
|---------------------------|--------------------------------------------------|
| `HEIGHTMAP_MAX`           | Número máximo de heightmaps permitidos          |
| `HEIGHTMAP_DEFAULT_FOV`   | Campo de visión por defecto (FOV x1000)         |
| `C_BILLBOARD`             | Flag para definir un sprite como billboard      |

---

## Funciones Exportadas

### Carga y Creación de Terrenos

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_LOAD(filename)` | Carga un heightmap desde archivo. Retorna ID. |
| `HEIGHTMAP_LOAD_TEXTURE(id, filename)` | Asocia una textura al heightmap especificado. |
| `HEIGHTMAP_CREATE(width, height)` | Crea un heightmap vacío de dimensiones dadas. |
| `HEIGHTMAP_CREATE_PROCEDURAL(width, height)` | Crea un terreno con generación procedural. |
| `HEIGHTMAP_UNLOAD(id)` | Libera el heightmap de la memoria. |

---

### Renderizado

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_RENDER_3D(id, width, height)` | Renderiza el terreno en pantalla. |
| `HEIGHTMAP_SET_RENDER_DISTANCE(distance)` | Define la distancia máxima de renderizado. |
| `HEIGHTMAP_SET_CHUNK_CONFIG(width, height)` | Configura el tamaño de los "chunks" internos. |

---

### Cámara

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_CAMERA(x, y, z, angle, pitch, zoom)` | Define la posición y orientación de la cámara. |
| `HEIGHTMAP_INIT_CAMERA_ON_TERRAIN(id)` | Inicializa la cámara sobre el terreno dado. |
| `HEIGHTMAP_GET_CAMERA_POSITION(&x, &y, &z, &angle, &pitch)` | Devuelve la posición actual de la cámara. |

---

### Control de Movimiento

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_CONTROL_SENSITIVITY(forward, strafe, look)` | Ajusta la sensibilidad del control. |
| `HEIGHTMAP_MOVE_FORWARD()` | Avanza la cámara. |
| `HEIGHTMAP_MOVE_FORWARD(speed)` | Avanza con velocidad específica. |
| `HEIGHTMAP_MOVE_BACKWARD()` | Retrocede. |
| `HEIGHTMAP_MOVE_BACKWARD(speed)` | Retrocede con velocidad específica. |
| `HEIGHTMAP_STRAFE_LEFT()` | Se mueve lateralmente a la izquierda. |
| `HEIGHTMAP_STRAFE_LEFT(speed)` | Igual, con velocidad. |
| `HEIGHTMAP_STRAFE_RIGHT()` | Se mueve a la derecha. |
| `HEIGHTMAP_STRAFE_RIGHT(speed)` | Igual, con velocidad. |
| `HEIGHTMAP_LOOK_HORIZONTAL(value)` | Rota la cámara en horizontal. |
| `HEIGHTMAP_LOOK_VERTICAL(value)` | Rota la cámara en vertical. |
| `HEIGHTMAP_ADJUST_HEIGHT(value)` | Ajusta la altura de la cámara. |

---

### Iluminación y Cielo

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_LIGHT(intensity)` | Define la iluminación general del terreno. |
| `HEIGHTMAP_SET_SKY_COLOR(r, g, b, alpha)` | Color del cielo. |
| `HEIGHTMAP_SET_SKY_TEXTURE(filename, id)` | Aplica una textura como cielo. |
| `HEIGHTMAP_GET_TERRAIN_LIGHTING(x, y, z)` | Luz del terreno en esa posición. |

---

### Agua

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_WATER_LEVEL(level)` | Define la altura del agua. |
| `HEIGHTMAP_SET_WATER_COLOR(r, g, b, alpha)` | Define el color y transparencia del agua. |
| `HEIGHTMAP_UPDATE_WATER_TIME()` | Actualiza la animación de las olas. |
| `HEIGHTMAP_SET_WATER_TEXTURE(filename, id)` | Asigna una textura animada de agua. |
| `HEIGHTMAP_SET_WAVE_AMPLITUDE(amplitude)` | Ajusta la fuerza de las olas. |

---

### Detección de Altura y Colisión

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_GET_HEIGHT(id, x, y)` | Obtiene altura del terreno en esas coordenadas. |
| `HEIGHTMAP_CHECK_TERRAIN_COLLISION(radius)` | Verifica colisión en la posición actual de cámara. |
| `HEIGHTMAP_MOVE_FORWARD_WITH_COLLISION(speed, radius)` | Avanza con detección de colisiones. |
| `HEIGHTMAP_MOVE_BACKWARD_WITH_COLLISION(speed, radius)` | Retrocede con detección. |
| `HEIGHTMAP_STRAFE_LEFT_WITH_COLLISION(speed, radius)` | Mueve a la izquierda con colisión. |
| `HEIGHTMAP_STRAFE_RIGHT_WITH_COLLISION(speed, radius)` | Mueve a la derecha con colisión. |
| `HEIGHTMAP_CAN_SPRITE_MOVE_TO(x, y, z, radius)` | Verifica si un sprite puede moverse a esa posición. |
| `HEIGHTMAP_GET_TERRAIN_HEIGHT_AT_SPRITE(id, x, y)` | Altura del terreno bajo un sprite. |
| `HEIGHTMAP_ADJUST_SPRITE_TO_TERRAIN(id, sprite_id, offset, &result)` | Ajusta sprite a la superficie del terreno. |

---

### Sprites en el Mundo 3D

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_WORLD_TO_SCREEN(x, y, z, &screen_x, &screen_y)` | Convierte coordenadas del mundo a pantalla. |

---

### Billboards

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_BILLBOARD(sprite_id)` | Marca un sprite como billboard. |
| `HEIGHTMAP_UNSET_BILLBOARD(sprite_id)` | Elimina el modo billboard. |
| `HEIGHTMAP_IS_BILLBOARD(sprite_id)` | Verifica si un sprite es billboard. |
| `HEIGHTMAP_PROJECT_BILLBOARD(x, y, z, size)` | Proyecta un sprite como billboard. |
| `HEIGHTMAP_CONVERT_SCREEN_TO_WORLD_X(id, screen_x)` | Convierte X pantalla a mundo. |
| `HEIGHTMAP_CONVERT_SCREEN_TO_WORLD_Y(id, screen_y)` | Convierte Y pantalla a mundo. |
| `HEIGHTMAP_ADD_VOXEL_BILLBOARD(x, y, graph_id, scale)` | Añade billboard tipo voxel. |
| `HEIGHTMAP_REGISTER_BILLBOARD(id, x, y, z, graph)` | Registra un billboard en el sistema. |
| `HEIGHTMAP_UPDATE_BILLBOARD(id, x, y, z)` | Actualiza posición de billboard. |
| `HEIGHTMAP_UNREGISTER_BILLBOARD(id)` | Elimina un billboard del sistema. |

---

### Seguimiento de Cámara

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_CAMERA_FOLLOW(sprite_id, offset_x, offset_y, offset_z, style)` | Hace que la cámara siga un sprite. |
| `HEIGHTMAP_UPDATE_CAMERA_FOLLOW(offset_angle, offset_pitch, style)` | Actualiza los parámetros de seguimiento. |
| `HEIGHTMAP_GET_CAMERA_FOLLOW()` | Devuelve el ID del sprite seguido. |

---

## Requisitos

- **BennuGD2**
- **SDL2**
- Texturas en formato soportado (ej: PNG)
- Heightmaps en formato RAW o PNG

---

## Créditos

Desarrollado como módulo adicional para BennuGD2 por la comunidad.

---

## Ejemplo básico de uso

Consulta la sección de ejemplo en el archivo fuente para ver un programa completo con control de cámara, renderizado y uso del ratón.

---

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Rufidj/libmod_heightmap)
