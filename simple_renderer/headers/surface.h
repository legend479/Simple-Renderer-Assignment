#pragma once

#include "common.h"
#include "texture.h"

struct BVH_Triangles {
    BVH_Triangles* left;
    BVH_Triangles* right;
    std:: vector<Vector3f> vertices; // array of vertices in BVH Node
    std:: vector<Vector3f> normals; // array of pointers to normals in BVH Node
    long int Num_Of_Triangles;
    Vector3f aabb[2]; // Axis-aligned bounding box (min, max)

    bool slab_test(Ray& ray); // Axis-aligned bounding box intersection test
};

struct Surface {
    std::vector<Vector3f> vertices, normals;
    std::vector<Vector3i> indices;
    std::vector<Vector2f> uvs;

    bool isLight;
    uint32_t shapeIdx;

    Vector3f diffuse;
    float alpha;

    Texture diffuseTexture, alphaTexture;

    Interaction rayPlaneIntersect(Ray ray, Vector3f p, Vector3f n);
    Interaction rayTriangleIntersect(Ray ray, Vector3f v1, Vector3f v2, Vector3f v3, Vector3f n);
    Interaction rayIntersect(Ray ray);

    bool slab_test(Ray& ray); // Axis-aligned bounding box intersection test

    Vector3f aabb[2]; // Axis-aligned bounding box (min, max)
    
    bool isNull;

    BVH_Triangles bvh;

    void PopulateBVH(BVH_Triangles* bvh);
    void PrintBVH(BVH_Triangles* bvh, int lvl);
    BVH_Triangles* Traverse_BVH(Ray& ray);
    void UpdateAABB(BVH_Triangles* bvh);


private:
    bool hasDiffuseTexture();
    bool hasAlphaTexture();
};

std::vector<Surface> createSurfaces(std::string pathToObj, bool isLight, uint32_t shapeIdx);

// structure for BVH
struct BVH_object {
    BVH_object* left;
    BVH_object* right;
    Surface** surfaces; // array of pointer to surfaces in AABB
    long int Num_Of_Surfaces;
    Vector3f aabb[2]; // Axis-aligned bounding box (min, max)

    bool slab_test(Ray& ray); // Axis-aligned bounding box intersection test
};

