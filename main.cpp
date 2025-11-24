#include <DrawingWindow.h>
#include <Utils.h>
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

#define WIDTH 640
#define HEIGHT 480

struct Material {
    glm::vec3 Kd{1.0f,1.0f,1.0f};
};

struct Model {
    std::vector<glm::vec3> vertices;
    std::vector<std::array<int,3>> faces;
    std::vector<std::string> faceMaterials;
    std::map<std::string,Material> materials;
};

// ------------------- Global view -------------------
float scale = 1.0f;
glm::vec2 panOffset(0.0f,0.0f);
float tiltOffset = 0.0f;
float orbitX = 0.0f;
float orbitY = 0.0f;

// ------------------- Lighting -------------------
glm::vec3 lightPos(0.0f, 6.4f, 1.0f);  // ceiling light
float ambientLight = 0.2f;

// ------------------- Z-buffer -------------------
std::vector<float> zBuffer(WIDTH*HEIGHT, -1e10f);

// ------------------- OBJ / MTL loading -------------------
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

Model loadOBJ(const std::string &filename){
    Model model;
    std::ifstream file(filename);
    if(!file.is_open()){ std::cerr << "Failed to open OBJ: " << filename << std::endl; exit(1); }
    std::string line, currentMaterial;
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
        }
        else if(line.substr(0,6)=="mtllib"){
            std::string mtlFile=line.substr(7);
            model.materials = loadMTL("box/"+mtlFile);
        }
    }
    return model;
}

// ------------------- Projection -------------------
glm::vec2 project(const glm::vec3 &v){
    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), orbitX, glm::vec3(0,1,0));
    rot = glm::rotate(rot, orbitY, glm::vec3(1,0,0));
    glm::vec4 pv = rot * glm::vec4(v,1.0f);
    return glm::vec2(pv.x*scale + WIDTH/2.0f + panOffset.x,
                     -pv.y*scale + HEIGHT/2.0f + panOffset.y + tiltOffset);
}

// ------------------- Rasterization -------------------
glm::vec3 barycentric(const glm::vec2 &p, const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c){
    float det = (b.y-c.y)*(a.x-c.x) + (c.x-b.x)*(a.y-c.y);
    float w1 = ((b.y-c.y)*(p.x-c.x) + (c.x-b.x)*(p.y-c.y)) / det;
    float w2 = ((c.y-a.y)*(p.x-c.x) + (a.x-c.x)*(p.y-c.y)) / det;
    float w3 = 1.0f - w1 - w2;
    return glm::vec3(w1,w2,w3);
}

glm::vec3 faceNormal(const glm::vec3 &v0,const glm::vec3 &v1,const glm::vec3 &v2){
    return glm::normalize(glm::cross(v1-v0,v2-v0));
}

void drawTriangleFlat(DrawingWindow &window,
                      const glm::vec3 &v0,const glm::vec3 &v1,const glm::vec3 &v2,
                      const glm::vec3 &color){
    glm::vec2 p0 = project(v0);
    glm::vec2 p1 = project(v1);
    glm::vec2 p2 = project(v2);

    int minX = std::floor(std::min({p0.x,p1.x,p2.x}));
    int maxX = std::ceil (std::max({p0.x,p1.x,p2.x}));
    int minY = std::floor(std::min({p0.y,p1.y,p2.y}));
    int maxY = std::ceil (std::max({p0.y,p1.y,p2.y}));

    minX = std::max(0,minX); maxX = std::min(WIDTH-1,maxX);
    minY = std::max(0,minY); maxY = std::min(HEIGHT-1,maxY);

    glm::vec3 n = faceNormal(v0,v1,v2);

    // Light vector (point light)
    glm::vec3 lightVec = glm::normalize(lightPos - (v0+v1+v2)/3.0f);
    float diffuse = std::max(0.0f, glm::dot(n, lightVec));
    float intensity = ambientLight + (1.0f - ambientLight) * diffuse;

    glm::vec3 shadedColor = color * intensity;

    uint32_t c = (255<<24) | (uint32_t(shadedColor.r*255)<<16) |
                 (uint32_t(shadedColor.g*255)<<8) | uint32_t(shadedColor.b*255);

    for(int y=minY;y<=maxY;y++){
        for(int x=minX;x<=maxX;x++){
            glm::vec3 bc = barycentric(glm::vec2(x+0.5f,y+0.5f),p0,p1,p2);
            if(bc.x >= 0 && bc.y >= 0 && bc.z >= 0){
                float z = bc.x*v0.z + bc.y*v1.z + bc.z*v2.z;
                int idx = y*WIDTH + x;
                if(z > zBuffer[idx]){
                    zBuffer[idx] = z;
                    window.setPixelColour(x,y,c);
                }
            }
        }
    }
}

void drawModelFlat(DrawingWindow &window, const Model &model){
    for(size_t i=0;i<model.faces.size();i++){
        const auto &f = model.faces[i];
        glm::vec3 v[3];
        for(int j=0;j<3;j++) v[j] = model.vertices[f[j]-1];

        glm::vec3 kd(1.0f,1.0f,1.0f);
        auto it = model.materials.find(model.faceMaterials[i]);
        if(it!=model.materials.end()) kd = it->second.Kd;

        drawTriangleFlat(window,v[0],v[1],v[2],kd);
    }
}

// ------------------- Bounding box & centering -------------------
void computeBoundingBox(const Model &model, glm::vec3 &minV, glm::vec3 &maxV){
    if(model.vertices.empty()) return;
    minV = maxV = model.vertices[0];
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
    scale = std::min(scaleX, scaleY);
}

// ------------------- Draw -------------------
void draw(DrawingWindow &window,const Model &model){
    window.clearPixels();
    std::fill(zBuffer.begin(), zBuffer.end(), -1e10f);
    drawModelFlat(window,model);
}

// ------------------- Event handling -------------------
void handleEvent(SDL_Event event){
    if(event.type==SDL_KEYDOWN){
        switch(event.key.keysym.sym){
            case SDLK_w: scale *= 1.1f; break;
            case SDLK_s: scale /= 1.1f; break;
            case SDLK_a: panOffset.x -= 10.0f; break;
            case SDLK_d: panOffset.x += 10.0f; break;
            case SDLK_q: tiltOffset += 10.0f; break;
            case SDLK_e: tiltOffset -= 10.0f; break;
            case SDLK_LEFT: orbitX -= 0.1f; break;
            case SDLK_RIGHT: orbitX += 0.1f; break;
            case SDLK_UP: orbitY += 0.1f; break;
            case SDLK_DOWN: orbitY -= 0.1f; break;
        }
    }
}

// ------------------- Main -------------------
int main(int argc,char *argv[]){
    DrawingWindow window(WIDTH,HEIGHT,false);
    SDL_Event event;

    Model box = loadOBJ("box/box.obj");
    centerModel(box);

    while(true){
        if(window.pollForInputEvents(event)) handleEvent(event);
        draw(window,box);
        window.renderFrame();
    }
}
