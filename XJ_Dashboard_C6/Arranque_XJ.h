#pragma once

#include <Adafruit_GFX.h>
#include <math.h>
#include "Logo_Willys.h"

namespace ArranqueXJ {

constexpr int panelWidth = 128;
constexpr int panelHeight = 64;
constexpr int columnPitch = 152;
constexpr int rowPitch = 104;
constexpr int sceneWidth = 2 * columnPitch + panelWidth;
constexpr int sceneHeight = rowPitch + panelHeight;
constexpr unsigned frameCount = 43;
constexpr unsigned closingFrameCount = 7;
constexpr int logoColumn = 1;
constexpr int logoRow = 0;

struct Point {
  int16_t x;
  int16_t y;
};

// El terreno conecta las posiciones fisicas de las OLED en el mismo lienzo.
constexpr Point trail[] = {
  {0, 55}, {79, 55}, {100, 57}, {116, 65}, {173, 148},
  {198, 154}, {217, 154}, {234, 145}, {247, 152}, {267, 155},
  {288, 151}, {307, 155}, {324, 147}, {337, 122},
  {355, 91}, {373, 58}, {382, 55}, {sceneWidth - 1, 55},
};

inline float trailHeight(float x) {
  for (unsigned i = 1; i < sizeof(trail) / sizeof(trail[0]); ++i) {
    const Point from = trail[i - 1];
    const Point to = trail[i];
    if (x <= to.x) {
      const float fraction = (x - from.x) / (to.x - from.x);
      return from.y + fraction * (to.y - from.y);
    }
  }
  return trail[sizeof(trail) / sizeof(trail[0]) - 1].y;
}

inline void drawTrail(GFXcanvas1 &scene) {
  for (unsigned i = 1; i < sizeof(trail) / sizeof(trail[0]); ++i) {
    const Point from = trail[i - 1];
    const Point to = trail[i];
    scene.drawLine(from.x, from.y, to.x, to.y, 1);
  }
  constexpr int rockPositions[] = {17, 58, 168, 201, 226, 263, 318, 325, 408};
  for (const int x : rockPositions) {
    const int y = lroundf(trailHeight(x)) + 3;
    scene.drawLine(x, y, x + 4, y + 3, 1);
    scene.drawLine(x + 4, y + 3, x + 7, y + 2, 1);
  }
}

struct Pose {
  float x;
  float y;
  float cosine;
  float sine;

