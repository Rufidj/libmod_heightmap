import "libmod_gfx";      
import "libmod_input";      
import "libmod_misc";      
import "libmod_heightmap";      
      
GLOBAL      
    int heightmap_id;      
    string base_path;      
    string assets_path;      
    int tree_graph;    
    int nave_graph;    
    float camera_distance = 30.0;    
    float camera_height = 15.0;    
    float current_angle;      
    float nave_x, nave_y, nave_z;  // Cambiar a int para coordenadas de proceso  
    int nave_id;  
    int bullet_graph;
    float bullet_X, bullet_y, bullet_z; // Coordenadas de prueba disparo
    int bullet_id;
    float enemy_x, enemy_y, enemy_z;
    int enemy_graph;
    int enemy_id;
    float enemy_shoot_x, enemy_shoot_y, enemy_shoot_z; 
    float exact_enemy_x, exact_enemy_y, exact_enemy_z;
    float global_enemy_shoot_x, global_enemy_shoot_y, global_enemy_shoot_z;
PROCESS main()      
BEGIN      
    set_mode(640, 480);      
    set_fps(60, 0);      
    window_set_title("Test Skybox Heightmap");      
      
    base_path = get_base_path();      
    assets_path = base_path + "assets/";      
        
    // Verificar carga de gráficos    
    tree_graph = map_load(assets_path + "tree.png");    
    if (!tree_graph)    
        say("Error: No se pudo cargar palmtree.png");    
    else    
        say("Palmtree cargado correctamente: ID " + tree_graph);    
    end    

    bullet_graph = map_load(assets_path + "bullet.png");    
    if (!tree_graph)    
        say("Error: No se pudo cargar bullet.png");    
    else    
        say("Bullet cargado correctamente: ID " + bullet_graph);    
    end    

    enemy_graph = map_load(assets_path + "enemy.png");    
    if (!tree_graph)    
        say("Error: No se pudo cargar enemy.png");    
    else    
        say("Enemy cargado correctamente: ID " + Enemy_graph);    
    end 
        
    nave_graph = map_load(assets_path + "sprite.png");    
    if (!nave_graph)    
        say("Error: No se pudo cargar sprite.png - La nave no aparecerá");    
        return;    
    else    
        say("Nave cargada correctamente: ID " + nave_graph);    
    end    
        
    // Cargar terreno      
    heightmap_id = HEIGHTMAP_LOAD(assets_path + "terrain.png");      
    if (!heightmap_id)      
        say("No se pudo cargar el terreno");      
        return;      
    end      
      
    // Configuración básica      
    HEIGHTMAP_LOAD_TEXTURE(heightmap_id, assets_path + "terrain_texture.png");      
    HEIGHTMAP_INIT_CAMERA_ON_TERRAIN(heightmap_id);      
    HEIGHTMAP_SET_LIGHT(200);      
    HEIGHTMAP_SET_RENDER_DISTANCE(5000);      
    HEIGHTMAP_SET_CHUNK_CONFIG(512, 512);    
    HEIGHTMAP_SET_WATER_TEXTURE(assets_path + "water.png", 30);    
    HEIGHTMAP_SET_WATER_LEVEL(20);    
    HEIGHTMAP_SET_WAVE_AMPLITUDE(20.0);    
    
    // Cargar textura del skybox      
    HEIGHTMAP_SET_SKY_TEXTURE(assets_path + "skybox.png", 1000);      
    HEIGHTMAP_SET_SKY_COLOR(135, 206, 235, 255);      

    // Añadir niebla
    HEIGHTMAP_SET_FOG_COLOR(255,255,255,1200);
    HEIGHTMAP_SET_BILLBOARD_FOV(2000);  // 1.5 radianes ≈ 86 grados
      
    // Crear procesos  
    terrain_display();      
    display_info();      
// En el proceso main, después de crear la nave  
nave_id = nave();  
enemy_id = enemy();
  
// Posicionar cámara inicialmente en la posición de la nave  
HEIGHTMAP_SET_CAMERA(  
    nave_x - 80,    // X detrás de la nave (usando offset)  
    nave_y,         // Y igual que la nave  
    nave_z + 30,    // Z un poco más alto  
    0,              // Ángulo horizontal  
    -300,           // Pitch hacia abajo (en milésimas)  
    60000           // FOV  
);  
   
// Configurar el seguimiento automático con la rotación corregida  
HEIGHTMAP_SET_CAMERA_FOLLOW(    
    nave_id,        
    0,              // Offset X  
    -4500,           // Offset Y hacia atrás    
    30,             // Altura de la cámara  
    8               // Velocidad de seguimiento  
);
// En el bucle principal, actualizar el seguimiento  
LOOP      
    HEIGHTMAP_UPDATE_WATER_TIME();  
      
    if (nave_id != 0)  
          HEIGHTMAP_UPDATE_CAMERA_FOLLOW(heightmap_id, nave_x, nave_y, nave_z); 
       say("Pos nave: " + nave_x + "," + nave_y + "," + nave_z);
    end  
      
    if(key(_esc)) exit(); end      
    FRAME;      
