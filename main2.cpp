#include <DrawingWindow.h>
#include <Utils.h>
#include <SDL2/SDL_image.h>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <array>
#include <algorithm>
#include <cmath>
#include <sys/stat.h>
#include <sys/types.h>

#define WIDTH 640
#define HEIGHT 480

// ============================================================
// ====================== FRAME SAVING =========================
// ============================================================

void ensureFramesFolder() {
#if defined(_WIN32)
    _mkdir("frames");
#else
    mkdir("frames", 0777);
#endif
}

void saveFramePNG(DrawingWindow &window, int frameNumber) {
    std::string num = std::to_string(frameNumber);
    num = std::string(5 - num.length(), '0') + num;  // zero pad
    std::string filename = "frames/frame_" + num + ".png";

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(
        0, WIDTH, HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888
    );

    uint32_t* pixels = (uint32_t*)surf->pixels;

    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            pixels[y * WIDTH + x] = window.getPixelColour(x, y);

    if (IMG_SavePNG(surf, filename.c_str()) != 0)
        std::cerr << "Failed to save PNG: " << IMG_GetError() << "\n";
    else
        std::cout << "Saved " << filename << "\n";

    SDL_FreeSurface(surf);
}

// ============================================================
// ==================== YOUR ORIGINAL CODE =====================
// ============================================================

struct Material {
    glm::vec3 Kd{1.0f,1.0f,1.0f};
    bool textured = false;
};

struct Model {
    std::vector<glm::vec3> vertices;
    std::vector<std::array<int,3>> faces;
    std::vector<std::string> faceMaterials;
    std::map<std::string,Material> materials;
    std::map<int, std::array<glm::vec2,3>> faceUVs;
    std::vector<glm::vec3> vertexNormals; // For Gouraud shading
};

float scale = 1.0f;
glm::vec2 panOffset(0.0f,0.0f);
float tiltOffset = 0.0f;
float orbitX = 0.0f;
float orbitY = 0.0f;

glm::vec3 lightPos(0.0f,6.4f,1.0f);
float ambientLight = 0.2f;

std::vector<float> zBuffer(WIDTH*HEIGHT, -1e10f);

SDL_Surface* floorTexture = nullptr;

