#include "stak.h"
#include "gfx.hpp"
#include "display.hpp"
#include "timeline.hpp"
#include "util.hpp"

#include "gtx/rotate_vector.hpp"

// Debug
#include <iostream>
#include <string>

using namespace otto;
using namespace choreograph;

static const float pi = M_PI;
static const float twoPi = M_PI * 2.0f;
static const float halfPi = M_PI / 2.0f;

static Display display = { { 96.0f, 96.0f } };

static struct {
  uint32_t minFrame = 1;
  uint32_t maxFrame = 33;
  uint32_t nextFrame = minFrame;

  bool isRewinding = false;
  float rewindAmountMin = 0.05f;
  float rewindAmount = 0.0f;

  Output<float> rewindMeterAmount = 0.0f;
  Output<float> rewindMeterOpacity = 0.0f;

  Output<vec3> flashColor;

  void save() {
    rewindAmount = 0.0f;
    nextFrame = minFrame;
  }

  void captureFrame() {
    static const vec3 flashColors[] = { colorBGR(0x00ADEF), colorBGR(0xEC008B),
                                        colorBGR(0xFFF100) };
    static size_t colorIndex = 0;

    if (nextFrame <= maxFrame) {
      nextFrame++;
      colorIndex = (colorIndex + 1) % 3;
      timeline.apply(&flashColor)
          .then<Hold>(flashColors[colorIndex], 0.0f)
          .then<RampTo>(vec3(), 0.3f, EaseOutQuad());
    } else if (!isRewinding) {
      rewind(0.05f);
    }
  }

  void startRewinding() {
    isRewinding = true;
    rewindAmount = 0.0f;
    timeline.apply(&rewindMeterAmount).then<Hold>(0.0f, 0.0f);
    timeline.apply(&rewindMeterOpacity).then<RampTo>(1.0f, 0.25f, EaseOutQuad());
  }

  void stopRewinding() {
    isRewinding = false;
    timeline.apply(&rewindMeterOpacity).then<RampTo>(0.0f, 0.25f, EaseInQuad());
  }

  void rewind(float amount) {
    if (!isRewinding) startRewinding();
    rewindAmount = std::min(1.0f, rewindAmount + amount);
    timeline.apply(&rewindMeterAmount).then<RampTo>(rewindAmount, 0.05f);
    if (rewindAmount > 0.99f) {
      save();
      stopRewinding();
    }
  }

} mode;

STAK_EXPORT int init() {
  loadFont(std::string(stak_assets_path()) + "232MKSD-round-medium.ttf");
  return 0;
}

STAK_EXPORT int shutdown() {
  return 0;
}

STAK_EXPORT int update(float dt) {
  display.update([dt] { timeline.step(dt); });
  return 0;
}

STAK_EXPORT int draw() {
  display.draw([] {
    beginPath();
    rect(vec2(), display.bounds.size);
    fillColor(mode.flashColor());
    fill();

    translate(display.bounds.size * 0.5f);

    {
      ScopedTransform xf;
      translate(0, -11);

      uint32_t frameNum = mode.isRewinding ? map(mode.rewindAmount, mode.rewindAmountMin, 1.0f,
                                                 mode.maxFrame, mode.minFrame)
                                           : mode.nextFrame;

      fontSize(40);
      textAlign(ALIGN_CENTER | ALIGN_BASELINE);
      fillColor(vec4(1));
      fillText(std::to_string(frameNum), 0, 11);

      if (!mode.isRewinding) {
        beginPath();
        moveTo(-15, -3);
        lineTo(0, 3);
        lineTo(0, -3);
        lineTo(15, 3);
        strokeWidth(2);
        strokeColor(vec4(1, 1, 1, 0.35f));
        stroke();
      }

      fillColor(vec4(1, 1, 1, 0.35f));
      if (mode.isRewinding) {
        fontSize(16);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillText("rewind", 0, -4);
      } else {
        fontSize(20);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        fillText(std::to_string(mode.maxFrame), 0, -5);
      }
    }

    if (mode.rewindMeterOpacity > 0.0f) {
      float r = display.bounds.size.x * 0.5f - 6.0f;
      float a = halfPi + mode.rewindMeterAmount() * twoPi;

      auto color = vec4(colorBGR(0xEC008B), mode.rewindMeterOpacity());

      ScopedMask mask(display.bounds.size);
      ScopedFillRule fr(VG_NON_ZERO);
      beginMask();

      beginPath();
      arc(vec2(), vec2(r * 2.0f), a, halfPi);
      strokeWidth(3.0f);
      strokeCap(VG_CAP_BUTT);
      strokeColor(color);
      stroke();

      beginPath();
      circle(0, r, 3);
      circle(glm::rotate(vec2(r, 0), a), 3);
      fillColor(color);
      fill();

      endMask();

      beginPath();
      rect(display.bounds.size * -0.5f, display.bounds.size);
      fillColor(color);
      fill();
    }
  });
  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  if (!display.wake()) {
    if (amount > 0) {
      if (!mode.isRewinding) {
        mode.captureFrame();
      } else if (!mode.rewindMeterAmount.isConnected()) {
        timeline.apply(&mode.rewindMeterAmount)
            .then<RampTo>(std::max(0.0f, mode.rewindAmount - 0.05f), 0.09f, EaseOutQuad())
            .then<RampTo>(mode.rewindAmount, 0.09f, EaseInQuad());
      }
    } else if (amount < 0 && (mode.isRewinding || mode.nextFrame > mode.minFrame)) {
      mode.rewind(mode.rewindAmountMin);
    }
  }
  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  if (!display.wake() && !mode.isRewinding) mode.captureFrame();
  return 0;
}

STAK_EXPORT int shutter_button_released() {
  display.wake();
  return 0;
}

STAK_EXPORT int crank_pressed() {
  display.wake();
  return 0;
}

STAK_EXPORT int crank_released() {
  display.wake();
  return 0;
}
