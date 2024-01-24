#include "scene.h"

Scene::Scene(std::string sceneDirectory, std::string sceneJson)
{
    nlohmann::json sceneConfig;
    try
    {
        sceneConfig = nlohmann::json::parse(sceneJson);
    }
    catch (std::runtime_error e)
    {
        std::cerr << "Could not parse json." << std::endl;
        exit(1);
    }

    this->parse(sceneDirectory, sceneConfig);
}

Scene::Scene(std::string pathToJson)
{
    std::string sceneDirectory;

#ifdef _WIN32
    const size_t last_slash_idx = pathToJson.rfind('\\');
#else
    const size_t last_slash_idx = pathToJson.rfind('/');
#endif

    if (std::string::npos != last_slash_idx)
    {
        sceneDirectory = pathToJson.substr(0, last_slash_idx);
    }

    nlohmann::json sceneConfig;
    try
    {
        std::ifstream sceneStream(pathToJson.c_str());
        sceneStream >> sceneConfig;
    }
    catch (std::runtime_error e)
    {
        std::cerr << "Could not load scene .json file." << std::endl;
        exit(1);
    }

    this->parse(sceneDirectory, sceneConfig);
}

void Scene::parse(std::string sceneDirectory, nlohmann::json sceneConfig)
{
    // Output
    try
    {
        auto res = sceneConfig["output"]["resolution"];
        this->imageResolution = Vector2i(res[0], res[1]);
    }
    catch (nlohmann::json::exception e)
    {
        std::cerr << "\"output\" field with resolution, filename & spp should be defined in the scene file." << std::endl;
        exit(1);
    }

    // Cameras
    try
    {
        auto cam = sceneConfig["camera"];

        this->camera = Camera(
            Vector3f(cam["from"][0], cam["from"][1], cam["from"][2]),
            Vector3f(cam["to"][0], cam["to"][1], cam["to"][2]),
            Vector3f(cam["up"][0], cam["up"][1], cam["up"][2]),
            float(cam["fieldOfView"]),
            this->imageResolution);
    }
    catch (nlohmann::json::exception e)
    {
        std::cerr << "No camera(s) defined. Atleast one camera should be defined." << std::endl;
        exit(1);
    }

    // Surface
    try
    {
        auto surfacePaths = sceneConfig["surface"];

        uint32_t surfaceIdx = 0;
        for (std::string surfacePath : surfacePaths)
        {
            surfacePath = sceneDirectory + "/" + surfacePath;

            auto surf = createSurfaces(surfacePath, /*isLight=*/false, /*idx=*/surfaceIdx);
            this->surfaces.insert(this->surfaces.end(), surf.begin(), surf.end());

            surfaceIdx = surfaceIdx + surf.size();
        }
        // Populate BVH
        
        // initialize the BVH
        this->bvh.Num_Of_Surfaces = this->surfaces.size();
        this->bvh.surfaces = new Surface *[this->bvh.Num_Of_Surfaces];
        Vector3f min = Vector3f(1e30, 1e30, 1e30);
        Vector3f max = Vector3f(-1e30, -1e30, -1e30);
        for (int i = 0; i < this->bvh.Num_Of_Surfaces; ++i)
        {
            this->bvh.surfaces[i] = &this->surfaces[i];
            for (int j = 0; j < 3; ++j)
            {
                if (this->surfaces[i].aabb[0][j] < min[j])
                {
                    min[j] = this->surfaces[i].aabb[0][j];
                }
                if (this->surfaces[i].aabb[1][j] > max[j])
                {
                    max[j] = this->surfaces[i].aabb[1][j];
                }
            }
        }
        this->bvh.aabb[0] = min;
        this->bvh.aabb[1] = max;
        this->PopulateBVH(&this->bvh);
        // this->PrintBVH(&this->bvh, 0);
    }
    catch (nlohmann::json::exception e)
    {
        std::cout << "No surfaces defined." << std::endl;
    }
}

Interaction Scene::rayIntersect(Ray &ray)   
{
    Interaction siFinal;
    // std:: cout << intersection_type << std::endl;
    switch (intersection_type)
    {
        case 0:
        {
            // printf("NAIVE\n");

            for (auto &surface : this->surfaces)
            {
                Interaction si = surface.rayIntersect(ray);
                if (si.t <= ray.t)
                {
                    siFinal = si;
                    ray.t = si.t;
                }
            }

            return siFinal;
        }
        case 1:
        {
            // printf("AABB\n");
            for (auto &surface : this->surfaces)
            {
                if (surface.slab_test(ray))
                {
                    Interaction si = surface.rayIntersect(ray);
                    if (si.t <= ray.t)
                    {
                        siFinal = si;
                        ray.t = si.t;
                    }
                }
            }

            return siFinal;
        }
        case 2:
        case 3:
        {
            // printf("BVH\n");
            // Traverse the BVH to find the Intersecting surfaces
            BVH_object *interset_obj = this->Traverse_BVH(ray);

            // if(interset_obj->Num_Of_Surfaces == 2)
            // {
            //     printf("useless\n");
            // }

            // Intersect the ray with the intersecting surfaces with slab test
            for (int i = 0; i < interset_obj->Num_Of_Surfaces; ++i)
            {
                // printf("Surface %d\n", interset_obj->surfaces[i]->shapeIdx);
                if (interset_obj->surfaces[i]->slab_test(ray))
                {
                    Interaction si = interset_obj->surfaces[i]->rayIntersect(ray);
                    if (si.t <= ray.t)
                    {
                        siFinal = si;
                        ray.t = si.t;
                    }
                }
            }

            return siFinal;
        }
        default:
        {
            printf("Invalid intersection type detected\n");
            exit(1);
        }
    }
}

