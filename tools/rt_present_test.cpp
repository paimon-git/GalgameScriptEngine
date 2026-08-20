#include <raylib.h>
#include <rlgl.h>
#include <cstdio>

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(320, 180, "rt present test");
    SetTargetFPS(60);

    RenderTexture2D rt = LoadRenderTexture(640, 360);

    BeginTextureMode(rt);
    printf("inside BeginTextureMode: GetScreenWidth=%d GetScreenHeight=%d GetRenderWidth=%d GetRenderHeight=%d\n",
           GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight());
    ClearBackground(Color{10, 10, 10, 255});
    DrawRectangle(0, 0, 320, 180, Color{30, 40, 50, 255});
    EndTextureMode();
    {
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        ExportImage(img, "rt_noscale.png");
        UnloadImage(img);
    }

    int frame = 0;
    while (!WindowShouldClose() && frame < 3)
    {
        BeginTextureMode(rt);
        ClearBackground(Color{10, 10, 10, 255});
        rlPushMatrix();
        rlScalef(2.0f, 2.0f, 1.0f);
        DrawRectangle(10, 10, 50, 50, Color{255, 0, 0, 255});
        DrawRectangle(160, 90, 40, 40, Color{0, 255, 0, 255});
        rlPopMatrix();
        EndTextureMode();

        BeginDrawing();
        DrawTexturePro(rt.texture,
                       {0, 0, static_cast<float>(rt.texture.width),
                        -static_cast<float>(rt.texture.height)},
                       {0, 0, static_cast<float>(GetScreenWidth()),
                        static_cast<float>(GetScreenHeight())},
                       {0, 0}, 0.0f, WHITE);
        EndDrawing();
        ++frame;
    }

    {
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        ExportImage(img, "rt_present_rt.png");
        UnloadImage(img);
    }
    TakeScreenshot("rt_present_shot.png");
    CloseWindow();
    return 0;
}
