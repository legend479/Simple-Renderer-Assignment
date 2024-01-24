#pragma once

#include "camera.h"
#include "surface.h" // contains BVH structure

struct Scene {
    std::vector<Surface> surfaces;
    Camera camera;
    Vector2i imageResolution;

    Scene() {};
    Scene(std::string sceneDirectory, std::string sceneJson);
    Scene(std::string pathToJson);
    
    void parse(std::string sceneDirectory, nlohmann::json sceneConfig);

    BVH_object bvh;
    BVH_object* Traverse_BVH(Ray& ray);
    void PopulateBVH(BVH_object* bvh);
    void PrintBVH(BVH_object* bvh, int lvl);

    Interaction rayIntersect(Ray& ray);
};