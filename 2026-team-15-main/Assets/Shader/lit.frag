#version 300 es
// The lit shader's fragment stage: one colour per mesh, lit by a fixed sun light, with an optional highlight tint,
// see-through alpha, and hiding behind real things using ARCore's depth picture.
// (The #version line must stay first: Shader.cpp swaps it for the desktop 330 core version.)
precision mediump float;
precision highp sampler2D; // the default (lowp) can round the depth's high byte away: steps of up to 25 cm
in vec3 vNormal;           // world-space normal from lit.vert
in vec4 vClip;             // clip-space position from lit.vert
uniform vec3 uColor;       // the piece's base colour (EquipmentScene::ColorFor for now)
uniform vec3 uTint;        // highlight colour, e.g. yellow for the selected piece
uniform float uTintAmount; // 0 = plain colour, 1 = fully the tint (unlit: used by the loader dots)
uniform float uAlpha;      // 1 = solid, below 1 = see-through

// Occlusion: ARCore's depth picture says how far the real world is at each screen spot.
// Where a real thing (a hand) is closer than this piece, the piece is hidden there.
uniform float uUseDepth;      // 1 = hide behind real things, 0 = always drawn (no depth, or screen-space items like the loader)
uniform sampler2D uDepthTex;  // millimetres: low byte in red, high byte in green, 0 = unknown
uniform vec3 uDepthUvX;       // screen (0..1) -> depth picture (0..1): uv = O + x * X + y * Y (only .xy is used)
uniform vec3 uDepthUvY;
uniform vec3 uDepthUvO;

out vec4 outColor;

// Visibility against ONE depth pixel: 1 = the piece is in front of the real surface there, 0 = behind it.
float VisibilityAt(ivec2 texel, highp float pieceMetres)
{
    highp vec2 bytes = texelFetch(uDepthTex, texel, 0).rg;
    highp float realMetres = dot(bytes, vec2(255.0, 65280.0)) * 0.001;
    if (realMetres <= 0.0)
    {
        return 1.0; // ARCore does not know the depth here: keep the piece
    }
    // Soft edge, all of it behind the real surface: fully shown up to 1 cm behind it (so a piece standing on the table,
    // which ARCore's rough depth puts at about the same distance, stays solid), fully hidden from 5 cm behind.
    return clamp(1.0 - (pieceMetres - realMetres - 0.01) / 0.04, 0.0, 1.0);
}

// How much of this pixel to keep (0..1) once real things in front of it are taken into account.
// The depth picture is tiny (160 x 90): one depth pixel covers a block of about 15 x 38 screen pixels. Reading only the
// nearest one gives a hard hide-or-show per block, so edges look like staircases. Instead, the 4 depth pixels around this
// spot each give their own answer, and the answers are blended by how close this spot is to each one. Blending the
// ANSWERS (not the depths) matters: a hand at 0.3 m next to a wall at 2 m must never average to a fake 1.15 m.
float Visibility()
{
    if (uUseDepth < 0.5)
    {
        return 1.0;
    }
    highp vec2 screen = vClip.xy / vClip.w * 0.5 + 0.5;
    highp vec2 uv = uDepthUvO.xy + screen.x * uDepthUvX.xy + screen.y * uDepthUvY.xy;
    highp float pieceMetres = vClip.w; // for a perspective camera, w is the distance in front of it

    highp vec2 size = vec2(textureSize(uDepthTex, 0));
    highp vec2 pos = uv * size - 0.5;          // in depth pixels, measured between pixel centres
    highp vec2 cell = floor(pos);
    highp vec2 f = pos - cell;                  // 0..1: how far between the 4 neighbours
    ivec2 maxTexel = ivec2(size) - 1;
    ivec2 t00 = clamp(ivec2(cell), ivec2(0), maxTexel);
    ivec2 t11 = clamp(ivec2(cell) + 1, ivec2(0), maxTexel);
    float v00 = VisibilityAt(t00, pieceMetres);
    float v10 = VisibilityAt(ivec2(t11.x, t00.y), pieceMetres);
    float v01 = VisibilityAt(ivec2(t00.x, t11.y), pieceMetres);
    float v11 = VisibilityAt(t11, pieceMetres);
    return mix(mix(v00, v10, f.x), mix(v01, v11, f.x), f.y);
}

void main()
{
    vec3 n = normalize(vNormal);
    float diffuse = max(dot(n, normalize(vec3(0.4, 1.0, 0.6))), 0.0); // the sun: up and a little to the side
    vec3 lit = uColor * (0.3 + 0.7 * diffuse);                         // 30% ambient so shadowed sides are not black
    float alpha = uAlpha * Visibility();
    if (alpha < 0.01)
    {
        discard; // fully behind a real thing: no colour, and no depth either
    }
    outColor = vec4(mix(lit, uTint, uTintAmount), alpha);
}