void Scene::PrintBVH(BVH_object *curNode, int lvl)
{
    for (int i = 0; i < lvl; ++i)
    {
        printf("  ");
    }
    for (int i = 0; i < curNode->Num_Of_Surfaces; ++i)
    {
        printf("%d ", curNode->surfaces[i]->shapeIdx);
    }
    printf("\n");
    if (curNode->left != NULL)
    {
        this->PrintBVH(curNode->left, lvl + 1);
    }
    if (curNode->right != NULL)
    {
        this->PrintBVH(curNode->right, lvl + 1);
    }
}
void Scene::PopulateBVH(BVH_object *curNode)
{
    // use Median Split to build the BVH
    // 1. find the longest axis of the bounding box
    // 2. sort the surfaces based on the longest axis
    // 3. split the surfaces into two groups
    // 4. repeat the process for each group until all surface reside in a leaf node

    // check if the current node is a leaf node
    // printf("%ld ", curNode->Num_Of_Surfaces);
    // printf("AABB: ");
    // printf("min: ");
    // for(int i = 0; i < 3; ++i)
    // {
    //     printf("%f ", curNode->aabb[0][i]);
    // }
    // printf("max: ");
    // for(int i = 0; i < 3; ++i)
    // {
    //     printf("%f ", curNode->aabb[1][i]);
    // }
    // printf("\n");
    if (curNode->Num_Of_Surfaces <= 1)
    {
        // printf("Leaf Node %d\n", curNode->surfaces[0]->shapeIdx);
        return;
    }

    // find the longest axis of the bounding box
    Vector3f box_size = curNode->aabb[1] - curNode->aabb[0];
    int longest_axis = 0;
    if (box_size[1] > box_size[0])
    {
        longest_axis = 1;
    }
    if (box_size[2] > box_size[longest_axis])
    {
        longest_axis = 2;
    }

    // sort the surfaces based on the longest axis
    std::sort(curNode->surfaces, curNode->surfaces + curNode->Num_Of_Surfaces, [longest_axis](Surface *a, Surface *b) -> bool
              { return a->aabb[0][longest_axis] < b->aabb[0][longest_axis]; });

    // split the surfaces into two groups
    BVH_object *left_node = new BVH_object();
    BVH_object *right_node = new BVH_object();
    left_node->Num_Of_Surfaces = curNode->Num_Of_Surfaces / 2;
    right_node->Num_Of_Surfaces = curNode->Num_Of_Surfaces - left_node->Num_Of_Surfaces;
    left_node->surfaces = new Surface *[left_node->Num_Of_Surfaces];
    right_node->surfaces = new Surface *[right_node->Num_Of_Surfaces];

    // update the bounding box of the left and right node
    left_node->aabb[0] = Vector3f(1e30, 1e30, 1e30);
    left_node->aabb[1] = Vector3f(-1e30, -1e30, -1e30);

    right_node->aabb[0] = Vector3f(1e30, 1e30, 1e30);
    right_node->aabb[1] = Vector3f(-1e30, -1e30, -1e30);

    // update the surfaces and bounding boxes of the left and right node
    for (int i = 0; i < left_node->Num_Of_Surfaces; ++i)
    {
        left_node->surfaces[i] = curNode->surfaces[i];
        for (int j = 0; j < 3; ++j)
        {
            left_node->aabb[0][j] = std::min(left_node->surfaces[i]->aabb[0][j], left_node->aabb[0][j]);
            left_node->aabb[1][j] = std::max(left_node->surfaces[i]->aabb[1][j], left_node->aabb[1][j]);
        }
    }
    for (int i = 0; i < right_node->Num_Of_Surfaces; ++i)
    {
        right_node->surfaces[i] = curNode->surfaces[i + left_node->Num_Of_Surfaces];
        for (int j = 0; j < 3; ++j)
        {
            right_node->aabb[0][j] = std::min(right_node->surfaces[i]->aabb[0][j], right_node->aabb[0][j]);
            right_node->aabb[1][j] = std::max(right_node->surfaces[i]->aabb[1][j], right_node->aabb[1][j]);
        }
    }

    // left_node->aabb[1][longest_axis] = (curNode->surfaces[left_node->Num_Of_Surfaces - 1])->aabb[0][longest_axis];
    // right_node->aabb[0][longest_axis] = (curNode->surfaces[left_node->Num_Of_Surfaces])->aabb[0][longest_axis];

    // update the current node
    curNode->left = left_node;
    curNode->right = right_node;

    // recursively build the BVH
    this->PopulateBVH(left_node);
    this->PopulateBVH(right_node);

    return;
}

BVH_object *Scene::Traverse_BVH(Ray& ray)
{
    // Traverse the BVH to find the Intersecting surface
    BVH_object *current_node = &this->bvh;

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