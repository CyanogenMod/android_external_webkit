/*
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SYNCPROXYCANVAS_H
#define SYNCPROXYCANVAS_H

#include "SkCanvas.h"
#include "SkDevice.h"

// This is just like SkProxyCanvas except we handle drawPicture ourselves
// and track the synchronization of the state between the two canvases.

class SyncProxyCanvas: public SkCanvas
{
private:
    SkBitmap blackHole;

protected:
    SkCanvas* target;

public:

    SyncProxyCanvas(SkCanvas* t)
        : target(t)
    {
        SkDevice* device = target->getDevice();
        int w = device->width();
        int h = device->height();
        blackHole.setConfig(SkBitmap::kNo_Config, w, h);
        SkCanvas::setBitmapDevice(blackHole);

        target->safeRef();
        SkCanvas::setMatrix(target->getTotalMatrix());
        SkCanvas::clipRegion(target->getTotalClip(), SkRegion::kReplace_Op);
    }

    virtual ~SyncProxyCanvas()
    {
        target->safeUnref();
    }

    virtual bool getViewport(SkIPoint* size) const {
        return target->getViewport(size);
    }

    virtual bool setViewport(int x, int y) {
        return target->setViewport(x, y);
    }

    virtual int save(SaveFlags flags) {
        SkCanvas::save(flags);
        return target->save(flags);
    }

    virtual int saveLayer(const SkRect* bounds, const SkPaint* paint,
                                 SaveFlags flags = kARGB_ClipLayer_SaveFlag) {
        // saveLayer() behaves the same as save() but in addition, it allocates
        // an offscreen bitmap where all drawing calls are directed. When the
        // balancing call to restore() is made, that offscreen is transferred
        // to the canvas and the bitmap is deleted. SyncProxyCanvas calls save()
        // instead in order to avoid the bitmap allocation.
        SkCanvas::save(flags);
        return target->saveLayer(bounds, paint, flags);
    }

    virtual void restore() {
        target->restore();
        SkCanvas::restore();
    }

    virtual bool translate(SkScalar dx, SkScalar dy) {
        bool result =  target->translate(dx, dy);
        if (result)
            SkCanvas::translate(dx, dy);
        return result;
    }

    virtual bool scale(SkScalar sx, SkScalar sy) {
        bool result = target->scale(sx, sy);
        if (result)
            SkCanvas::scale(sx, sy);
        return result;
    }

    virtual bool rotate(SkScalar degrees) {
        bool result = target->rotate(degrees);
        if (result)
            SkCanvas::rotate(degrees);
        return result;
    }

    virtual bool skew(SkScalar sx, SkScalar sy) {
        bool result = target->skew(sx, sy);
        if (result)
            SkCanvas::skew(sx, sy);
        return result;
    }

    virtual bool concat(const SkMatrix& matrix) {
        bool result = target->concat(matrix);
        if (result)
            SkCanvas::concat(matrix);
        return result;
    }

    virtual void setMatrix(const SkMatrix& matrix) {
        target->setMatrix(matrix);
        SkCanvas::setMatrix(matrix);
    }

    virtual bool clipRect(const SkRect& rect, SkRegion::Op op) {
        bool result = target->clipRect(rect, op);
        if (result)
            SkCanvas::clipRect(rect, op);
        return result;
    }

    virtual bool clipPath(const SkPath& path, SkRegion::Op op) {
        bool result = target->clipPath(path, op);
        if (result)
            SkCanvas::clipPath(path, op);
        return result;
    }

    virtual bool clipRegion(const SkRegion& deviceRgn, SkRegion::Op op) {
        bool result = target->clipRegion(deviceRgn, op);
        if (result)
            SkCanvas::clipRegion(deviceRgn, op);
        return result;
    }

    virtual void drawPaint(const SkPaint& paint) {
        target->drawPaint(paint);
    }

    virtual void drawPoints(PointMode mode, size_t count,
                                   const SkPoint pts[], const SkPaint& paint) {
        target->drawPoints(mode, count, pts, paint);
    }

    virtual void drawRect(const SkRect& rect, const SkPaint& paint) {
        target->drawRect(rect, paint);
    }

    virtual void drawPath(const SkPath& path, const SkPaint& paint) {
        target->drawPath(path, paint);
    }

    virtual void drawBitmap(const SkBitmap& bitmap, SkScalar x, SkScalar y,
                                   const SkPaint* paint) {
        target->drawBitmap(bitmap, x, y, paint);
    }

    virtual void drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src,
                                       const SkRect& dst, const SkPaint* paint) {
        target->drawBitmapRect(bitmap, src, dst, paint);
    }

    virtual void drawBitmapMatrix(const SkBitmap& bitmap, const SkMatrix& m,
                                         const SkPaint* paint) {
        target->drawBitmapMatrix(bitmap, m, paint);
    }

    virtual void drawSprite(const SkBitmap& bitmap, int x, int y,
                                   const SkPaint* paint) {
        target->drawSprite(bitmap, x, y, paint);
    }

    virtual void drawText(const void* text, size_t byteLength, SkScalar x,
                                 SkScalar y, const SkPaint& paint) {
        target->drawText(text, byteLength, x, y, paint);
    }

    virtual void drawPosText(const void* text, size_t byteLength,
                                    const SkPoint pos[], const SkPaint& paint) {
        target->drawPosText(text, byteLength, pos, paint);
    }

    virtual void drawPosTextH(const void* text, size_t byteLength,
                                     const SkScalar xpos[], SkScalar constY,
                                     const SkPaint& paint) {
        target->drawPosTextH(text, byteLength, xpos, constY, paint);
    }

    virtual void drawTextOnPath(const void* text, size_t byteLength,
                                       const SkPath& path, const SkMatrix* matrix,
                                       const SkPaint& paint) {
        target->drawTextOnPath(text, byteLength, path, matrix, paint);
    }

    virtual void drawShape(SkShape* shape) {
        target->drawShape(shape);
    }

    virtual void drawVertices(VertexMode vmode, int vertexCount,
                                     const SkPoint vertices[], const SkPoint texs[],
                                     const SkColor colors[], SkXfermode* xmode,
                                     const uint16_t indices[], int indexCount,
                                     const SkPaint& paint) {
        target->drawVertices(vmode, vertexCount, vertices, texs, colors,
                                         xmode, indices, indexCount, paint);
    }

    virtual void drawData(const void* data, size_t length) {
        target->drawData(data, length);
    }

    virtual void drawPicture(SkPicture& picture) {
        // Use target canvas for save and restore operations because SyncProxyCanvas
        // returns target's save count during calls to save() or saveLayer().
        int saveCount = target->save();
        picture.draw(this);
        target->restoreToCount(saveCount);
    }

};

#endif