bool loadTexture(const std::string &path){
    floorTexture = IMG_Load(path.c_str());
    if(!floorTexture){
        std::cerr << "Failed to load texture: " << path << std::endl;
        return false;
    }
    SDL_Surface* converted = SDL_ConvertSurfaceFormat(floorTexture, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(floorTexture);
    floorTexture = converted;
    return true;
}

// ---------- load MTL ----------
std::map<std::string,Material> loadMTL(const std::string &filename){
    std::map<std::string,Material> materials;
    std::ifstream file(filename);
    if(!file.is_open()){ std::cerr << "Failed to open MTL: " << filename << std::endl; return materials; }
    std::string line, currentMaterial;
    while(std::getline(file,line)){
        if(line.substr(0,6)=="newmtl"){ currentMaterial = line.substr(7); materials[currentMaterial] = Material{}; }
        else if(line.substr(0,2)=="Kd"){
            std::istringstream s(line.substr(3)); float r,g,b; s>>r>>g>>b;
            materials[currentMaterial].Kd = glm::vec3(r,g,b);
        }
    }
    return materials;
}

// ---------- load OBJ ----------
Model loadOBJ(const std::string &filename){
    Model model;
    std::ifstream file(filename);
    if(!file.is_open()){ std::cerr << "Failed to open OBJ: " << filename << std::endl; exit(1); }
    std::string line, currentMaterial;

    auto normalizeUV = [](float x,float z){
        float u = (x+3.0f)/6.0f;
        float v = (z+2.0f)/6.0f;
        return glm::vec2(u,v);
    };

    while(std::getline(file,line)){
        if(line.substr(0,2)=="v "){
            std::istringstream s(line.substr(2)); glm::vec3 v; s>>v.x>>v.y>>v.z;
            model.vertices.push_back(v);
        }
        else if(line.substr(0,6)=="usemtl") currentMaterial = line.substr(7);
        else if(line.substr(0,2)=="f "){
            std::istringstream s(line.substr(2)); std::array<int,3> f;
            for(int i=0;i<3;i++){
                s>>f[i];
                if(s.peek()=='/') while(s.peek()!=' ' && s.peek()!=EOF) s.get();
            }
            model.faces.push_back(f);
            model.faceMaterials.push_back(currentMaterial);

            if(currentMaterial=="Floor"){
                glm::vec3 v0=model.vertices[f[0]-1];
                glm::vec3 v1=model.vertices[f[1]-1];
                glm::vec3 v2=model.vertices[f[2]-1];
                model.faceUVs[model.faces.size()-1]={
                    { normalizeUV(v0.x,v0.z),
                      normalizeUV(v1.x,v1.z),
                      normalizeUV(v2.x,v2.z) }
                };
            }
        }
        else if(line.substr(0,6)=="mtllib"){
            std::string mtlFile=line.substr(7);
            model.materials = loadMTL("box/"+mtlFile);
            model.materials["Floor"].textured = true;
        }
    }

    // Compute per-vertex normals for Gouraud shading
    model.vertexNormals.resize(model.vertices.size(), glm::vec3(0.0f));
    for(size_t i=0;i<model.faces.size();i++){
        const auto &f = model.faces[i];
        glm::vec3 v0 = model.vertices[f[0]-1];
        glm::vec3 v1 = model.vertices[f[1]-1];
        glm::vec3 v2 = model.vertices[f[2]-1];
        glm::vec3 n = glm::normalize(glm::cross(v1-v0, v2-v0));
        model.vertexNormals[f[0]-1] += n;
        model.vertexNormals[f[1]-1] += n;
        model.vertexNormals[f[2]-1] += n;
    }
    for(auto &vn : model.vertexNormals) vn = glm::normalize(vn);

    return model;
}

glm::vec2 project(const glm::vec3 &v){
    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), orbitX, glm::vec3(0,1,0));
    rot = glm::rotate(rot, orbitY, glm::vec3(1,0,0));
    glm::vec4 pv = rot * glm::vec4(v,1.0f);
    return glm::vec2(pv.x*scale + WIDTH/2.0f + panOffset.x,
                     -pv.y*scale + HEIGHT/2.0f + panOffset.y + tiltOffset);
}

glm::vec3 barycentric(const glm::vec2 &p,const glm::vec2 &a,const glm::vec2 &b,const glm::vec2 &c){
    float det = (b.y-c.y)*(a.x-c.x) + (c.x-b.x)*(a.y-c.y);
    float w1 = ((b.y-c.y)*(p.x-c.x) + (c.x-b.x)*(p.y-c.y))/det;
    float w2 = ((c.y-a.y)*(p.x-c.x) + (a.x-c.x)*(p.y-c.y))/det;
    float w3 = 1.0f-w1-w2;
    return glm::vec3(w1,w2,w3);
}

glm::vec3 sampleTexture(int faceIndex,const glm::vec3 &bc,const Model &model){
    if(floorTexture==nullptr) return glm::vec3(1.0f,1.0f,1.0f);

    auto &uvs = model.faceUVs.at(faceIndex);
    float u = bc.x*uvs[0].x + bc.y*uvs[1].x + bc.z*uvs[2].x;
    float v = bc.x*uvs[0].y + bc.y*uvs[1].y + bc.z*uvs[2].y;

    u = std::clamp(u,0.0f,1.0f);
    v = std::clamp(v,0.0f,1.0f);

    int texX = int(u*(floorTexture->w-1));
    int texY = int((1.0f-v)*(floorTexture->h-1));

    uint32_t* pixels=(uint32_t*)floorTexture->pixels;
    uint32_t px = pixels[texY*floorTexture->w + texX];
    uint8_t r=(px>>16)&0xFF;
    uint8_t g=(px>>8)&0xFF;
    uint8_t b=px&0xFF;
    return glm::vec3(r/255.0f,g/255.0f,b/255.0f);
}

