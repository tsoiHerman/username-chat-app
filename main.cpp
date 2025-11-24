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
    glm::vec3 Kd{1.0f,1.0f,1.0f}; // diffuse color
};

struct Model {
    std::vector<glm::vec3> vertices;
    std::vector<std::array<int,3>> faces;
    std::vector<std::string> faceMaterials;
    std::map<std::string,Material> materials;
};

// Global view control
float scale = 1.0f;
glm::vec2 panOffset(0.0f,0.0f);
float tiltOffset = 0.0f;
float orbitX = 0.0f;
float orbitY = 0.0f;

// Load MTL
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

// Load OBJ
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

// Project 3D point to 2D
glm::vec2 project(const glm::vec3 &v){
    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), orbitX, glm::vec3(0,1,0));
    rot = glm::rotate(rot, orbitY, glm::vec3(1,0,0));
    glm::vec4 pv = rot * glm::vec4(v,1.0f);
    return glm::vec2(pv.x*scale + WIDTH/2.0f + panOffset.x,
                     -pv.y*scale + HEIGHT/2.0f + panOffset.y + tiltOffset);
}

// Simple wireframe draw for a triangle
void drawTriangle(DrawingWindow &window, const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec3 &color){
    uint32_t c = (255<<24) | (uint32_t(color.r*255)<<16) | (uint32_t(color.g*255)<<8) | uint32_t(color.b*255);

    auto drawLine = [&](const glm::vec2 &a, const glm::vec2 &b){
        int x0=int(a.x), y0=int(a.y), x1=int(b.x), y1=int(b.y);
        int dx=abs(x1-x0), dy=abs(y1-y0);
        int sx=x0<x1?1:-1, sy=y0<y1?1:-1;
        int err=dx-dy;
        while(true){
            if(x0>=0 && x0<window.width && y0>=0 && y0<window.height) window.setPixelColour(x0,y0,c);
            if(x0==x1 && y0==y1) break;
            int e2 = 2*err;
            if(e2>-dy){ err-=dy; x0+=sx; }
            if(e2<dx){ err+=dx; y0+=sy; }
        }
    };

    drawLine(p0,p1);
    drawLine(p1,p2);
    drawLine(p2,p0);
}

// Draw model using flat shading (just edges for now)
void drawModelFlat(DrawingWindow &window, const Model &model){
    for(size_t i=0;i<model.faces.size();i++){
        const auto &f = model.faces[i];
        glm::vec2 pts[3];
        for(int j=0;j<3;j++) pts[j] = project(model.vertices[f[j]-1]);

        glm::vec3 kd(1.0f,1.0f,1.0f);
        auto it = model.materials.find(model.faceMaterials[i]);
        if(it!=model.materials.end()) kd = it->second.Kd;

        drawTriangle(window, pts[0], pts[1], pts[2], kd);
    }
}

// Bounding box
void computeBoundingBox(const Model &model, glm::vec3 &minV, glm::vec3 &maxV){
    if(model.vertices.empty()) return;
    minV = maxV = model.vertices[0];
    for(const auto &v:model.vertices){
        minV = glm::min(minV,v);
        maxV = glm::max(maxV,v);
    }
}

// Center model and auto-fit scale
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

// Draw model
void draw(DrawingWindow &window, const Model &model){
    window.clearPixels();
    drawModelFlat(window,model);
}

// Event handling
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
