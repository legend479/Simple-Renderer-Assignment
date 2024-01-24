#include "render.h"

int intersection_type;
Integrator::Integrator(Scene &scene)
{
    this->scene = scene;
    this->outputImage.allocate(TextureType::UNSIGNED_INTEGER_ALPHA, this->scene.imageResolution);
}

long long Integrator::render()
{
    auto startTime = std::chrono::high_resolution_clock::now();
    for (int x = 0; x < this->scene.imageResolution.x; x++) {
        for (int y = 0; y < this->scene.imageResolution.y; y++) {
            Ray cameraRay = this->scene.camera.generateRay(x, y);
            // std::cout << "cordinate :" << x << y  << "Camera ray: " << cameraRay.d.x << " " << cameraRay.d.y << " " << cameraRay.d.z << std::endl;
            Interaction si = this->scene.rayIntersect(cameraRay);

            if (si.didIntersect)
                this->outputImage.writePixelColor(0.5f * (si.n + Vector3f(1.f, 1.f, 1.f)), x, y);
            else
                this->outputImage.writePixelColor(Vector3f(0.0f, 0.0f, 0.0f), x, y);
        }
    }
    auto finishTime = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::microseconds>(finishTime - startTime).count();
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::cerr << "Usage: ./render <scene_config> <out_path> <intersection_variant>\n";
        return 1;
    }
    Scene scene(argv[1]);

    intersection_type = std::stoi(argv[3]);
    switch (intersection_type)
    {
    case 0:
        printf("Intersection type: NAIVE\n");
        break;
    case 1:
        printf("Intersection type: AABB\n");
        break;
    case 2:
        printf("Intersection type: BVH\n");
        break;    
    case 3:
        printf("Intersection type: two level BVH\n");
        break;
    default:
        break;
    }


    Integrator rayTracer(scene);
    auto renderTime = rayTracer.render();

    std::cout << "Render Time: " << std::to_string(renderTime / 1000.f) << " ms" << std::endl;
    rayTracer.outputImage.save(argv[2]);

    return 0;
}
