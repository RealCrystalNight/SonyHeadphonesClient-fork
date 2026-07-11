#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include "ImageLoader.hpp"

DeviceImage LoadPNG(SDL_Renderer* renderer, const std::string& path)
{
    DeviceImage result;
    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &result.width, &result.height, &channels, 4);
    if (!data)
        return result;

    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        result.width, result.height,
        SDL_PIXELFORMAT_RGBA32,
        data, result.width * 4
    );
    if (!surface)
    {
        stbi_image_free(data);
        return result;
    }

    result.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    stbi_image_free(data);
    return result;
}