END
end 
         
      
PROCESS display_info()        
PRIVATE  
    int cam_x, cam_y, cam_z, cam_angle, cam_pitch;  
    int debug_counter = 0;  
BEGIN        
    LOOP        
        // Obtener posición actual de la cámara  
        HEIGHTMAP_GET_CAMERA_POSITION(&cam_x, &cam_y, &cam_z, &cam_angle, &cam_pitch);  
          
        write(0, 10, 10, 0, "FPS: " + frame_info.fps);        
        write(0, 10, 30, 0, "WASD: Mover nave | QE: Subir/Bajar");        
        write(0, 10, 50, 0, "ESC: Salir");    
        write(0, 10, 70, 0, "Pos nave: " + nave_x + "," + nave_y + "," + nave_z);  
          
        // Debug de cámara en pantalla  
        write(0, 10, 90, 0, "Cam pos: " + cam_x + "," + cam_y + "," + cam_z);  
        write(0, 10, 110, 0, "Cam angle: " + cam_angle + " pitch: " + cam_pitch);  
        write(0, 10, 130, 0, "Distancia nave-cam: " + sqrt((nave_x-cam_x)*(nave_x-cam_x) + (nave_y-cam_y)*(nave_y-cam_y)));  
          
        // Debug con say() cada 60 frames (1 segundo a 60fps)  
        debug_counter++;  
        if (debug_counter >= 60) 
            say("=== DEBUG CAMARA ===");  
            say("Nave: " + nave_x + "," + nave_y + "," + nave_z);  
            say("Cam: " + cam_x + "," + cam_y + "," + cam_z);  
            say("Angle: " + cam_angle + " Pitch: " + cam_pitch);  
            say("Distancia: " + sqrt((nave_x-cam_x)*(nave_x-cam_x) + (nave_y-cam_y)*(nave_y-cam_y)));  
            say ("====DISPARO====");
            say ("Disparo " + bullet_x + "," + bullet_y + "," + bullet_z);
            say("==================");  
            debug_counter = 0;  
        end  
            
        FRAME;        
        write_delete(all_text);        
    END        
END

 PROCESS terrain_display()          
PRIVATE    
    int static_billboards_created = 0;  
    int i, j;  
    float tree_x, tree_y;  
    int trees_created = 0;  
    int grid_size = 2; // 14x14 = 196 árboles (cerca de 200)  
    float spacing_x, spacing_y;  
BEGIN          
    LOOP          
        graph = HEIGHTMAP_RENDER_3D(heightmap_id, 320, 240);          
        x = 320;          
        y = 240;          
        size = 200;          
        
        if (!static_billboards_created)  
            say("Creando árboles en grid " + grid_size + "x" + grid_size + "...");  
              
            spacing_x = 1900.0 / grid_size; // Espaciado en X  
            spacing_y = 1900.0 / grid_size; // Espaciado en Y  
            for (i = 0; i < grid_size; i++)  
                for (j = 0; j < grid_size; j++)  
                    // Posición base del grid con variación aleatoria  
                    tree_x = 100 + (i * spacing_x) + rand(-30, 30);  
                    tree_y = 100 + (j * spacing_y) + rand(-30, 30);  
                      
                    // Asegurar que está dentro de los límites  
                    if (tree_x < 100) tree_x = 100; end  
                    if (tree_x > 2000) tree_x = 2000; end  
                    if (tree_y < 100) tree_y = 100; end  
                    if (tree_y > 2000) tree_y = 2000; end  
                      
                    if (HEIGHTMAP_ADD_VOXEL_BILLBOARD(tree_x, tree_y, 10.0, tree_graph, 1-0) >= 0)  
                        trees_created++;  
                    end  
                end  
            end  
              
            say("Árboles creados: " + trees_created);  
            static_billboards_created = 1;    
        end    
            
        FRAME;          
    END          
END
  
PROCESS nave()                  
PRIVATE                  
    int billboard_index;                
    int speed = 4;                
    int terrain_z;    
    float new_x, new_y;  // CAMBIAR A FLOAT              
