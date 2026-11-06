// --------------------------------------------------------------------------------------------------------------------
// ArDepth: ARCore's depth picture, for hiding pieces behind real things (optional module).
//
// ArDepthConfigure turns ARCore's depth on when the phone supports it. Each tracked frame ArDepthUpdate uploads the
// 160 x 90 depth picture to a GL texture (two bytes per pixel, millimetres) for lit.frag, works out how screen
// positions map into it, and keeps a CPU copy that ArDepthPicture hands to other modules (ArCover).
// --------------------------------------------------------------------------------------------------------------------

#include "AR/ArModules.hpp"

// Shared AR state this module uses (owned by ArContext, see AR/ArModules.hpp).
static ArSession*&       session             = gAr.session;
static ArFrame*&         sessionFrame        = gAr.sessionFrame;
static bool&             depthEnabled        = gAr.depthEnabled;

// Private to this module
static GLuint depthTexture = 0;                         // this frame's depth picture: millimetres, low byte in R, high byte in G
static bool depthValid = false;                         // depthTexture holds this frame's picture
static float depthUvTransform[9]{};                     // screen (0..1) -> depth picture (0..1), column-major 3x3
static std::vector<uint8_t> picture;                    // CPU copy of this frame's depth picture
static int pictureWidth = 0;
static int pictureHeight = 0;
static int pictureRowStride = 0;

void ArDepthConfigure(ArConfig* config)
{
    int32_t depthSupported = 0;
    ArSession_isDepthModeSupported(session, AR_DEPTH_MODE_AUTOMATIC, &depthSupported);
    depthEnabled = depthSupported != 0;
    ArConfig_setDepthMode(session, config, depthEnabled ? AR_DEPTH_MODE_AUTOMATIC : AR_DEPTH_MODE_DISABLED);
    LOGI("Depth: %s", depthEnabled ? "on (real things hide the pieces)" : "not supported on this phone");
}

void ArDepthInitGL()
{
    // The depth picture: NEAREST, because each pixel is two bytes of one number and must not be blended with its neighbours.
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    depthValid = false;
}

void ArDepthUpdate()
{
    depthValid = false;
    if(depthEnabled && depthTexture != 0)
    {
        ArImage* depthImage = nullptr;
        if(ArFrame_acquireDepthImage16Bits(session, sessionFrame, &depthImage) == AR_SUCCESS && depthImage != nullptr)
        {
            int32_t width = 0;
            int32_t height = 0;
            int32_t rowStride = 0;
            int32_t length = 0;
            const uint8_t* data = nullptr;
            ArImage_getWidth(session, depthImage, &width);
            ArImage_getHeight(session, depthImage, &height);
            ArImage_getPlaneRowStride(session, depthImage, 0, &rowStride);
            ArImage_getPlaneData(session, depthImage, 0, &data, &length);
            if(data != nullptr && width > 0 && height > 0 && rowStride >= width * 2)
            {
                glBindTexture(GL_TEXTURE_2D, depthTexture);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, rowStride / 2);   // in pixels: rows may be padded
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, data);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glBindTexture(GL_TEXTURE_2D, 0);

                // Screen corners -> where they fall in the depth picture. The mapping is a straight-line one, so three
                // corners give it: column 0 = along screen x, column 1 = along screen y, column 2 = the bottom-left corner.
                const float ndc[6] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f};
                float uv[6];
                ArFrame_transformCoordinates2d(session, sessionFrame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES,
                                               3, ndc, AR_COORDINATES_2D_IMAGE_NORMALIZED, uv);
                const float transform[9] = {uv[2] - uv[0], uv[3] - uv[1], 0.0f,
                                            uv[4] - uv[0], uv[5] - uv[1], 0.0f,
                                            uv[0],         uv[1],         1.0f};
                std::memcpy(depthUvTransform, transform, sizeof(transform));
                depthValid = true;

                // Keep a copy of the picture for the other modules this frame (ArCover reads it).
                picture.assign(data, data + static_cast<std::size_t>(rowStride) * static_cast<std::size_t>(height));
                pictureWidth = width;
                pictureHeight = height;
                pictureRowStride = rowStride;

                static bool loggedSize = false;
                if(!loggedSize)
                {
                    loggedSize = true;
                    LOGI("Depth picture: %d x %d", width, height);
                }
            }
            ArImage_release(depthImage);
        }
    }
}

void ArDepthInvalidate()
{
    depthValid = false;      // the last depth picture no longer lines up with the screen
}

bool ArDepthPicture(const uint8_t*& data, int& width, int& height, int& rowStride)
{
    if(!depthValid || picture.empty())
    {
        return false;
    }
    data = picture.data();
    width = pictureWidth;
    height = pictureHeight;
    rowStride = pictureRowStride;
    return true;
}

void ArDepthShutdown()
{
    depthValid = false;
    depthEnabled = false;
    picture.clear();
}


bool ArGetDepthTexture(unsigned int* texture, float* uvTransform)
{
    if(!depthValid || depthTexture == 0)
    {
        return false;
    }
    *texture = depthTexture;
    std::memcpy(uvTransform, depthUvTransform, sizeof(depthUvTransform));
    return true;
}