  Point project(Point p) const {
    return {
      static_cast<int16_t>(lroundf(x + cosine * p.x - sine * p.y)),
      static_cast<int16_t>(lroundf(y + sine * p.x + cosine * p.y)),
    };
  }
};

inline void bodyLine(GFXcanvas1 &scene, const Pose &pose, Point from, Point to,
                     uint16_t color = 1) {
  const Point a = pose.project(from);
  const Point b = pose.project(to);
  scene.drawLine(a.x, a.y, b.x, b.y, color);
}

inline void bodyRect(GFXcanvas1 &scene, const Pose &pose, Point topLeft,
                     Point bottomRight) {
  const Point a = pose.project(topLeft);
  const Point b = pose.project({bottomRight.x, topLeft.y});
  const Point c = pose.project(bottomRight);
  const Point d = pose.project({topLeft.x, bottomRight.y});
  scene.fillTriangle(a.x, a.y, b.x, b.y, c.x, c.y, 1);
  scene.fillTriangle(a.x, a.y, c.x, c.y, d.x, d.y, 1);
}

inline void drawWheel(GFXcanvas1 &scene, Point hub, float rotation) {
  scene.fillCircle(hub.x, hub.y, 10, 0);
  scene.fillCircle(hub.x, hub.y, 9, 1);
  scene.fillCircle(hub.x, hub.y, 6, 0);
  // Cinco rayos macizos: cada uno ocupa un trapecio de unos 2-3 pixeles.
  for (int spoke = 0; spoke < 5; ++spoke) {
    const float angle = rotation + spoke * 1.25663706f;
    const float dx = cosf(angle), dy = sinf(angle);
    const Point a{static_cast<int16_t>(lroundf(hub.x + dx - dy * 0.7f)),
                  static_cast<int16_t>(lroundf(hub.y + dy + dx * 0.7f))};
    const Point b{static_cast<int16_t>(lroundf(hub.x + dx * 5 - dy)),
                  static_cast<int16_t>(lroundf(hub.y + dy * 5 + dx))};
    const Point c{static_cast<int16_t>(lroundf(hub.x + dx * 5 + dy)),
                  static_cast<int16_t>(lroundf(hub.y + dy * 5 - dx))};
    const Point d{static_cast<int16_t>(lroundf(hub.x + dx + dy * 0.7f)),
                  static_cast<int16_t>(lroundf(hub.y + dy - dx * 0.7f))};
    scene.fillTriangle(a.x, a.y, b.x, b.y, c.x, c.y, 1);
    scene.fillTriangle(a.x, a.y, c.x, c.y, d.x, d.y, 1);
  }
  scene.fillCircle(hub.x, hub.y, 1, 1);
}

inline void drawWillys(GFXcanvas1 &scene, float x) {
  constexpr int halfWheelbase = 19;
  constexpr int tireRadius = 9;
  const float rear = trailHeight(x - halfWheelbase);
  const float front = trailHeight(x + halfWheelbase);
  const float pitch = atan2f(front - rear, 2 * halfWheelbase);
  const float cosine = cosf(pitch);
  const float sine = sinf(pitch);
  const Pose pose = {
    x + sine * tireRadius, (front + rear) * 0.5f - cosine * tireRadius,
    cosine, sine,
  };

  // Carrocería abierta, cofre plano, salpicaderas y parabrisas del Willys.
  bodyRect(scene, pose, {-29, -9}, {29, -3});
  bodyRect(scene, pose, {-28, -18}, {-9, -9});
  bodyRect(scene, pose, {5, -17}, {27, -9});
  bodyRect(scene, pose, {8, -11}, {31, -8});
  bodyRect(scene, pose, {-30, -5}, {-26, -2});
  bodyRect(scene, pose, {27, -5}, {34, -2});
  bodyLine(scene, pose, {6, -16}, {1, -34});
  bodyLine(scene, pose, {8, -16}, {3, -34});
  bodyLine(scene, pose, {0, -34}, {4, -34});
  bodyLine(scene, pose, {-11, -11}, {-13, -23});
  bodyLine(scene, pose, {-10, -11}, {-12, -23});
  bodyLine(scene, pose, {-1, -15}, {3, -11});
  bodyLine(scene, pose, {25, -14}, {27, -14}, 0);

  const Point spare = pose.project({-31, -14});
  scene.fillCircle(spare.x, spare.y, 4, 1);
  scene.fillCircle(spare.x, spare.y, 2, 0);
  const float rotation = x / tireRadius;
  drawWheel(scene, pose.project({-halfWheelbase, 0}), rotation);
  drawWheel(scene, pose.project({halfWheelbase, 0}), rotation);
}

inline void drawFrame(GFXcanvas1 &scene, unsigned frame) {
  scene.fillScreen(0);
  drawTrail(scene);
  const float progress = static_cast<float>(frame) / (frameCount - 1);
  const float eased = 0.78f * progress +
                      0.22f * progress * progress * (3.0f - 2.0f * progress);
  drawWillys(scene, 41.0f + 353.0f * eased);
}

inline void drawClosingFrame(GFXcanvas1 &scene, unsigned step) {
  constexpr int logoX = logoColumn * columnPitch + (panelWidth - logoWidth) / 2;
  constexpr int logoY = logoRow * rowPitch + (panelHeight - logoHeight) / 2;
  const int edge = sceneWidth * step / closingFrameCount;

  drawFrame(scene, frameCount - 1);
  scene.fillRect(0, 0, edge, sceneHeight, 0);
  if (edge >= logoX + logoWidth) {
    scene.drawBitmap(logoX, logoY, logoWillys, logoWidth, logoHeight, 1);
    return;
  }

  // El mismo barrido que borra el recorrido descubre el logo en su posicion.
  constexpr int stride = (logoWidth + 7) / 8;
  for (int x = 0; x < edge - logoX; ++x) {
    for (int y = 0; y < logoHeight; ++y) {
      const uint8_t bits = pgm_read_byte(logoWillys + y * stride + x / 8);
      if (bits & (0x80 >> (x % 8))) scene.drawPixel(logoX + x, logoY + y, 1);
    }
  }
}

}  // namespace ArranqueXJ
