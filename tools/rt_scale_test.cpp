#include <raylib.h>
#include <rlgl.h>
#include <cstdio>

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(320, 180, "rt scale test");
    SetTargetFPS(60);

    RenderTexture2D rt = LoadRenderTexture(640, 360); // 2x of 320x180

    BeginTextureMode(rt);
    ClearBackground(Color{0, 0, 0, 255});
    rlPushMatrix();
    rlScalef(2.0f, 2.0f, 1.0f);
    // logical coords: 0..320 x 0..180 should fill the whole 640x360 rt
    DrawRectangle(0, 0, 320, 180, Color{20, 30, 40, 255});
    // red rect at logical (10,10,50,50) -> rt (20,20,100,100)
    DrawRectangle(10, 10, 50, 50, Color{255, 0, 0, 255});
    // green rect at logical (160,90,40,40) -> rt center (320,180,80,80)
    DrawRectangle(160, 90, 40, 40, Color{0, 255, 0, 255});
    rlPopMatrix();
    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    ExportImage(img, "rt_scale_test.png");
    printf("rt (640x360): corner=%d,%d center=%d,%d top-left-red=%d,%d\n",
           GetImageColor(img, 1, 1).r, GetImageColor(img, 1, 1).g,
           GetImageColor(img, 320, 180).r, GetImageColor(img, 320, 180).g,
           GetImageColor(img, 60, 60).r, GetImageColor(img, 60, 60).g);
    UnloadImage(img);

    UnloadRenderTexture(rt);
    CloseWindow();
    return 0;
}
