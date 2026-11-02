#include "n3ds_graphics.h"

#include <cstdio>
#include <string>
#include <cstring>
#include <malloc.h>

#include "vshader_shbin.h"

C3D_RenderTarget *n3ds_graphics::topRenderTarget = nullptr;
C3D_RenderTarget *n3ds_graphics::bottomRenderTarget = nullptr;

DVLB_s *n3ds_graphics::vshader_dvlb = nullptr;
shaderProgram_s n3ds_graphics::program = {};
int8_t n3ds_graphics::uLoc_projection = 0;
int8_t n3ds_graphics::uLoc_modelView = 0;
int8_t n3ds_graphics::uLoc_uvOffset = 0;
C3D_Mtx n3ds_graphics::topProjection = {};
C3D_Mtx n3ds_graphics::bottomProjection = {};
void *n3ds_graphics::vbo_data = nullptr;
C3D_AttrInfo n3ds_graphics::attrInfo = {};
C3D_BufInfo n3ds_graphics::bufInfo = {};
C3D_Tex *n3ds_graphics::boundTexture = nullptr;

typedef struct
{
    float position[3];
    float texcoord[2];
} vertex;

static const vertex vertex_list[] =
{
    {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
    {{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}
};

#define vertex_list_count (sizeof(vertex_list) / sizeof(vertex_list[0]))

void n3ds_graphics::Initialize()
{
    // Initialize graphics

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    // Initialize the render targets

    topRenderTarget = C3D_RenderTargetCreate(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    C3D_RenderTargetSetOutput(topRenderTarget, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
    bottomRenderTarget = C3D_RenderTargetCreate(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    C3D_RenderTargetSetOutput(bottomRenderTarget, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    // Load the vertex shader, create a shader program and bind it

    vshader_dvlb = DVLB_ParseFile((uint32_t *)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
    C3D_BindProgram(&program);

    // Get the location of the uniforms

    uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
    uLoc_modelView = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
    uLoc_uvOffset = shaderInstanceGetUniformLocation(program.vertexShader, "uvOffset");

    // Configure attributes for use with the vertex shader

    // Kept in our own structs so they can be restored after citro2d draws text.
    AttrInfo_Init(&attrInfo);
    AttrInfo_AddLoader(&attrInfo, 0, GPU_FLOAT, 3); // v0=position
    AttrInfo_AddLoader(&attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord

    // Create the VBO (vertex buffer object)

    vbo_data = linearAlloc(sizeof(vertex_list));
    memcpy(vbo_data, vertex_list, sizeof(vertex_list));

    // Configure buffers

    BufInfo_Init(&bufInfo);
    BufInfo_Add(&bufInfo, vbo_data, sizeof(vertex), 2, 0x10);

    RestoreState();

    // citro2d draws the text on the bottom screen
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
}

void n3ds_graphics::RestoreState()
{
    C3D_BindProgram(&program);
    C3D_SetAttrInfo(&attrInfo);
    C3D_SetBufInfo(&bufInfo);

    // Texture stage 0 outputs the texture color; the rest pass it through
    // (citro2d reconfigures stages 0-5 for its own drawing).
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0xff00ffff);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_CONSTANT, GPU_CONSTANT);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    for (int i = 1; i < 6; i++)
        C3D_TexEnvInit(C3D_GetTexEnv(i));

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    if (boundTexture)
        C3D_TexBind(0, boundTexture);
}

void n3ds_graphics::Dispose()
{
    // Free the VBO

    C2D_Fini();

    linearFree(vbo_data);

    // Free the shader program

    shaderProgramFree(&program);
    DVLB_Free(vshader_dvlb);

    // Deinitialize graphics

    C3D_Fini();
}

void n3ds_graphics::BeginRender(bool vSync)
{
    C3D_FrameBegin(vSync ? C3D_FRAME_SYNCDRAW : 0);
}

void n3ds_graphics::DrawTopRenderTarget(uint32_t clearColor)
{
    C3D_RenderTargetClear(topRenderTarget, C3D_CLEAR_ALL, clearColor, 0);
    C3D_FrameDrawOn(topRenderTarget);
    RestoreState();
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &topProjection);
}

void n3ds_graphics::DrawBottomRenderTarget(uint32_t clearColor)
{
    C3D_RenderTargetClear(bottomRenderTarget, C3D_CLEAR_ALL, clearColor, 0);
    C3D_FrameDrawOn(bottomRenderTarget);
    RestoreState();
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &bottomProjection);
}

void n3ds_graphics::FinishRender()
{
    C3D_FrameEnd(0);
}

bool n3ds_graphics::IsMainLoopRunning()
{
    return aptMainLoop();
}

void n3ds_graphics::SetTopOrthoProjectionMatrix(float left, float right, float bottom, float top, float near, float far)
{
    Mtx_OrthoTilt(&topProjection, left, right, bottom, top, near, far, true);
}

void n3ds_graphics::SetBottomOrthoProjectionMatrix(float left, float right, float bottom, float top, float near, float far)
{
    Mtx_OrthoTilt(&bottomProjection, left, right, bottom, top, near, far, true);
}

void n3ds_graphics::SetModelViewMatrix(float x, float y, float w, float h)
{
    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x, y, 0.5f, false);
    Mtx_Scale(&modelView, w, h, 1.0f);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
}

void n3ds_graphics::DrawQuad(float x, float y, float w, float h, float uvX, float uvY, float uvW, float uvH)
{
    SetModelViewMatrix(x, y, w, h);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_uvOffset, uvX, uvY, uvW, uvH);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, vertex_list_count);
}

void n3ds_graphics::DrawQuadMatrix(const C3D_Mtx *modelView, float uvX, float uvY, float uvW, float uvH)
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, modelView);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_uvOffset, uvX, uvY, uvW, uvH);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, vertex_list_count);
}

