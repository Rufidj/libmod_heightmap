#version 330 core

in vec2 v_uv;

uniform sampler2D u_heightmap;
uniform sampler2D u_texturemap;
uniform sampler2D u_water_texture;
uniform sampler2D u_wall_texture;
uniform sampler2D u_floor_texture;
uniform sampler2D u_ceiling_texture;
uniform float u_has_ceiling;
uniform vec3 u_camera_pos;
uniform float u_camera_angle;
uniform float u_camera_pitch;
uniform float u_fov;
uniform float u_max_distance;
uniform float u_water_level;
uniform float u_water_time;
uniform float u_wave_amplitude;
uniform float u_light_intensity;
uniform vec2 u_heightmap_size;
uniform vec3 u_sky_color;
uniform float u_current_distance;
uniform vec3 u_fog_color;
uniform float u_fog_intensity;
uniform vec2 u_chunk_min;
uniform vec2 u_chunk_max;
uniform vec2 u_map_bounds_min;
uniform vec2 u_map_bounds_max;

uniform int u_map_type;
uniform int u_num_sectors;
uniform int u_num_walls;

out vec4 FragColor;

// -------------------------------------------------------------
// Simplex noise
// -------------------------------------------------------------
vec3 mod289(vec3 x){ return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod289(vec2 x){ return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute(vec3 x){ return mod289(((x*34.0)+1.0)*x); }

float snoise(vec2 v){
    const vec4 C = vec4(0.211324865405187, 0.366025403784439,
                       -0.577350269189626, 0.024390243902439);
    vec2 i = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);
    vec2 i1 = x0.x > x0.y ? vec2(1.0,0.0) : vec2(0.0,1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = mod289(i);
    vec3 p = permute( permute(i.y + vec3(0.0,i1.y,1.0)) + i.x + vec3(0.0,i1.x,1.0) );
    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
    m = m*m; m = m*m;
    vec3 x = 2.0 * fract(p* C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314*(a0*a0 + h*h);
    vec3 g;
    g.x = a0.x*x0.x + h.x*x0.y;
    g.yz = a0.yz*x12.xz + h.yz*x12.yw;
    return 130.0 * dot(m,g);
}

// -------------------------------------------------------------
// Bilinear filtering
// -------------------------------------------------------------
vec4 texture_bilinear(sampler2D tex, vec2 uv, vec2 ts){
    vec2 p = uv * ts;
    vec2 pf = floor(p - 0.5) + 0.5;
    vec2 f = p - pf;
    vec2 uv00 = pf / ts;
    vec2 uv10 = (pf + vec2(1,0)) / ts;
    vec2 uv01 = (pf + vec2(0,1)) / ts;
    vec2 uv11 = (pf + vec2(1,1)) / ts;
    vec4 c00 = texture(tex, uv00);
    vec4 c10 = texture(tex, uv10);
    vec4 c01 = texture(tex, uv01);
    vec4 c11 = texture(tex, uv11);
    return mix(mix(c00,c10,f.x), mix(c01,c11,f.x), f.y);
}

// -------------------------------------------------------------
// MAIN
// -------------------------------------------------------------
void main(){

    // ---------------------------------------------------------
    // DOOM MODE
    // ---------------------------------------------------------
    if(u_map_type == 1){

        float ang = u_camera_angle - u_fov*0.5 + v_uv.x*u_fov;
        float cx = cos(ang);
        float sy = sin(ang);
        vec2 rd = vec2(cx, sy);
        vec2 rp = u_camera_pos.xy;

        float tmin = 999999.0;
        bool hit = false;

        // NORTH
        if(rd.y>0.001){
            float t = (u_map_bounds_max.y - rp.y)/rd.y;
            if(t>0 && t<tmin){
                float hx = rp.x + rd.x*t;
                if(hx>=u_map_bounds_min.x && hx<=u_map_bounds_max.x){
                    tmin = t; hit = true;
                }
            }
        }
        // SOUTH
        if(rd.y<-0.001){
            float t = (u_map_bounds_min.y - rp.y)/rd.y;
            if(t>0 && t<tmin){
                float hx = rp.x + rd.x*t;
                if(hx>=u_map_bounds_min.x && hx<=u_map_bounds_max.x){
                    tmin = t; hit = true;
                }
            }
        }
        // EAST
        if(rd.x>0.001){
            float t = (u_map_bounds_max.x - rp.x)/rd.x;
            if(t>0 && t<tmin){
                float hy = rp.y + rd.y*t;
                if(hy>=u_map_bounds_min.y && hy<=u_map_bounds_max.y){
                    tmin = t; hit = true;
                }
            }
        }
        // WEST
        if(rd.x<-0.001){
            float t = (u_map_bounds_min.x - rp.x)/rd.x;
            if(t>0 && t<tmin){
                float hy = rp.y + rd.y*t;
                if(hy>=u_map_bounds_min.y && hy<=u_map_bounds_max.y){
                    tmin = t; hit = true;
                }
            }
        }

        if(hit){

            float wh = 150.0;
            float fh = 0.0;

            float wt = 0.5 + ((u_camera_pos.z - wh)/tmin*300.0 + u_camera_pitch*40.0)/240.0;
            float wb = 0.5 + ((u_camera_pos.z - fh)/tmin*300.0 + u_camera_pitch*40.0)/240.0;

            float eps = 0.002;

            // WALL
            if(v_uv.y >= wt-eps && v_uv.y <= wb+eps){

                float hx = rp.x + rd.x*tmin;
                float hy = rp.y + rd.y*tmin;

                float ucoord;
                if(abs(rd.y)>abs(rd.x))
                    ucoord = fract((hx - u_map_bounds_min.x)/(u_map_bounds_max.x-u_map_bounds_min.x));
                else
                    ucoord = fract((hy - u_map_bounds_min.y)/(u_map_bounds_max.y-u_map_bounds_min.y));

                float r = wb - wt;
                float vcoord = clamp((v_uv.y - wt)/r,0,1);

                vec2 ts = vec2(textureSize(u_wall_texture,0));
                vec3 col = texture_bilinear(u_wall_texture, vec2(ucoord,vcoord), ts).rgb;
                col *= u_light_intensity;

                float fog = clamp(1.0 - tmin/u_max_distance,0.3,1.0);
                vec3 fc = mix(u_sky_color, col, fog);

                FragColor = vec4(fc,1);
                return;
            }

            // FLOOR
            else if(v_uv.y > wb - eps){

                float sh = 0.0;
                float sy = (v_uv.y - 0.5)*2.0;
                float vf = u_fov*0.75;
                float th = tan(vf*0.5);
                float rpo = atan(sy*th);
                float rpz = u_camera_pitch + rpo;
                float sp = sin(rpz);

                if(abs(sp)>0.001){
                    float hd = u_camera_pos.z - sh;
                    float tf = hd/sp;
                    if(tf>0){
                        vec2 hit = u_camera_pos.xy + rd*tf;
                        vec2 uv = fract(hit/100.0);
                        vec2 ts = vec2(textureSize(u_floor_texture,0));
                        vec3 col = texture_bilinear(u_floor_texture, uv, ts).rgb*u_light_intensity;

                        float fog = clamp(1.0 - tf/u_max_distance,0.3,1.0);

                        FragColor = vec4(mix(u_sky_color,col,fog),1);
                        return;
                    }
                }

                FragColor = vec4(u_sky_color,1);
                return;
            }

            // CEILING
            else if(v_uv.y < wt + eps){

                if(u_has_ceiling>0.5){

                    float ch = 150.0;
                    float sy = (v_uv.y - 0.5)*2.0;
                    float vf = u_fov*0.75;
                    float th = tan(vf*0.5);
                    float rpo = atan(sy*th);
                    float rpz = u_camera_pitch + rpo;
                    float sp = sin(rpz);

                    if(abs(sp)>0.001){
                        float hd = u_camera_pos.z - ch;
                        float tc = hd/sp;
                        if(tc>0){
                            vec2 hit = u_camera_pos.xy + rd*tc;
                            vec2 uv = fract(hit/100.0);
                            vec2 ts = vec2(textureSize(u_ceiling_texture,0));
                            vec3 col = texture_bilinear(u_ceiling_texture, uv, ts).rgb*u_light_intensity;

                            float fog = clamp(1.0 - tc/u_max_distance,0.3,1.0);

                            FragColor = vec4(mix(u_sky_color,col,fog),1);
                            return;
                        }
                    }

                    FragColor = vec4(u_sky_color,1);
                    return;
                }

                FragColor = vec4(u_sky_color,1);
                return;
            }

            FragColor = vec4(0.5,0.5,0.5,1);
            return;
        }

        FragColor = vec4(u_sky_color,1);
        return;
    }

    // ---------------------------------------------------------
    // VOXELSPACE MODE
    // ---------------------------------------------------------
    float ang = u_camera_angle - u_fov*0.5 + v_uv.x*u_fov;
    float cx = cos(ang);
    float sy = sin(ang);

    vec2 wp = u_camera_pos.xy + vec2(cx,sy)*u_current_distance;

    if(wp.x<u_chunk_min.x || wp.x>u_chunk_max.x || wp.y<u_chunk_min.y || wp.y>u_chunk_max.y) discard;
    if(wp.x<0 || wp.x>=u_heightmap_size.x || wp.y<0 || wp.y>=u_heightmap_size.y) discard;

    vec2 uv = wp / u_heightmap_size;
    float th = texture(u_heightmap, uv).r * 255.0;
    bool water = (th < u_water_level);

    float rh;

    if(water){
        vec2 wc = wp * 0.01 + vec2(u_water_time*0.1, u_water_time*0.05);
        float wv = (snoise(wc)*0.5 + snoise(wc*2.0)*0.25 + snoise(wc*4.0)*0.125) * u_wave_amplitude;
        float ws = u_water_level + wv;
        bool under = (u_camera_pos.z < u_water_level);
        rh = under ? th : ws;
    } else {
        rh = th;
    }

    float hd = u_camera_pos.z - rh;
    float py = hd/u_current_distance*300.0 + u_camera_pitch*40.0;
    float sy_scr = 0.5 + py/240.0;

    // Pixel por encima del suelo
    if(v_uv.y < sy_scr){
        bool under = (u_camera_pos.z < u_water_level);
        if(under){
            vec3 col = u_sky_color;
            col.b *= 1.5;
            FragColor = vec4(col,1);
        } else {
            FragColor = vec4(u_sky_color,1);
        }
        return;
    }

    float fog_base = clamp(1.0 - u_current_distance/u_max_distance,0.3,1.0);

    float fog_factor = 0.0;
    if(u_fog_intensity>0.0){

        float df = 0.0;
        if(u_current_distance > u_max_distance*0.3){
            float fs = u_max_distance*0.3;
            float fr = u_max_distance - fs;
            float fp = (u_current_distance - fs)/fr;
            df = fp*fp * u_fog_intensity;
        }

        float hf = 0.0;
        if(rh<100.0)
            hf = (1.0 - rh/100.0)*u_fog_intensity*0.3;

        fog_factor = clamp(max(df,hf),0.0,0.7);
    }

    // TERRENO
    if(!water){

        vec2 ts = vec2(textureSize(u_texturemap,0));
        vec3 tc = texture_bilinear(u_texturemap, uv, ts).rgb * u_light_intensity;

        vec3 bc = mix(u_sky_color, tc, fog_base);
        if(fog_factor>0.1)
            bc = mix(bc, u_fog_color, fog_factor);

        FragColor = vec4(bc,1);
        return;
    }

    // AGUA
    bool under = (u_camera_pos.z < u_water_level);

    if(under){
        vec2 ts = vec2(textureSize(u_texturemap,0));
        vec3 tc = texture_bilinear(u_texturemap, uv, ts).rgb * (u_light_intensity*0.7);
        tc.b *= 1.3;

        vec3 bc = mix(u_sky_color, tc, fog_base);
        if(fog_factor>0.1)
            bc = mix(bc, u_fog_color, fog_factor);

        FragColor = vec4(bc,1);
        return;
    }

    float wd = u_water_level - th;
    float wa = clamp(wd/30.0, 0.4,0.85);

    vec2 wuv = mod(wp*0.01 + vec2(u_water_time*0.1,u_water_time*0.05),1.0);
    vec2 wts = vec2(textureSize(u_water_texture,0));
    vec4 wsmp = texture_bilinear(u_water_texture, wuv, wts);

    vec2 ts = vec2(textureSize(u_texturemap,0));
    vec3 tc = texture_bilinear(u_texturemap, uv, ts).rgb * u_light_intensity;

    vec3 vd = normalize(vec3(wp.x - u_camera_pos.x, wp.y - u_camera_pos.y, -1.0));
    vec3 wn = vec3(0,0,1);

    float fr = pow(1.0 - max(dot(-vd, wn),0.0),3.0);
    vec3 rc = u_sky_color;

    vec3 wc = mix(tc, wsmp.rgb, wa);
    wc = mix(wc, rc, fr*0.3);

    vec3 bc = mix(u_sky_color, wc, fog_base);
    if(fog_factor>0.1)
        bc = mix(bc, u_fog_color, fog_factor);

    FragColor = vec4(bc,1.0);
}