// ---------- HARD SHADOW HELPER ----------
bool inShadow(const glm::vec3 &point, const Model &model){
    glm::vec3 dir = glm::normalize(lightPos - point);
    float lightDist = glm::length(lightPos - point);

    for(size_t i=0;i<model.faces.size();i++){
        const auto &f = model.faces[i];
        glm::vec3 v0 = model.vertices[f[0]-1];
        glm::vec3 v1 = model.vertices[f[1]-1];
        glm::vec3 v2 = model.vertices[f[2]-1];

        glm::vec3 n = glm::normalize(glm::cross(v1-v0,v2-v0));
        float denom = glm::dot(n, dir);
        if(std::abs(denom) < 1e-5f) continue;

        float t = glm::dot(n, v0 - point) / denom;
        if(t>0.001f && t<lightDist) return true; // blocked
    }
    return false;
}

void drawTriangle(DrawingWindow &window,int faceIndex,
                  const glm::vec3 &v0,const glm::vec3 &v1,const glm::vec3 &v2,
                  const glm::vec3 &n0,const glm::vec3 &n1,const glm::vec3 &n2,
                  const Model &model){

    glm::vec2 p0=project(v0);
    glm::vec2 p1=project(v1);
    glm::vec2 p2=project(v2);

    int minX=std::floor(std::min({p0.x,p1.x,p2.x}));
    int maxX=std::ceil (std::max({p0.x,p1.x,p2.x}));
    int minY=std::floor(std::min({p0.y,p1.y,p2.y}));
    int maxY=std::ceil (std::max({p0.y,p1.y,p2.y}));

    minX=std::max(0,minX); maxX=std::min(WIDTH-1,maxX);
    minY=std::max(0,minY); maxY=std::min(HEIGHT-1,maxY);

    glm::vec3 centroid = (v0+v1+v2)/3.0f;
    bool shadowed = inShadow(centroid, model);

    // Compute vertex colors (Gouraud)
    glm::vec3 c0 = model.materials.at(model.faceMaterials[faceIndex]).Kd *
                   (ambientLight + (shadowed?0.0f: std::max(0.0f, glm::dot(glm::normalize(n0), glm::normalize(lightPos-v0)))));
    glm::vec3 c1 = model.materials.at(model.faceMaterials[faceIndex]).Kd *
                   (ambientLight + (shadowed?0.0f: std::max(0.0f, glm::dot(glm::normalize(n1), glm::normalize(lightPos-v1)))));
    glm::vec3 c2 = model.materials.at(model.faceMaterials[faceIndex]).Kd *
                   (ambientLight + (shadowed?0.0f: std::max(0.0f, glm::dot(glm::normalize(n2), glm::normalize(lightPos-v2)))));

    for(int y=minY;y<=maxY;y++){
        for(int x=minX;x<=maxX;x++){
            glm::vec3 bc = barycentric(glm::vec2(x+0.5f,y+0.5f),p0,p1,p2);
            if(bc.x>=0 && bc.y>=0 && bc.z>=0){
                float z = bc.x*v0.z + bc.y*v1.z + bc.z*v2.z;
                int idx = y*WIDTH + x;
                if(z>zBuffer[idx]){
                    zBuffer[idx]=z;

                    glm::vec3 color = bc.x*c0 + bc.y*c1 + bc.z*c2;

                    // Floor texture
                    if(model.faceMaterials[faceIndex]=="Floor")
                        color = sampleTexture(faceIndex, bc, model);

                    uint32_t c = (255<<24)
                                 | (uint32_t(std::clamp(color.r,0.0f,1.0f)*255)<<16)
                                 | (uint32_t(std::clamp(color.g,0.0f,1.0f)*255)<<8)
                                 | uint32_t(std::clamp(color.b,0.0f,1.0f)*255);

                    window.setPixelColour(x,y,c);
                }
            }
        }
    }
}

