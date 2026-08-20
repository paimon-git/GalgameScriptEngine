#include "character.h"

#include <cmath>

void Character::update(float dt)
{
    // 淡入 / 淡出：时间线性推进，透明度用 smoothstep 缓动（非生硬线性）
    float goal = (targetAlpha > 0.0f) ? 1.0f : 0.0f;
    float speed = dt / 0.32f;
    if (goal > fadeT)
    {
        fadeT += speed;
        if (fadeT >= goal) fadeT = goal;
    }
    else
    {
        fadeT -= speed;
        if (fadeT <= goal) fadeT = goal;
    }
    float t = fadeT;
    alpha = t * t * (3.0f - 2.0f * t) * 255.0f;

    // 位置移动（缓入缓出）
    if (moveT < 1.0f)
    {
        moveT += dt / moveDuration;
        if (moveT >= 1.0f) moveT = 1.0f;
        float t = moveT;
        float eased = t * t * (3.0f - 2.0f * t);
        anchorX = moveFrom + (moveTarget - moveFrom) * eased;
    }

    breathPhase += dt * 1.5f;
}

void Character::moveTo(float x, float duration)
{
    moveFrom = anchorX;
    moveTarget = x;
    moveT = 0.0f;
    moveDuration = duration > 0.05f ? duration : 0.05f;
}

void Character::draw(float screenW, float screenH, bool speaking) const
{
    (void)speaking;
    if (alpha <= 0.01f) return;
    Texture2D bodyTex = body;
    Texture2D exprTex = face;
    if (bodyTex.id == 0 && exprTex.id == 0) return;

    float chH = screenH * 0.88f;
    float aspect = bodyTex.id ? static_cast<float>(bodyTex.width) / static_cast<float>(bodyTex.height) : 1.0f;
    float chW = chH * aspect;
    float breath = 1.0f + 0.006f * std::sin(breathPhase);

    float drawH = chH * breath;
    float drawW = chW * breath;
    float cx = anchorX * screenW + speakOffset;
    float bottomY = screenH + 4.0f;

    Color tint{255, 255, 255, static_cast<unsigned char>(alpha)};
    if (bodyTex.id)
    {
        Rectangle src{0, 0, static_cast<float>(bodyTex.width), static_cast<float>(bodyTex.height)};
        Rectangle dst{cx - drawW * 0.5f, bottomY - drawH, drawW, drawH};
        DrawTexturePro(bodyTex, src, dst, {0, 0}, 0.0f, tint);
    }
    // 表情图与本体分离：单独 PNG，叠加绘制在同一位置（画布尺寸一致时自然对齐）
    if (exprTex.id)
    {
        float exprAspect = static_cast<float>(exprTex.width) / static_cast<float>(exprTex.height);
        float exprW = drawH * exprAspect;
        Rectangle src{0, 0, static_cast<float>(exprTex.width), static_cast<float>(exprTex.height)};
        Rectangle dst{cx - exprW * 0.5f, bottomY - drawH, exprW, drawH};
        DrawTexturePro(exprTex, src, dst, {0, 0}, 0.0f, tint);
    }
}
