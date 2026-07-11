#pragma once
#include <SDL3/SDL.h>
#include <string>

struct DeviceImage
{
    SDL_Texture* texture{nullptr};
    int width{0};
    int height{0};

    ~DeviceImage() { destroy(); }
    DeviceImage() = default;
    DeviceImage(const DeviceImage&) = delete;
    DeviceImage& operator=(const DeviceImage&) = delete;
    DeviceImage(DeviceImage&& other) noexcept
        : texture(other.texture), width(other.width), height(other.height)
    {
        other.texture = nullptr;
    }
    DeviceImage& operator=(DeviceImage&& other) noexcept
    {
        if (this != &other)
        {
            destroy();
            texture = other.texture;
            width = other.width;
            height = other.height;
            other.texture = nullptr;
        }
        return *this;
    }
    explicit operator bool() const { return texture != nullptr; }

    void destroy()
    {
        if (texture)
            SDL_DestroyTexture(texture);
        texture = nullptr;
        width = height = 0;
    }
};

DeviceImage LoadPNG(SDL_Renderer* renderer, const std::string& path);
