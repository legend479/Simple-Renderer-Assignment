#include "camera.h"

Camera::Camera(Vector3f from, Vector3f to, Vector3f up, float fieldOfView, Vector2i imageResolution)
    : from(from),
    to(to),
    up(up),
    fieldOfView(fieldOfView),
    imageResolution(imageResolution)
{
    this->aspect = imageResolution.x / float(imageResolution.y);

    // Determine viewport dimensions in 3D
    float fovRadians = fieldOfView * M_PI / 180.f;
    float h = std::tan(fovRadians / 2.f);
    float viewportHeight = 2.f * h * this->focusDistance;
    float viewportWidth = viewportHeight * this->aspect;

    // Calculate basis vectors of the camera for the given transform
    this->w = Normalize(this->from - this->to);
    this->u = Normalize(Cross(up, this->w));
    this->v = Normalize(Cross(this->w, this->u));

    // Pixel delta vectors
    Vector3f viewportU = viewportWidth * this->u;
    Vector3f viewportV = viewportHeight * (-this->v);

    this->pixelDeltaU = viewportU / float(imageResolution.x);
    this->pixelDeltaV = viewportV / float(imageResolution.y);

    // Upper left
    this->upperLeft = from - this->w * this->focusDistance - viewportU / 2.f - viewportV / 2.f;
}

Ray Camera::generateRay(int x, int y)
{
    // Vector3f pixelCenter = this->upperLeft + 0.5f * (this->pixelDeltaU + this->pixelDeltaV);
    
    Vector3f pixelCenter = (x + 0.5f) * this->pixelDeltaU + (y + 0.5f) * this->pixelDeltaV;
    Vector3f standard = (((this->upperLeft)/PRECISION) - ((this->from)/PRECISION)) * PRECISION;
    // std:: cout << "standard: " << standard.x << " " << standard.y << " " << standard.z << std::endl;
    
    Vector3f direction = Normalize(pixelCenter + standard);

    // std::cout << "direction: " << direction.x << " " << direction.y << " " << direction.z << std::endl;


    return Ray(this->from, direction);
}