void n3ds_graphics::SetProjection(const C3D_Mtx *projection)
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, projection);
}

void n3ds_graphics::CreateTextureObject(C3D_Tex *textureObject, uint16_t width, uint16_t height, GPU_TEXCOLOR format, GPU_TEXTURE_WRAP_PARAM wrap, GPU_TEXTURE_FILTER_PARAM filter)
{
    bool success = C3D_TexInitVRAM(textureObject, width, height, format);

    if (!success)
    {
        std::string message = "Error creating texture object.";
        svcOutputDebugString(message.c_str(), message.length());
    }

    C3D_TexSetFilter(textureObject, filter, filter);
    C3D_TexSetWrap(textureObject, wrap, wrap);
}

void n3ds_graphics::UploadTextureObject(C3D_Tex *textureObject, void *textureData)
{
    C3D_TexUpload(textureObject, textureData);
    //C3D_TexFlush(textureObject);
}

void n3ds_graphics::BindTextureObject(C3D_Tex *textureObject, int32_t mapIndex)
{
    if (mapIndex == 0)
        boundTexture = textureObject;
    C3D_TexBind(mapIndex, textureObject);
}

uint32_t n3ds_graphics::GetTextureSize(uint16_t width, uint16_t height, GPU_TEXCOLOR format, int32_t maxLevel)
{
    uint32_t size = 0;

    switch (format)
    {
    case GPU_RGBA8:
        size = 32;
        break;
    case GPU_RGB8:
        size = 24;
        break;
    case GPU_RGBA5551:
    case GPU_RGB565:
    case GPU_RGBA4:
    case GPU_LA8:
    case GPU_HILO8:
        size = 16;
        break;
    case GPU_L8:
    case GPU_A8:
    case GPU_LA4:
    case GPU_ETC1A4:
        size = 8;
        break;
    case GPU_L4:
    case GPU_A4:
    case GPU_ETC1:
        size = 4;
        break;
    }

    size *= (uint32_t)width * height / 8;
    return C3D_TexCalcTotalSize(size, maxLevel);
}
