#include "Image.h"
#include "SDL_surface.h"
#include <SDL_image.h>
#include <unordered_set>
#include <unordered_map>
#include <cstring>

namespace TDT4102 {
/// A caching mechanism to avoid processing images loaded from files
    struct ImageCache {
        /// Two-way lookup
        std::unordered_map<std::filesystem::path, std::pair<SDL_Surface *, SDL_Texture *>> pathToTexture;
        std::unordered_set<SDL_Texture *> textureMembershipSet;
        std::unordered_set<SDL_Surface *> surfaceMembershipSet;

        /// Loads an SDL_Texture from the file path and caches it
        SDL_Texture *insert(SDL_Renderer *renderer, SDL_Surface *surface, const std::filesystem::path &filePath);

        SDL_Texture *getTexture(const std::filesystem::path &path) const;
        SDL_Surface *getSurface(const std::filesystem::path &path) const;

        bool isTextureCached(SDL_Texture *texture) const;
        bool isSurfaceCached(SDL_Surface *texture) const;

        ~ImageCache();

    private:
        ImageCache() = default;
        friend class Image;
    };
}

constexpr uint32_t GetPixelFormat() {
    return SDL_PIXELFORMAT_ARGB8888;
}

TDT4102::Image::Image() {}

TDT4102::Image::Image(const std::filesystem::path pathToImageFile) {
    path = pathToImageFile;

    SDL_Surface *cachedSurface = cache.getSurface(pathToImageFile);

    if ( cachedSurface == nullptr ) {

        if ( ! std::filesystem::exists(path) ) {
            throw std::runtime_error("The image file located at " + path.string() + "\ncould not be found");
        }

        surface = IMG_Load(pathToImageFile.string().c_str());
        surface = SDL_ConvertSurfaceFormat(surface, GetPixelFormat(), 0);
        width = surface->w;
        height = surface->h;
    } else {
        surface = cachedSurface;
        width = surface->w;
        height = surface->h;
    }

    state = ImageState::FromFile;
}

void TDT4102::Image::load(SDL_Renderer* renderer) {
    // We need a reference to SDL_Renderer to perform the loading operation
    // This requires loading it once it gets used
    if ( state == ImageState::FromFile ) {
        texture = cache.insert(renderer, surface, path);
        state = texture ? ImageState::Loaded : ImageState::Invalidated;
    } else if ( state == ImageState::FromBuffer ) {
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        state = texture ? ImageState::Loaded : ImageState::Invalidated;
    }
}

TDT4102::Image TDT4102::Image::fromPixels(const std::vector<TDT4102::Color> &pixels, int imageWidth, int imageHeight)
{
    TDT4102::Image result = TDT4102::Image{};
    result.setPixels(pixels, imageWidth, imageHeight);
    result.state = ImageState::FromBuffer;
    return result;
}

std::vector<TDT4102::Color> TDT4102::Image::getPixels() {

    std::vector<TDT4102::Color> result;
    result.resize(width * height);

    // Convert the pixel format before copying
    if ( surface->format->format != GetPixelFormat()) {
        SDL_Surface *oldSurface = surface;
        SDL_Surface *newSurface = SDL_ConvertSurfaceFormat(oldSurface, GetPixelFormat(), 0);

        if ( newSurface != nullptr ) {
            SDL_FreeSurface(oldSurface);
            surface = newSurface;
        }
    }

    // Early return if we're left with no surface to pull pixels from
    if ( surface == nullptr ) {
        return result;
    }

    // Respect pitch
    unsigned char *pixels = static_cast<unsigned char *>(surface->pixels);
    int pitch = surface->pitch;

    for ( int y = 0; y < height; y++ ) {
        std::memcpy(&result[y*width], &pixels[y*pitch], sizeof(TDT4102::Color)*width);
    }
    return result;
}

void TDT4102::Image::setPixels(const std::vector<TDT4102::Color> &pixels, int width_, int height_) {

    width = width_;
    height = height_;

    cleanupTexture();
    cleanupSurface();

    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, GetPixelFormat());

    if ( surface == nullptr ) {
        throw std::runtime_error{"Failed to create SDL Surface when setting pixels"};
        state = ImageState::Invalidated;
        return;
    }

    unsigned char *pixelData = static_cast<unsigned char *>(surface->pixels);
    int pitch = surface->pitch;

    for ( int y = 0; y < height; y++ ) {
        std::memcpy(
            &pixelData[y*pitch],
            &pixels[y*width],
            width * sizeof(TDT4102::Color)
        );
    }

    // Invalidate the image. Reload texture
    state = ImageState::FromBuffer;
}