BEGIN                  
    // CORRECCIÓN: Usar float explícitamente  
    x = 1010.0;    // Coordenada X como float  
    y = 1858.0;    // Coordenada Y como float    
    z = 300.0;     // Altura como float  
    billboard_index = HEIGHTMAP_REGISTER_BILLBOARD(id, x, y, z, nave_graph,1);             
                      
    LOOP                  
        new_x = x;    
        new_y = y;    
            
        if (key(_s)) new_y -= speed; end        
        if (key(_w)) new_y += speed; end        
        if (key(_a)) new_x -= speed; end        
        if (key(_d)) new_x += speed; end        
            
        // Límites con float  
        if (new_x < 100.0) new_x = 100.0; end             
        if (new_x > 2000.0) new_x = 2000.0; end           
        if (new_y < 60.0) new_y = 60.0; end             
        if (new_y > 2000.0) new_y = 2000.0; end  
            
        x = new_x;    
        y = new_y;    
            
        if (key(_q)) z += speed; end              
        if (key(_e)) z -= speed; end              
                      
        nave_x = x; nave_y = y; nave_z = z;            
        HEIGHTMAP_UPDATE_BILLBOARD(id, x, y, z);   // Ahora pasa float correctamente  
          
        if (key(_space)) bullet_id = bullet(); end                
        FRAME;                
    END                
END

process bullet();      
private      
    int billboard_bullet;      
    int speed = 5;      
    int unique_bullet_id;      
begin      
    // CORREGIDO: Aplicar offset para que salga desde la proa  
    x = father.x;       
    y = father.y + 20;  // Offset hacia adelante  
    z = father.z;       // Misma altura  
          
    unique_bullet_id = -id;      
          
    // CORREGIDO: Escala más pequeña para mejor visibilidad  
    billboard_bullet = HEIGHTMAP_REGISTER_BILLBOARD(unique_bullet_id, x, y, z, bullet_graph,3);      
          
    if (billboard_bullet < 0)      
        return;      
    end      
          
    loop      
        y += speed;  // Mover hacia adelante  
              
        bullet_x = x;       
        bullet_y = y;       
        bullet_z = z;      
              
        HEIGHTMAP_UPDATE_BILLBOARD(unique_bullet_id, x, y, z);      
              
        if (y > 2000 || y < 0)    
            break;      
        end      
              
        FRAME;      
    END      
          
    HEIGHTMAP_UNREGISTER_BILLBOARD(unique_bullet_id);      
END


process enemy();            
private             
    int billboard_enemy;            
    float base_enemy_y = 1900.0;    // Al fondo del mapa    
    float enemy_x, enemy_y, enemy_z = 300.0;         
    int speed = 3;               // AUMENTADO: de 1 a 3 para mayor velocidad  
    int direction = 1;            
    float min_x = 20.0;                
    float max_x = 1980.0;              
    int shoot_timer = 0;            
    int shoot_delay = 60;            
            
begin             
    enemy_x = min_x;    
    enemy_y = base_enemy_y;    
      
    // CRÍTICO: Inicializar coordenadas del proceso  
    x = enemy_x;  
    y = enemy_y;  
    z = enemy_z;  
        
    billboard_enemy = HEIGHTMAP_REGISTER_BILLBOARD(id, enemy_x, enemy_y, enemy_z, enemy_graph,2);      
          
    if (billboard_enemy < 0)      
        say("ERROR: No se pudo registrar billboard enemigo");      
        return;      
    else      
        say("Enemigo registrado OK en: " + enemy_x + "," + enemy_y + "," + enemy_z);      
    end      
                  
    loop        
        enemy_x += speed * direction;  
            
        if (enemy_x >= max_x)     
            direction = -1;        
            enemy_x = max_x;    
        end    
        if (enemy_x <= min_x)     
            direction = 1;        
            enemy_x = min_x;    
        end    
          
        // CRÍTICO: Actualizar coordenadas del proceso ANTES del disparo  
        x = enemy_x;  
        y = enemy_y;  
        z = enemy_z;  
                
        HEIGHTMAP_UPDATE_BILLBOARD(id, enemy_x, enemy_y, enemy_z);        
                
        shoot_timer++;        
        if (shoot_timer >= shoot_delay)        
            // DEBUG: Verificar coordenadas antes del disparo  
            say("Enemy shooting from: " + x + "," + y + "," + z);  
            enemy_bullet();        
            shoot_timer = 0;        
        end        
                
        frame;        
    end        
end
  
process enemy_bullet();        
private        
    int billboard_bullet;        
    int speed = 4;        
    int unique_bullet_id;        
begin        
    x = father.x;       
    y = father.y;    
    z = father.z;     
            
    unique_bullet_id = -id;        
            
    // CORREGIDO: Solo 5 parámetros, sin scale  
    billboard_bullet = HEIGHTMAP_REGISTER_BILLBOARD(unique_bullet_id, x, y, z, bullet_graph,3);        
            
    if (billboard_bullet < 0)        
        say("ERROR: No se pudo registrar bullet enemigo");  
        return;        
    end        
            
    loop        
        y -= speed;    
                
        HEIGHTMAP_UPDATE_BILLBOARD(unique_bullet_id, x, y, z);        
                
        if (y < 0 || y > 2000)        
            break;        
        end        
                
        frame;        
    end        
            
    HEIGHTMAP_UNREGISTER_BILLBOARD(unique_bullet_id);        
end