void drawModel(DrawingWindow &window,const Model &model){
    for(size_t i=0;i<model.faces.size();i++){
        const auto &f = model.faces[i];
        glm::vec3 v[3] = {
            model.vertices[f[0]-1],
            model.vertices[f[1]-1],
            model.vertices[f[2]-1]
        };
        glm::vec3 n[3] = {
            model.vertexNormals[f[0]-1],
            model.vertexNormals[f[1]-1],
            model.vertexNormals[f[2]-1]
        };
        drawTriangle(window,i,v[0],v[1],v[2], n[0], n[1], n[2], model);
    }
}

void computeBoundingBox(const Model &model, glm::vec3 &minV, glm::vec3 &maxV){
    if(model.vertices.empty()) return;
    minV=maxV=model.vertices[0];
    for(const auto &v:model.vertices){
        minV = glm::min(minV,v);
        maxV = glm::max(maxV,v);
    }
}

void centerModel(Model &model){
    glm::vec3 minV,maxV;
    computeBoundingBox(model,minV,maxV);
    glm::vec3 center = (minV+maxV)*0.5f;
    for(auto &v:model.vertices) v -= center;

    glm::vec3 size = maxV-minV;
    float scaleX = (WIDTH-40)/size.x;
    float scaleY = (HEIGHT-40)/size.y;
    scale = std::min(scaleX,scaleY);
}

void draw(DrawingWindow &window,const Model &model){
    window.clearPixels();
    std::fill(zBuffer.begin(), zBuffer.end(), -1e10f);
    drawModel(window,model);
}

void handleEvent(SDL_Event event){
    if(event.type==SDL_KEYDOWN){
        switch(event.key.keysym.sym){
            case SDLK_w: scale *= 1.1f; break;
            case SDLK_s: scale /= 1.1f; break;
            case SDLK_a: panOffset.x -= 10.0f; break;
            case SDLK_d: panOffset.x += 10.0f; break;
            case SDLK_q: tiltOffset += 10.0f; break;
            case SDLK_e: tiltOffset -= 10.0f; break;
            case SDLK_LEFT:  orbitX -= 0.1f; break;
            case SDLK_RIGHT: orbitX += 0.1f; break;
            case SDLK_UP:    orbitY += 0.1f; break;
            case SDLK_DOWN:  orbitY -= 0.1f; break;
        }
    }
}

// ============================================================
// ========================= MAIN ==============================
// ============================================================

int main(int argc,char *argv[]){
    if(SDL_Init(SDL_INIT_VIDEO)!=0){
        std::cerr<<"SDL Init Failed"<<std::endl;
        return -1;
    }
    if(!(IMG_Init(IMG_INIT_PNG)&IMG_INIT_PNG)){
        std::cerr<<"SDL_image Init Failed"<<std::endl;
        return -1;
    }

    DrawingWindow window(WIDTH,HEIGHT,false);
    SDL_Event event;

    if(!loadTexture("box/ground.png"))
        return -1;

    Model box = loadOBJ("box/box.obj");
    centerModel(box);

    ensureFramesFolder();
    int frameCounter = 0;

    Uint64 lastTime = SDL_GetPerformanceCounter();
    double targetFrame = 1.0 / 30.0;

    while(true){
        while(window.pollForInputEvents(event))
            handleEvent(event);

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = double(now - lastTime) / SDL_GetPerformanceFrequency();

        if(dt >= targetFrame){
            lastTime = now;

            // Simple animation: rotate model automatically
            orbitX += 0.01f;

            draw(window,box);
            window.renderFrame();

            saveFramePNG(window, frameCounter++);
        }
    }
}
