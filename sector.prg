import "libmod_gfx";    
import "libmod_input";    
import "libmod_misc";    
import "libmod_heightmap";    
    
GLOBAL    
    int graph_id;    
    string assets_path;    
    int wld_map;  
END    
    
PROCESS main()  
PRIVATE    
    int cam_x, cam_y, cam_z, cam_angle, cam_pitch;    
BEGIN  
    set_mode(640, 480);    
    set_fps(0, 0);    
    window_set_title("WLD 3D Renderer");    
      
    assets_path = "assets/";    
      
    if (!LOAD_TEX_FILE("textures.tex"))   
        say("ERROR: No se pudo cargar el archivo TEX");    
        return;    
    end    
      
    if (!LOAD_WLD("test.wld"))    
        say("ERROR: No se pudo cargar el archivo WLD");    
        return;    
    end    
      
    // Usar coordenadas que funcionan  
    HEIGHTMAP_SET_CAMERA(5700, 6000, 1160, 0, 0, 60000);    
      
    wld_display();    
      
    LOOP    
        // Controles de cámara horizontal  
        if (key(_w)) HEIGHTMAP_MOVE_FORWARD(5); end    
        if (key(_s)) HEIGHTMAP_MOVE_BACKWARD(5); end    
        if (key(_a)) HEIGHTMAP_STRAFE_LEFT(5); end    
        if (key(_d)) HEIGHTMAP_STRAFE_RIGHT(5); end    
          
        // Controles de rotación  
        if (key(_left)) HEIGHTMAP_LOOK_HORIZONTAL(-5); end    
        if (key(_right)) HEIGHTMAP_LOOK_HORIZONTAL(5); end    
        if (key(_up)) HEIGHTMAP_LOOK_VERTICAL(5); end    
        if (key(_down)) HEIGHTMAP_LOOK_VERTICAL(-5); end    
            
        // NUEVO: Movimiento vertical (subir/bajar)  
        if (key(_q)) HEIGHTMAP_ADJUST_HEIGHT(-5); end   // Bajar  
        if (key(_e)) HEIGHTMAP_ADJUST_HEIGHT(5); end    // Subir  
            
        // Mostrar información de depuración    
        HEIGHTMAP_GET_CAMERA_POSITION(&cam_x, &cam_y, &cam_z, &cam_angle, &cam_pitch);    
        write(0, 10, 10, 0, "POS: " + cam_x + "," + cam_y + "," + cam_z);    
        write(0, 10, 30, 0, "WASD: Mover | QE: Subir/Bajar | Flechas: Rotar | ESC: Salir");    
            
        if (key(_esc)) exit(); end    
        FRAME;    
        WRITE_DELETE(all_text);
    END    
END
    
PROCESS wld_display()    
BEGIN    
    LOOP    
        // Renderizar WLD en 3D    
        graph_id = HEIGHTMAP_RENDER_WLD_3D(320, 240);    
            
        if (graph_id)   
            graph = graph_id;    
            x = 320;    
            y = 240;    
            size = 200;    
        else    
            say("ERROR: No se pudo renderizar el WLD");    
        end    
            
        FRAME;    
    END    
END