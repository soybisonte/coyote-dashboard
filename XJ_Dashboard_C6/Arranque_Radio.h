#pragma once

#include <stdint.h>
#include "Arranque_XJ.h"

namespace ArranqueRadio {

constexpr uint32_t magic = 0x584A5331;
constexpr uint16_t version = 2;
constexpr uint16_t introFrame = 0, firstMotionFrame = 1;
constexpr uint16_t motionFrames = ArranqueXJ::frameCount + ArranqueXJ::closingFrameCount;
constexpr uint16_t logoFrame = firstMotionFrame + motionFrames;
constexpr uint16_t doneFrame = logoFrame + 1;
constexpr uint32_t frameMs = 95;
constexpr uint32_t introHoldMs = 1800;
constexpr uint32_t logoHoldMs = 1250;
constexpr uint32_t logoOffsetMs = introHoldMs + motionFrames * frameMs;
constexpr uint32_t durationMs = logoOffsetMs + logoHoldMs;

inline uint16_t frameAt(uint32_t elapsed) {
  if (elapsed < introHoldMs) return introFrame;
  if (elapsed < logoOffsetMs) return firstMotionFrame + (elapsed - introHoldMs) / frameMs;
  return elapsed < durationMs ? logoFrame : doneFrame;
}

inline uint32_t offsetFor(uint16_t frame) {
  if (frame == introFrame) return 0;
  if (frame >= doneFrame) return durationMs;
  return frame >= logoFrame ? logoOffsetMs :
      introHoldMs + (frame - firstMotionFrame) * frameMs;
}

struct Packet {
  uint32_t magic;
  uint16_t version;
  uint16_t frame;
  uint32_t session;
};
static_assert(sizeof(Packet) == 12, "Formato de arranque inesperado");

}  // namespace ArranqueRadio
