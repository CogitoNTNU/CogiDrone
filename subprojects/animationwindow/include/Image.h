#pragma once

#include <vector>
#include "Point.h"
#include "Color.h"
#include <filesystem>
#include "SDL_render.h"

namespace TDT4102 {

    struct ImageCache;
  
    enum class FlipImage {
        NONE, HORIZONTAL, VERTICAL
    };

    struct Image {
        // Allow the texture to be read out and drawn despite being private
        friend class AnimationWindow;
        friend class ImageCache;

    public:
        explicit Image();
        explicit Image(std::filesystem::path pathToImageFile);

        Image &operator=(Image &&other) {
            this->~Image();

            state = other.state;
            texture = other.texture;
            surface = other.surface;
            path = std::move(other.path);

            other.reset();

            return *this;
        }

        Image(Image &&other)
            : texture{other.texture}, surface{other.surface}, path{std::move(other.path)}, state{other.state}
            {
            other.reset();
        }

        ~Image();

        /// Creates an image instance from a vector of colors.
        /// \see TDT4102::Image::bytes
        static Image fromPixels(const std::vector<TDT4102::Color> &bytes,
                               int imageWidth, int imageHeight);

        static std::vector<TDT4102::Color> loadPixels(std::filesystem::path pathToImageFile);

        int width = 0;
        int height = 0;

        std::vector<TDT4102::Color> getPixels();
        void setPixels(const std::vector<TDT4102::Color> &, int width_, int height_);

    private:

        enum class ImageState {
            Loaded,
            FromFile,
            FromBuffer,
            Invalidated
        };

        SDL_Texture *texture = nullptr;

        /// Trade-off. Allows image manipulation at the cost of memory.
        SDL_Surface *surface = nullptr;

        std::filesystem::path path{"nonexistent"};

        /// The state of the image
        ImageState state = ImageState::Invalidated;

        void load(SDL_Renderer* renderer);

        void draw(SDL_Renderer* renderer, TDT4102::Point location,
                  int imageWidth = 0, int imageHeight = 0, 
                  double angleDegrees = 0, TDT4102::Point rotationOrigin = {0, 0});

        /// Does conditional texture cleanup.
        /// Unsets the associated texture pointer
        void cleanupTexture();

        /// Conditionally cleans up the SDL_Surface
        /// Unsets the associated SDL_Surface pointer
        void cleanupSurface();

        /// Resets the Image. For use with move-operators
        void reset();

        static ImageCache cache;
      
        void drawRegion(SDL_Renderer* renderer, TDT4102::Point location, 
                        int imageWidth = 0, int imageHeight = 0, 
                        TDT4102::Point sourceTopLeftPoint = {0, 0},  
                        int sourceWidth = 0, int sourceHeight = 0, SDL_RendererFlip flip = SDL_FLIP_NONE);
    };
}
