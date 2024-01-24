#include "surface.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

std::vector<Surface> createSurfaces(std::string pathToObj, bool isLight, uint32_t shapeIdx)
{
    std::string objDirectory;
    const size_t last_slash_idx = pathToObj.rfind('/');
    if (std::string::npos != last_slash_idx)
    {
        objDirectory = pathToObj.substr(0, last_slash_idx);
    }

    std::vector<Surface> surfaces;

    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig reader_config;
    if (!reader.ParseFromFile(pathToObj, reader_config))
    {
        if (!reader.Error().empty())
        {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }

    if (!reader.Warning().empty())
    {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &materials = reader.GetMaterials();

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++)
    {
        Surface surf;
        surf.isLight = isLight;
        surf.shapeIdx = shapeIdx;
        std::set<int> materialIds;

        surf.aabb[0] = Vector3f(1e30, 1e30, 1e30);
        surf.aabb[1] = Vector3f(-1e30, -1e30, -1e30);

        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
            if (fv > 3)
            {
                std::cerr << "Not a triangle mesh" << std::endl;
                exit(1);
            }

            // Loop over vertices in the face. Assume 3 vertices per-face
            Vector3f vertices[3], normals[3];
            Vector2f uvs[3];
            for (size_t v = 0; v < fv; v++)
            {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                // Check if `normal_index` is zero or positive. negative = no normal data
                if (idx.normal_index >= 0)
                {
                    tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                    normals[v] = Vector3f(nx, ny, nz);
                }

                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                if (idx.texcoord_index >= 0)
                {
                    tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

                    uvs[v] = Vector2f(tx, ty);
                }

                vertices[v] = Vector3f(vx, vy, vz);

                // Update AABB for entire surface
                for (int i = 0; i < 3; ++i)
                {
                    surf.aabb[0][i] = vertices[v][i] < surf.aabb[0][i] ? vertices[v][i] : surf.aabb[0][i];
                    surf.aabb[1][i] = vertices[v][i] > surf.aabb[1][i] ? vertices[v][i] : surf.aabb[1][i];
                }

            }

            int vSize = surf.vertices.size();
            Vector3i findex(vSize, vSize + 1, vSize + 2);

            surf.vertices.push_back(vertices[0]);
            surf.vertices.push_back(vertices[1]);
            surf.vertices.push_back(vertices[2]);

            surf.normals.push_back(normals[0]);
            surf.normals.push_back(normals[1]);
            surf.normals.push_back(normals[2]);

            surf.uvs.push_back(uvs[0]);
            surf.uvs.push_back(uvs[1]);
            surf.uvs.push_back(uvs[2]);

            surf.indices.push_back(findex);

            // per-face material
            materialIds.insert(shapes[s].mesh.material_ids[f]);

            index_offset += fv;
        }

        if (materialIds.size() > 1)
        {
            std::cerr << "One of the meshes has more than one material. This is not allowed." << std::endl;
            exit(1);
        }

        if (materialIds.size() == 0)
        {
            std::cerr << "One of the meshes has no material definition, may cause unexpected behaviour." << std::endl;
        }
        else
        {
            // Load textures from Materials
            auto matId = *materialIds.begin();
            if (matId != -1)
            {
                auto mat = materials[matId];

                surf.diffuse = Vector3f(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
                if (mat.diffuse_texname != "")
                    surf.diffuseTexture = Texture(objDirectory + "/" + mat.diffuse_texname);

                surf.alpha = mat.specular[0];
                if (mat.alpha_texname != "")
                    surf.alphaTexture = Texture(objDirectory + "/" + mat.alpha_texname);
            }
        }

        // Populate BVH
        surf.bvh.Num_Of_Triangles = surf.indices.size();
        surf.bvh.vertices.resize(surf.bvh.Num_Of_Triangles * 3);
        surf.bvh.normals.resize(surf.bvh.Num_Of_Triangles * 3);

        Vector3f max = Vector3f(-1e30, -1e30, -1e30);
        Vector3f min = Vector3f(1e30, 1e30, 1e30);
        for (int i = 0; i < surf.bvh.Num_Of_Triangles; ++i)
        {
            surf.bvh.vertices[i * 3] = surf.vertices[i * 3];
            surf.bvh.vertices[i * 3 + 1] = surf.vertices[i * 3 + 1];
            surf.bvh.vertices[i * 3 + 2] = surf.vertices[i * 3 + 2];

            for (int j = 0; j < 3; ++j)
            {
                max[j] = surf.bvh.vertices[i * 3 + j][j] > max[j] ? surf.bvh.vertices[i * 3 + j][j] : max[j];
                min[j] = surf.bvh.vertices[i * 3 + j][j] < min[j] ? surf.bvh.vertices[i * 3 + j][j] : min[j];
            }

            surf.bvh.normals[i * 3] = surf.normals[i * 3];
            surf.bvh.normals[i * 3 + 1] = surf.normals[i * 3 + 1];
            surf.bvh.normals[i * 3 + 2] = surf.normals[i * 3 + 2];
        }

        surf.bvh.aabb[0] = min;
        surf.bvh.aabb[1] = max;

        surf.bvh.left = NULL;
        surf.bvh.right = NULL;

        surf.PopulateBVH(&surf.bvh);
        // surf.PrintBVH(&surf.bvh, 0);
        // surf.UpdateAABB(&surf.bvh);

        surfaces.push_back(surf);
        shapeIdx++;
    }

    return surfaces;
}

bool Surface::hasDiffuseTexture() { return this->diffuseTexture.data != 0; }

bool Surface::hasAlphaTexture() { return this->alphaTexture.data != 0; }

Interaction Surface::rayPlaneIntersect(Ray ray, Vector3f p, Vector3f n)
{
    Interaction si;

    float dDotN = Dot(ray.d, n);
    if (dDotN != 0.f)
    {
        float t = -Dot((ray.o - p), n) / dDotN;

        if (t >= 0.f)
        {
            si.didIntersect = true;
            si.t = t;
            si.n = n;
            si.p = ray.o + ray.d * si.t;
        }
    }

    return si;
}

Interaction Surface::rayTriangleIntersect(Ray ray, Vector3f v1, Vector3f v2, Vector3f v3, Vector3f n)
{
    Interaction si = this->rayPlaneIntersect(ray, v1, n);

    if (si.didIntersect)
    {
        bool edge1 = false, edge2 = false, edge3 = false;

        // Check edge 1
        {
            Vector3f nIp = Cross((si.p - v1), (v3 - v1));
            Vector3f nTri = Cross((v2 - v1), (v3 - v1));
            edge1 = Dot(nIp, nTri) > 0;
        }

        // Check edge 2
        {
            Vector3f nIp = Cross((si.p - v1), (v2 - v1));
            Vector3f nTri = Cross((v3 - v1), (v2 - v1));
            edge2 = Dot(nIp, nTri) > 0;
        }

        // Check edge 3
        {
            Vector3f nIp = Cross((si.p - v2), (v3 - v2));
            Vector3f nTri = Cross((v1 - v2), (v3 - v2));
            edge3 = Dot(nIp, nTri) > 0;
        }

        if (edge1 && edge2 && edge3)
        {
            // Intersected triangle!
            si.didIntersect = true;
        }
        else
        {
            si.didIntersect = false;
        }
    }

    return si;
}

Interaction Surface::rayIntersect(Ray ray)
{
    if (intersection_type < 3)
    {
        Interaction siFinal;
        float tmin = ray.t;

        for (auto face : this->indices)
        {
            Vector3f p1 = this->vertices[face.x];
            Vector3f p2 = this->vertices[face.y];
            Vector3f p3 = this->vertices[face.z];

            Vector3f n1 = this->normals[face.x];
            Vector3f n2 = this->normals[face.y];
            Vector3f n3 = this->normals[face.z];
            Vector3f n = Normalize(n1 + n2 + n3);

            Interaction si = this->rayTriangleIntersect(ray, p1, p2, p3, n);
            if (si.t <= tmin && si.didIntersect)
            {
                siFinal = si;
                tmin = si.t;
            }
        }

        return siFinal;
    }
    else if (intersection_type == 3)
    {
        // BVH for triangles
        Interaction siFinal;
        float tmin = ray.t;
        BVH_Triangles *bvh = this->Traverse_BVH(ray);
        // printf("hello\n");
        if (bvh == NULL)
        {
            // printf("No intersection with BVH\n");
            siFinal.didIntersect = false;
            return siFinal;
        }
        // printf("Intersecting with BVH\n");
        // printf("Number of triangles in BVH: %ld\n", bvh->Num_Of_Triangles);
        // NAIVE intersection check for  all triangles in BVH

        for (int i = 0; i < bvh->Num_Of_Triangles; ++i)
        {
            // slab test
            Vector3f p1 = bvh->vertices[i * 3];
            Vector3f p2 = bvh->vertices[i * 3 + 1];
            Vector3f p3 = bvh->vertices[i * 3 + 2];

            // slab test
            {
                Vector3f AaBb[2] = {Vector3f(1e30, 1e30, 1e30), Vector3f(-1e30, -1e30, -1e30)};

                for(int j = 0; j < 3; ++j)
                {
                    AaBb[0][j] = std::min(AaBb[0][j], p1[j]);
                    AaBb[0][j] = std::min(AaBb[0][j], p2[j]);
                    AaBb[0][j] = std::min(AaBb[0][j], p3[j]);

                    AaBb[1][j] = std::max(AaBb[1][j], p1[j]);
                    AaBb[1][j] = std::max(AaBb[1][j], p2[j]);
                    AaBb[1][j] = std::max(AaBb[1][j], p3[j]);
                }

                float tmin = -1e30, tmax = 1e30;
                for (int i = 0; i < 3; ++i)
                {
                    float t1 = (AaBb[0][i] - ray.o[i]) / ray.d[i];
                    float t2 = (AaBb[1][i] - ray.o[i]) / ray.d[i];
                    tmin = std::max(tmin, std::min(t1, t2));
                    tmax = std::min(tmax, std::max(t1, t2));
                }
                if(tmax < tmin)
                {
                    // printf("Not intersecting with triangle\n");
                    continue;
                }
            }

            Vector3f n1 = bvh->normals[i * 3];
            Vector3f n2 = bvh->normals[i * 3 + 1];
            Vector3f n3 = bvh->normals[i * 3 + 2];
            Vector3f n = Normalize(n1 + n2 + n3);

            Interaction si = this->rayTriangleIntersect(ray, p1, p2, p3, n);
            if (si.t <= tmin && si.didIntersect)
            {
                siFinal = si;
                tmin = si.t;
            }
        }

        return siFinal;
    }
    else
    {
        std::cerr << "Invalid intersection type detected\n";
        exit(1);
    }
}

bool Surface::slab_test(Ray &ray)
{
    float tmin = -1e30, tmax = 1e30;
    for (int i = 0; i < 3; ++i)
    {
        float t1 = (this->aabb[0][i] - ray.o[i]) / ray.d[i];
        float t2 = (this->aabb[1][i] - ray.o[i]) / ray.d[i];
        tmin = std::max(tmin, std::min(t1, t2));
        tmax = std::min(tmax, std::max(t1, t2));
    }
    return tmax >= tmin;
}

// #define MARGIN -1
bool BVH_object::slab_test(Ray &ray)
{
    float tmin = -1e30, tmax = 1e30;
    for (int i = 0; i < 3; ++i)
    {
        float t1 = (this->aabb[0][i] - ray.o[i]) / ray.d[i];
        float t2 = (this->aabb[1][i] - ray.o[i]) / ray.d[i];
        tmin = std::max(tmin, std::min(t1, t2));
        tmax = std::min(tmax, std::max(t1, t2));
    }
    // if(tmax >= tmin)
    // {
    //     // printf("tmax: %f, tmin: %f\n", tmax, tmin);
    //     // printf("tmax >= tmin\n");
    //     printf("Intersecting with BVH\n");
    // }
    // else
    // {
    //     printf("Not intersecting with BVH\n");
    //     // printf("tmax: %f, tmin: %f\n", tmax, tmin);
    //     // printf("tmax < tmin\n");
    // }

    return tmax >= tmin;
}

bool BVH_Triangles::slab_test(Ray &ray)
{
    float tmin = -1e30, tmax = 1e30;
    for (int i = 0; i < 3; ++i)
    {
        float t1 = (this->aabb[0][i] - ray.o[i]) / ray.d[i];
        float t2 = (this->aabb[1][i] - ray.o[i]) / ray.d[i];
        tmin = std::max(tmin, std::min(t1, t2));
        tmax = std::min(tmax, std::max(t1, t2));
    }
    // if(tmax >= tmin)
    // {
    //     // printf("tmax: %f, tmin: %f\n", tmax, tmin);
    //     // printf("tmax >= tmin\n");
    //     printf("Intersecting with BVH\n");
    // }
    // else
    // {
    //     printf("Not intersecting with BVH\n");
    //     // printf("tmax: %f, tmin: %f\n", tmax, tmin);
    //     // printf("tmax < tmin\n");
    // }

    return tmax >= tmin;
}

void Surface::PopulateBVH(BVH_Triangles *bvh)
{
    // printf("Populating BVH\n");
    if (bvh->Num_Of_Triangles <= 4)
    {
        return;
    }

    float factor = 1.0f / 3.0f;
    std::vector<std::tuple<Vector3f, Vector3f, int>> vertices_normals_indices;
    vertices_normals_indices.reserve(bvh->Num_Of_Triangles);

    for (int i = 0; i < bvh->Num_Of_Triangles; ++i)
    {
        Vector3f v1 = bvh->vertices[i * 3];
        Vector3f v2 = bvh->vertices[i * 3 + 1];
        Vector3f v3 = bvh->vertices[i * 3 + 2];

        Vector3f n1 = bvh->normals[i * 3];
        Vector3f n2 = bvh->normals[i * 3 + 1];
        Vector3f n3 = bvh->normals[i * 3 + 2];

        vertices_normals_indices.emplace_back(factor * (v1 + v2 + v3), factor * (n1 + n2 + n3), i);
    }

    int longest_axis = 0;
    Vector3f aabb_size = bvh->aabb[1] - bvh->aabb[0];

    if (aabb_size[1] > aabb_size[0])
    {
        longest_axis = 1;
    }
    if (aabb_size[2] > aabb_size[longest_axis])
    {
        longest_axis = 2;
    }

    std::sort(vertices_normals_indices.begin(), vertices_normals_indices.end(),
              [longest_axis](const auto &a, const auto &b)
              {
                  return std::get<0>(a)[longest_axis] < std::get<0>(b)[longest_axis];
              });

    BVH_Triangles* bvh_left = new BVH_Triangles();
    BVH_Triangles* bvh_right = new BVH_Triangles();

    bvh_left->Num_Of_Triangles = bvh->Num_Of_Triangles / 2;
    bvh_right->Num_Of_Triangles = bvh->Num_Of_Triangles - bvh_left->Num_Of_Triangles;


    bvh_left->vertices.reserve(bvh_left->Num_Of_Triangles * 3);
    bvh_left->normals.reserve(bvh_left->Num_Of_Triangles * 3);

    bvh_right->vertices.reserve(bvh_right->Num_Of_Triangles * 3);
    bvh_right->normals.reserve(bvh_right->Num_Of_Triangles * 3);

    Vector3f max = Vector3f(-1e30,-1e30,-1e30);
    Vector3f min = Vector3f(1e30,1e30,1e30);

    for (int i = 0; i < bvh_left->Num_Of_Triangles; ++i)
    {
        // Copy vertices and normals
        // Update bounding box of the left node
        // max = bvh_left->vertices[i * 3];
        // min = bvh_left->vertices[i * 3];
        for (int j = 0; j < 3; ++j)
        {
            bvh_left->vertices.emplace_back(bvh->vertices[i * 3 + j]);
            bvh_left->normals.emplace_back(bvh->normals[i * 3 + j]);

            max[j] = std::max(max[j], bvh_left->vertices[i * 3 + j][j]);
            min[j] = std::min(min[j], bvh_left->vertices[i * 3 + j][j]);
        }
    }
    bvh_left->aabb[0] = min;
    bvh_left->aabb[1] = max;

    max = Vector3f(-1e30, -1e30, -1e30);
    min = Vector3f(1e30, 1e30, 1e30);

    for (int i = 0; i < bvh_right->Num_Of_Triangles; ++i)
    {    // Copy vertices and normals
        // Update bounding box of the right node
        // max = bvh_right->vertices[i * 3];
        // min = bvh_right->vertices[i * 3];
        for (int j = 0; j < 3; ++j)
        {
            bvh_right->vertices.emplace_back(bvh->vertices[(i + bvh_left->Num_Of_Triangles) * 3 + j]);
            bvh_right->normals.emplace_back(bvh->normals[(i + bvh_left->Num_Of_Triangles) * 3 + j]);

            max[j] = std::max(max[j], bvh_right->vertices[i * 3 + j][j]);
            min[j] = std::min(min[j], bvh_right->vertices[i * 3 + j][j]);
        }
    }
    bvh_right->aabb[0] = min;
    bvh_right->aabb[1] = max;

    bvh->left = bvh_left;
    bvh->right = bvh_right;

    PopulateBVH(bvh->left);
    PopulateBVH(bvh->right);
}
void Surface::PrintBVH(BVH_Triangles *bvh, int lvl)
{
    for (int i = 0; i < lvl; ++i)
    {
        printf("  ");
    }
    for (int i = 0; i < bvh->Num_Of_Triangles; ++i)
    {
        printf("%d ", i + 1);
        // printf("vertices(%f, %f, %f) ", bvh->vertices[i][0], bvh->vertices[i][1], bvh->vertices[i][2]);
    }
    printf("\n");
    if (bvh->left != NULL)
    {
        this->PrintBVH(bvh->left, lvl + 1);
    }
    if (bvh->right != NULL)
    {
        this->PrintBVH(bvh->right, lvl + 1);
    }
}
BVH_Triangles *Surface::Traverse_BVH(Ray &ray)
{
    BVH_Triangles *current_node = &this->bvh;
    // printf("Traversing BVH\n");

    while (current_node->left != NULL && current_node->right != NULL)
    {
        // check if the ray intersects with the left node
        if (current_node->left && !current_node->left->slab_test(ray))
        {
            current_node = current_node->right;
        }
        // check if the ray intersects with the right node
        else if (current_node->right && !current_node->right->slab_test(ray))
        {
            current_node = current_node->left;
        }
        else
        {
            return current_node;
        }
    }
    return current_node;
}

void Surface::UpdateAABB(BVH_Triangles *bvh)
{
    Vector3f max = Vector3f(-1e30, -1e30, -1e30);
    Vector3f min = Vector3f(1e30, 1e30, 1e30);
    for (int i = 0; i < bvh->Num_Of_Triangles; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            max[j] = bvh->vertices[i * 3 + j][j] > max[j] ? bvh->vertices[i * 3 + j][j] : max[j];
            min[j] = bvh->vertices[i * 3 + j][j] < min[j] ? bvh->vertices[i * 3 + j][j] : min[j];
        }
    }
    bvh->aabb[0] = min;
    bvh->aabb[1] = max;

    if (bvh->left != NULL)
    {
        this->UpdateAABB(bvh->left);
    }
    if (bvh->right != NULL)
    {
        this->UpdateAABB(bvh->right);
    }

    return;
}
