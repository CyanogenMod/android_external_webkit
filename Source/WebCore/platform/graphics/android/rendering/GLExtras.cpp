/*
 * Copyright 2011, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "GLExtras"
#define LOG_NDEBUG 1

#include "config.h"

#include "AndroidLog.h"
#include "DrawExtra.h"
#include "DrawQuadData.h"
#include "GLExtras.h"
#include "IntRect.h"
#include "SkPath.h"
#include "TilesManager.h"
#include "android_graphics.h"

// Touch ring border width. This is doubled if the ring is not pressed
#define RING_BORDER_WIDTH 1

GLExtras::GLExtras()
    : m_drawExtra(0)
    , m_visibleContentRect()
{
}

GLExtras::~GLExtras()
{
}

void GLExtras::drawRing(SkRect& srcRect, Color color, const TransformationMatrix* drawMat)
{
    if (srcRect.fRight <= srcRect.fLeft || srcRect.fBottom <= srcRect.fTop) {
        // Invalid rect, reject it
        return;
    }
    ALOGV("drawQuad [%fx%f, %f, %f]", srcRect.fLeft, srcRect.fTop,
          srcRect.width(), srcRect.height());
    // Pull the alpha out of the color so that the shader applies it correctly.
    // Otherwise we either don't have blending enabled, or the alpha will get
    // double applied
    Color colorWithoutAlpha(0xFF000000 | color.rgb());
    float alpha = color.alpha() / (float) 255;

    PureColorQuadData data(colorWithoutAlpha, drawMat ? LayerQuad : BaseQuad,
                           drawMat, &srcRect, alpha, false);
    TilesManager::instance()->shader()->drawQuad(&data);
}

void GLExtras::drawRegion(const SkRegion& region, bool fill, bool drawBorder,
                          const TransformationMatrix* drawMat, Color color)
{
    if (region.isEmpty())
        return;
    if (fill) {
        SkRegion::Iterator rgnIter(region);
        while (!rgnIter.done()) {
            const SkIRect& ir = rgnIter.rect();
            SkRect r;
            r.set(ir.fLeft, ir.fTop, ir.fRight, ir.fBottom);
            drawRing(r, color, drawMat);
            rgnIter.next();
        }
    }
    if (fill && !drawBorder)
        return;
    SkPath path;
    if (!region.getBoundaryPath(&path))
        return;
    SkPath::Iter iter(path, true);
    SkPath::Verb verb;
    SkPoint pts[4];
    SkRegion clip;
    SkIRect startRect;
    while ((verb = iter.next(pts)) != SkPath::kDone_Verb) {
        if (verb == SkPath::kLine_Verb) {
            SkRect r;
            r.set(pts, 2);
            SkIRect line;
            int borderWidth = RING_BORDER_WIDTH;
            if (!fill)
                borderWidth *= 2;
            line.fLeft = r.fLeft - borderWidth;
            line.fRight = r.fRight + borderWidth;
            line.fTop = r.fTop - borderWidth;
            line.fBottom = r.fBottom + borderWidth;
            if (clip.intersects(line)) {
                clip.op(line, SkRegion::kReverseDifference_Op);
                if (clip.isEmpty())
                    continue; // Nothing to draw, continue
                line = clip.getBounds();
                if (SkIRect::Intersects(startRect, line)) {
                    clip.op(startRect, SkRegion::kDifference_Op);
                    if (clip.isEmpty())
                        continue; // Nothing to draw, continue
                    line = clip.getBounds();
                }
            } else {
                clip.setRect(line);
            }
            r.set(line.fLeft, line.fTop, line.fRight, line.fBottom);
            drawRing(r, color, drawMat);
            if (startRect.isEmpty()) {
                startRect.set(line.fLeft, line.fTop, line.fRight, line.fBottom);
            }
        }
        if (verb == SkPath::kMove_Verb) {
            startRect.setEmpty();
        }
    }
}

void GLExtras::drawGL(const LayerAndroid* layer)
{
    if (m_drawExtra)
        m_drawExtra->drawGL(this, layer);
}