void TDT4102::Image::draw(SDL_Renderer *renderer, TDT4102::Point location, int imageWidth, int imageHeight, double angleDegrees, TDT4102::Point rotationOrigin) {
    // We need a reference to SDL_Renderer to perform the loading operation
    // This requires loading it once it gets used
    if ( texture == nullptr ) {
        load(renderer);
    }

    SDL_Rect imageBounds {location.x, location.y, width, height};

    if(imageWidth > 0 && imageHeight > 0) {
        imageBounds.w = imageWidth;
        imageBounds.h = imageHeight;
    }

    SDL_Point rotationOrigin_sdl = {rotationOrigin.x, rotationOrigin.y};

    SDL_RenderCopyEx(renderer, texture, nullptr, &imageBounds, angleDegrees, &rotationOrigin_sdl, SDL_FLIP_NONE);
}

void TDT4102::Image::drawRegion(SDL_Renderer* renderer, TDT4102::Point location, int imageWidth, int imageHeight, TDT4102::Point sourceTopLeftPoint,  int sourceWidth, int sourceHeight, SDL_RendererFlip flip) {
    if(texture == nullptr) {
        load(renderer);
    }

    SDL_Rect sourceBounds {sourceTopLeftPoint.x, sourceTopLeftPoint.y, width, height};

    if(sourceWidth > 0 && sourceHeight > 0) {
        sourceBounds.w = sourceWidth;
        sourceBounds.h = sourceHeight;
    }

    SDL_Rect imageBounds {location.x, location.y, width, height};

    if(imageWidth > 0 && imageHeight > 0) {
        imageBounds.w = imageWidth;
        imageBounds.h = imageHeight;
    }

    SDL_RenderCopyEx(renderer, texture, &sourceBounds, &imageBounds, 0.0, nullptr, flip);
}


TDT4102::Image::~Image() {
    cleanupTexture();
    cleanupSurface();
}

void TDT4102::Image::cleanupTexture() {
    if (texture != nullptr && ! cache.isTextureCached(texture)) {
        SDL_DestroyTexture(texture);
    }
    texture = nullptr;
}

void TDT4102::Image::cleanupSurface() {
    if ( surface != nullptr && !cache.isSurfaceCached(surface)) {
        SDL_FreeSurface(surface);
    }
    surface = nullptr;
}

void TDT4102::Image::reset() {
    state = ImageState::Invalidated;
    texture = nullptr;
    surface = nullptr;
}

TDT4102::ImageCache TDT4102::Image::cache = TDT4102::ImageCache{};

SDL_Texture *TDT4102::ImageCache::insert(SDL_Renderer *renderer, SDL_Surface *surface, const std::filesystem::path &filePath) {
    // TODO: Normalize cache key
    if ( pathToTexture.contains(filePath) ) {
        return pathToTexture[filePath].second;
    } else {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        pathToTexture.insert(std::make_pair(filePath, std::make_pair(surface, texture)));
        textureMembershipSet.insert(texture);
        surfaceMembershipSet.insert(surface);
        return texture;
    }
}

bool TDT4102::ImageCache::isTextureCached(SDL_Texture *texture) const {
    return textureMembershipSet.contains(texture);
}

bool TDT4102::ImageCache::isSurfaceCached(SDL_Surface *surface) const {
    return surfaceMembershipSet.contains(surface);
}


SDL_Surface *TDT4102::ImageCache::getSurface(const std::filesystem::path &path) const {
    if ( pathToTexture.contains(path) ) {
        return pathToTexture.at(path).first;
    }
    return nullptr;
}

SDL_Texture *TDT4102::ImageCache::getTexture(const std::filesystem::path &path) const {
    if ( pathToTexture.contains(path) ) {
        return pathToTexture.at(path).second;
    }
    return nullptr;
}

TDT4102::ImageCache::~ImageCache() {
    for ( auto &[key, value] : pathToTexture ) {
        SDL_FreeSurface(value.first);
        SDL_DestroyTexture(value.second);
    }
}
