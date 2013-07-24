/*
 * Copyright 2012, The Android Open Source Project
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

#define LOG_TAG "PlatformGraphicsContextRecording"
#define LOG_NDEBUG 1

#include "config.h"
#include "PlatformGraphicsContextRecording.h"

#include "AndroidLog.h"
#include "FloatRect.h"
#include "FloatQuad.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "GraphicsOperation.h"
#include "PlatformGraphicsContextSkia.h"
#include "RTree.h"
#include "SkDevice.h"

#include "wtf/NonCopyingSort.h"
#include "wtf/HashSet.h"
#include "wtf/StringHasher.h"

#include <utils/LinearAllocator.h>

#define NEW_OP(X) new (heap()) GraphicsOperation::X

#define USE_CLIPPING_PAINTER true

// Operations smaller than this area aren't considered opaque, and thus don't
// clip operations below. Chosen empirically.
#define MIN_TRACKED_OPAQUE_AREA 750

// Cap on ClippingPainter's recursive depth. Chosen empirically.
#define MAX_CLIPPING_RECURSION_COUNT 400

namespace WebCore {

static FloatRect approximateTextBounds(size_t numGlyphs,
    const SkPoint pos[], const SkPaint& paint)
{
    if (!numGlyphs || !pos) {
        return FloatRect();
    }

    // get glyph position bounds
    SkScalar minX = pos[0].x();
    SkScalar maxX = minX;
    SkScalar minY = pos[0].y();
    SkScalar maxY = minY;
    for (size_t i = 1; i < numGlyphs; ++i) {
        SkScalar x = pos[i].x();
        SkScalar y = pos[i].y();
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    // build final rect
    SkPaint::FontMetrics metrics;
    SkScalar bufY = paint.getFontMetrics(&metrics);
    SkScalar bufX = bufY * 2;
    SkScalar adjY = metrics.fAscent / 2;
    minY += adjY;
    maxY += adjY;
    SkRect rect;
    rect.set(minX - bufX, minY - bufY, maxX + bufX, maxY + bufY);
    return rect;
}

class StateHash {
public:
    static unsigned hash(PlatformGraphicsContext::State* const& state)
    {
        return StringHasher::hashMemory(state, sizeof(PlatformGraphicsContext::State));
    }

    static bool equal(PlatformGraphicsContext::State* const& a,
                      PlatformGraphicsContext::State* const& b)
    {
        return a && b && !memcmp(a, b, sizeof(PlatformGraphicsContext::State));
    }

    static const bool safeToCompareToEmptyOrDeleted = false;
};

class SkPaintHash {
public:
    static unsigned hash(const SkPaint* const& paint)
    {
        return StringHasher::hashMemory(paint, sizeof(SkPaint));
    }

    static bool equal(const SkPaint* const& a,
                      const SkPaint* const& b)
    {
        return a && b && (*a == *b);
    }

    static const bool safeToCompareToEmptyOrDeleted = false;
};

typedef HashSet<PlatformGraphicsContext::State*, StateHash> StateHashSet;
typedef HashSet<const SkPaint*, SkPaintHash> SkPaintHashSet;

class CanvasState {
public:
    CanvasState(CanvasState* parent)
        : m_parent(parent)
        , m_isTransparencyLayer(false)
    {}

    CanvasState(CanvasState* parent, float opacity)
        : m_parent(parent)
        , m_isTransparencyLayer(true)
        , m_opacity(opacity)
    {}

    ~CanvasState() {
        ALOGV("Delete %p", this);
        for (size_t i = 0; i < m_operations.size(); i++)
            m_operations[i]->~RecordingData();
        m_operations.clear();
    }

    bool isParentOf(CanvasState* other) {
        while (other->m_parent) {
            if (other->m_parent == this)
                return true;
            other = other->m_parent;
        }
        return false;
    }

    void playback(PlatformGraphicsContext* context, size_t fromId, size_t toId) const {
        ALOGV("playback %p from %d->%d", this, fromId, toId);
        for (size_t i = 0; i < m_operations.size(); i++) {
            RecordingData *data = m_operations[i];
            if (data->m_orderBy < fromId)
                continue;
            if (data->m_orderBy > toId)
                break;
            ALOGV("Applying operation[%d] %p->%s()", i, data->m_operation,
                  data->m_operation->name());
            data->m_operation->apply(context);
        }
    }

    CanvasState* parent() { return m_parent; }

    void enterState(PlatformGraphicsContext* context) {
        ALOGV("enterState %p", this);
        if (m_isTransparencyLayer)
            context->beginTransparencyLayer(m_opacity);
        else
            context->save();
    }

    void exitState(PlatformGraphicsContext* context) {
        ALOGV("exitState %p", this);
        if (m_isTransparencyLayer)
            context->endTransparencyLayer();
        else
            context->restore();
    }

    void adoptAndAppend(RecordingData* data) {
        m_operations.append(data);
    }

    bool isTransparencyLayer() {
        return m_isTransparencyLayer;
    }

    void* operator new(size_t size, android::LinearAllocator* la) {
        return la->alloc(size);
    }

private:
    CanvasState *m_parent;
    bool m_isTransparencyLayer;
    float m_opacity;
    Vector<RecordingData*> m_operations;
};

class RecordingImpl {
private:
    // Careful, ordering matters here. Ordering is first constructed == last destroyed,
    // so we have to make sure our Heap is the first thing listed so that it is
    // the last thing destroyed.
    android::LinearAllocator m_heap;
public:
    RecordingImpl()
        : m_tree(&m_heap)
        , m_nodeCount(0)
    {
    }

    ~RecordingImpl() {
        clearStates();
        clearCanvasStates();
        clearSkPaints();
    }

    PlatformGraphicsContext::State* getState(PlatformGraphicsContext::State* inState) {
        StateHashSet::iterator it = m_states.find(inState);
        if (it != m_states.end())
            return (*it);
        void* buf = heap()->alloc(sizeof(PlatformGraphicsContext::State));
        PlatformGraphicsContext::State* state = new (buf) PlatformGraphicsContext::State(*inState);
        m_states.add(state);
        return state;
    }

    const SkPaint* getSkPaint(const SkPaint& inPaint) {
        SkPaintHashSet::iterator it = m_paints.find(&inPaint);
        if (it != m_paints.end())
            return (*it);
        void* buf = heap()->alloc(sizeof(SkPaint));
        SkPaint* paint = new (buf) SkPaint(inPaint);
        m_paints.add(paint);
        return paint;
    }

    void addCanvasState(CanvasState* state) {
        m_canvasStates.append(state);
    }

    void removeCanvasState(const CanvasState* state) {
        if (m_canvasStates.last() == state)
            m_canvasStates.removeLast();
        else {
            size_t indx = m_canvasStates.find(state);
            m_canvasStates.remove(indx);
        }
    }

    void applyState(PlatformGraphicsContext* context,
                    CanvasState* fromState, size_t fromId,
                    CanvasState* toState, size_t toId) {
        ALOGV("applyState(%p->%p, %d-%d)", fromState, toState, fromId, toId);
        if (fromState != toState && fromState) {
            if (fromState->isParentOf(toState)) {
                // Going down the tree, playback any parent operations then save
                // before playing back our current operations
                applyState(context, fromState, fromId, toState->parent(), toId);
                toState->enterState(context);
            } else if (toState->isParentOf(fromState)) {
                // Going up the tree, pop some states
                while (fromState != toState) {
                    fromState->exitState(context);
                    fromState = fromState->parent();
                }
            } else {
                // Siblings in the tree
                fromState->exitState(context);
                applyState(context, fromState->parent(), fromId, toState, toId);
                return;
            }
        } else if (!fromState) {
            if (toState->parent())
                applyState(context, fromState, fromId, toState->parent(), toId);
            toState->enterState(context);
        }
        toState->playback(context, fromId, toId);
    }

    android::LinearAllocator* heap() { return &m_heap; }

    RTree::RTree m_tree;
    int m_nodeCount;

    void dumpMemoryStats() {
        static const char* PREFIX = "  ";
        ALOGD("Heap:");
        m_heap.dumpMemoryStats(PREFIX);
    }

private:

    void clearStates() {
        StateHashSet::iterator end = m_states.end();
        for (StateHashSet::iterator it = m_states.begin(); it != end; ++it)
            (*it)->~State();
        m_states.clear();
    }

    void clearSkPaints() {
        SkPaintHashSet::iterator end = m_paints.end();
        for (SkPaintHashSet::iterator it = m_paints.begin(); it != end; ++it)
            (*it)->~SkPaint();
        m_paints.clear();
    }

    void clearCanvasStates() {
        for (size_t i = 0; i < m_canvasStates.size(); i++)
            m_canvasStates[i]->~CanvasState();
        m_canvasStates.clear();
    }

    StateHashSet m_states;
    SkPaintHashSet m_paints;
    Vector<CanvasState*> m_canvasStates;
};

Recording::~Recording()
{
    delete m_recording;
}

static bool CompareRecordingDataOrder(const RecordingData* a, const RecordingData* b)
{
    return a->m_orderBy < b->m_orderBy;
}

static IntRect enclosedIntRect(const FloatRect& rect)
{
    float left = ceilf(rect.x());
    float top = ceilf(rect.y());
    float width = floorf(rect.maxX()) - left;
    float height = floorf(rect.maxY()) - top;

    return IntRect(clampToInteger(left), clampToInteger(top),
                   clampToInteger(width), clampToInteger(height));
}

#if USE_CLIPPING_PAINTER
class ClippingPainter {
public:
    ClippingPainter(RecordingImpl* recording,
                    PlatformGraphicsContextSkia& context,
                    const SkMatrix& initialMatrix,
                    Vector<RecordingData*> &nodes)
        : m_recording(recording)
        , m_context(context)
        , m_initialMatrix(initialMatrix)
        , m_nodes(nodes)
        , m_lastOperationId(0)
        , m_currState(0)
    {}

    void draw(const SkIRect& bounds) {
        drawWithClipRecursive(static_cast<int>(m_nodes.size()) - 1, bounds, 0);

        while (m_currState) {
            m_currState->exitState(&m_context);
            m_currState = m_currState->parent();
        }
    }

private:
    void drawOperation(RecordingData* node, const SkRegion* uncovered)
    {
        GraphicsOperation::Operation* op = node->m_operation;
        m_recording->applyState(&m_context, m_currState,
                                m_lastOperationId, op->m_canvasState, node->m_orderBy);
        m_currState = op->m_canvasState;
        m_lastOperationId = node->m_orderBy;

        // if other opaque operations will cover the current one, clip that area out
        // (and restore the clip immediately after drawing)
        if (uncovered) {
            m_context.save();
            m_context.canvas()->clipRegion(*uncovered, SkRegion::kIntersect_Op);
        }
        op->apply(&(m_context));
        if (uncovered)
            m_context.restore();
    }

    void drawWithClipRecursive(int index, const SkIRect& bounds, const SkRegion* uncovered)
    {
        if (index < 0)
            return;
        RecordingData* recordingData = m_nodes[index];
        GraphicsOperation::Operation* op = recordingData->m_operation;
        if (index != 0) {
            const IntRect* opaqueRect = op->opaqueRect();
            if (!opaqueRect || opaqueRect->isEmpty()) {
                drawWithClipRecursive(index - 1, bounds, uncovered);
            } else {
                SkRegion newUncovered;
                if (uncovered)
                    newUncovered = *uncovered;
                else
                    newUncovered = SkRegion(bounds);

                SkRect mappedRect = *opaqueRect;
                m_initialMatrix.mapRect(&mappedRect);
                newUncovered.op(enclosedIntRect(mappedRect), SkRegion::kDifference_Op);
                if (!newUncovered.isEmpty())
                    drawWithClipRecursive(index - 1, bounds, &newUncovered);
            }
        }

        if (!uncovered || !uncovered->isEmpty())
            drawOperation(recordingData, uncovered);
    }

    RecordingImpl* m_recording;
    PlatformGraphicsContextSkia& m_context;
    const SkMatrix& m_initialMatrix;
    const Vector<RecordingData*>& m_nodes;
    size_t m_lastOperationId;
    CanvasState* m_currState;
};
#endif // USE_CLIPPING_PAINTER

void Recording::draw(SkCanvas* canvas)
{
    if (!m_recording) {
        ALOGW("No recording!");
        return;
    }
    SkRect clip;
    if (!canvas->getClipBounds(&clip)) {
        ALOGW("Empty clip!");
        return;
    }
    Vector<RecordingData*> nodes;

    WebCore::IntRect iclip = enclosingIntRect(clip);
    m_recording->m_tree.search(iclip, nodes);

    size_t count = nodes.size();
    ALOGV("Drawing %d nodes out of %d", count, m_recording->m_nodeCount);
    if (count) {
        int saveCount = canvas->getSaveCount();
        nonCopyingSort(nodes.begin(), nodes.end(), CompareRecordingDataOrder);
        PlatformGraphicsContextSkia context(canvas);
#if USE_CLIPPING_PAINTER
        if (canvas->getDevice() && canvas->getDevice()->config() != SkBitmap::kNo_Config
            && count < MAX_CLIPPING_RECURSION_COUNT) {
            ClippingPainter painter(recording(), context, canvas->getTotalMatrix(), nodes);
            painter.draw(canvas->getTotalClip().getBounds());
        } else
#endif
        {
            CanvasState* currState = 0;
            size_t lastOperationId = 0;
            for (size_t i = 0; i < count; i++) {
                GraphicsOperation::Operation* op = nodes[i]->m_operation;
                m_recording->applyState(&context, currState, lastOperationId,
                                        op->m_canvasState, nodes[i]->m_orderBy);
                currState = op->m_canvasState;
                lastOperationId = nodes[i]->m_orderBy;
                ALOGV("apply: %p->%s()", op, op->name());
                op->apply(&context);
            }
            while (currState) {
                currState->exitState(&context);
                currState = currState->parent();
            }
        }
        if (saveCount != canvas->getSaveCount()) {
            ALOGW("Save/restore mismatch! %d vs. %d", saveCount, canvas->getSaveCount());
        }
    }
}

void Recording::setRecording(RecordingImpl* impl)
{
    if (m_recording == impl)
        return;
    if (m_recording)
        delete m_recording;
    m_recording = impl;
}

//**************************************
// PlatformGraphicsContextRecording
//**************************************

PlatformGraphicsContextRecording::PlatformGraphicsContextRecording(Recording* recording)
    : PlatformGraphicsContext()
    , mPicture(0)
    , mRecording(recording)
    , mOperationState(0)
    , m_maxZoomScale(1)
    , m_isEmpty(true)
    , m_canvasProxy(this)
{
    ALOGV("RECORDING: begin");
    if (mRecording)
        mRecording->setRecording(new RecordingImpl());
    mMatrixStack.append(SkMatrix::I());
    mCurrentMatrix = &(mMatrixStack.last());
    pushStateOperation(new (heap()) CanvasState(0));
}

PlatformGraphicsContextRecording::~PlatformGraphicsContextRecording()
{
    ALOGV("RECORDING: end");
    IF_ALOGV()
        mRecording->recording()->dumpMemoryStats();
}

bool PlatformGraphicsContextRecording::isPaintingDisabled()
{
    return !mRecording;
}

SkCanvas* PlatformGraphicsContextRecording::recordingCanvas()
{
    m_maxZoomScale = 1e6f;
    return &m_canvasProxy;
}

//**************************************
// State management
//**************************************

void PlatformGraphicsContextRecording::beginTransparencyLayer(float opacity)
{
    CanvasState* parent = mRecordingStateStack.last().mCanvasState;
    pushStateOperation(new (heap()) CanvasState(parent, opacity));
    mRecordingStateStack.last().disableOpaqueTracking();
}

void PlatformGraphicsContextRecording::endTransparencyLayer()
{
    popStateOperation();
}

void PlatformGraphicsContextRecording::save()
{
    PlatformGraphicsContext::save();
    CanvasState* parent = mRecordingStateStack.last().mCanvasState;
    pushStateOperation(new (heap()) CanvasState(parent));
    pushMatrix();
}

void PlatformGraphicsContextRecording::restore()
{
    PlatformGraphicsContext::restore();
    popMatrix();
    popStateOperation();
}

//**************************************
// State setters
//**************************************

void PlatformGraphicsContextRecording::setAlpha(float alpha)
{
    PlatformGraphicsContext::setAlpha(alpha);
    mOperationState = 0;
}

void PlatformGraphicsContextRecording::setCompositeOperation(CompositeOperator op)
{
    PlatformGraphicsContext::setCompositeOperation(op);
    mOperationState = 0;
}

bool PlatformGraphicsContextRecording::setFillColor(const Color& c)
{
    if (PlatformGraphicsContext::setFillColor(c)) {
        mOperationState = 0;
        return true;
    }
    return false;
}

bool PlatformGraphicsContextRecording::setFillShader(SkShader* fillShader)
{
    if (PlatformGraphicsContext::setFillShader(fillShader)) {
        mOperationState = 0;
        return true;
    }
    return false;
}

void PlatformGraphicsContextRecording::setLineCap(LineCap cap)
{
    PlatformGraphicsContext::setLineCap(cap);
    mOperationState = 0;
}

void PlatformGraphicsContextRecording::setLineDash(const DashArray& dashes, float dashOffset)
{
    PlatformGraphicsContext::setLineDash(dashes, dashOffset);
    mOperationState = 0;
}

void PlatformGraphicsContextRecording::setLineJoin(LineJoin join)
{
    PlatformGraphicsContext::setLineJoin(join);
    mOperationState = 0;
}

void PlatformGraphicsContextRecording::setMiterLimit(float limit)
{
    PlatformGraphicsContext::setMiterLimit(limit);
    mOperationState = 0;
}

void PlatformGraphicsContextRecording::setShadow(int radius, int dx, int dy, SkColor c)
{
    PlatformGraphicsContext::setShadow(radius, dx, dy, c);
    mOperationState = 0;
}

void PlatformGraphicsContextRecording::setShouldAntialias(bool useAA)
{
    m_state->useAA = useAA;
    PlatformGraphicsContext::setShouldAntialias(useAA);
    mOperationState = 0;
}

bool PlatformGraphicsContextRecording::setStrokeColor(const Color& c)
{
    if (PlatformGraphicsContext::setStrokeColor(c)) {
        mOperationState = 0;
        return true;
    }
    return false;
}

bool PlatformGraphicsContextRecording::setStrokeShader(SkShader* strokeShader)
{
    if (PlatformGraphicsContext::setStrokeShader(strokeShader)) {
        mOperationState = 0;
        return true;
    }
    return false;
}

void PlatformGraphicsContextRecording::setStrokeStyle(StrokeStyle style)
{
    PlatformGraphicsContext::setStrokeStyle(style);
    mOperationState = 0;
}

void PlatformGraphicsContextRecording::setStrokeThickness(float f)
{
    PlatformGraphicsContext::setStrokeThickness(f);
    mOperationState = 0;
}

//**************************************
// Matrix operations
//**************************************

void PlatformGraphicsContextRecording::concatCTM(const AffineTransform& affine)
{
    mCurrentMatrix->preConcat(affine);
    appendStateOperation(NEW_OP(ConcatCTM)(affine));
}

void PlatformGraphicsContextRecording::rotate(float angleInRadians)
{
    float value = angleInRadians * (180.0f / 3.14159265f);
    mCurrentMatrix->preRotate(SkFloatToScalar(value));
    appendStateOperation(NEW_OP(Rotate)(angleInRadians));
}

void PlatformGraphicsContextRecording::scale(const FloatSize& size)
{
    mCurrentMatrix->preScale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
    appendStateOperation(NEW_OP(Scale)(size));
}

void PlatformGraphicsContextRecording::translate(float x, float y)
{
    mCurrentMatrix->preTranslate(SkFloatToScalar(x), SkFloatToScalar(y));
    appendStateOperation(NEW_OP(Translate)(x, y));
}

const SkMatrix& PlatformGraphicsContextRecording::getTotalMatrix()
{
    return *mCurrentMatrix;
}

//**************************************
// Clipping
//**************************************

void PlatformGraphicsContextRecording::addInnerRoundedRectClip(const IntRect& rect,
                                                      int thickness)
{
    mRecordingStateStack.last().disableOpaqueTracking();
    appendStateOperation(NEW_OP(InnerRoundedRectClip)(rect, thickness));
}

void PlatformGraphicsContextRecording::canvasClip(const Path& path)
{
    mRecordingStateStack.last().disableOpaqueTracking();
    clip(path);
}

bool PlatformGraphicsContextRecording::clip(const FloatRect& rect)
{
    clipState(rect);
    appendStateOperation(NEW_OP(Clip)(rect));
    return true;
}

bool PlatformGraphicsContextRecording::clip(const Path& path)
{
    mRecordingStateStack.last().disableOpaqueTracking();
    clipState(path.boundingRect());
    appendStateOperation(NEW_OP(ClipPath)(path));
    return true;
}

bool PlatformGraphicsContextRecording::clipConvexPolygon(size_t numPoints,
                                                const FloatPoint*, bool antialias)
{
    // TODO
    return true;
}

bool PlatformGraphicsContextRecording::clipOut(const IntRect& r)
{
    mRecordingStateStack.last().disableOpaqueTracking();
    appendStateOperation(NEW_OP(ClipOut)(r));
    return true;
}

bool PlatformGraphicsContextRecording::clipOut(const Path& path)
{
    mRecordingStateStack.last().disableOpaqueTracking();
    appendStateOperation(NEW_OP(ClipPath)(path, true));
    return true;
}

bool PlatformGraphicsContextRecording::clipPath(const Path& pathToClip, WindRule clipRule)
{
    mRecordingStateStack.last().disableOpaqueTracking();
    clipState(pathToClip.boundingRect());
    GraphicsOperation::ClipPath* operation = NEW_OP(ClipPath)(pathToClip);
    operation->setWindRule(clipRule);
    appendStateOperation(operation);
    return true;
}

void PlatformGraphicsContextRecording::clearRect(const FloatRect& rect)
{
    appendDrawingOperation(NEW_OP(ClearRect)(rect), rect);
}

//**************************************
// Drawing
//**************************************

void PlatformGraphicsContextRecording::drawBitmapPattern(
        const SkBitmap& bitmap, const SkMatrix& matrix,
        CompositeOperator compositeOp, const FloatRect& destRect)
{
    appendDrawingOperation(
            NEW_OP(DrawBitmapPattern)(bitmap, matrix, compositeOp, destRect),
            destRect);
}

void PlatformGraphicsContextRecording::drawBitmapRect(const SkBitmap& bitmap,
                                   const SkIRect* srcPtr, const SkRect& dst,
                                   CompositeOperator op)
{
    float widthScale = dst.width() == 0 ? 1 : bitmap.width() / dst.width();
    float heightScale = dst.height() == 0 ? 1 : bitmap.height() / dst.height();
    m_maxZoomScale = std::max(m_maxZoomScale, std::max(widthScale, heightScale));
    // null src implies full bitmap as source rect
    SkIRect src = srcPtr ? *srcPtr : SkIRect::MakeWH(bitmap.width(), bitmap.height());
    appendDrawingOperation(NEW_OP(DrawBitmapRect)(bitmap, src, dst, op), dst);
}

void PlatformGraphicsContextRecording::drawConvexPolygon(size_t numPoints,
                                                const FloatPoint* points,
                                                bool shouldAntialias)
{
    if (numPoints < 1) return;
    if (numPoints != 4) {
        // TODO: Build a path and call draw on that (webkit currently never calls this)
        ALOGW("drawConvexPolygon with numPoints != 4 is not supported!");
        return;
    }
    FloatRect bounds;
    bounds.fitToPoints(points[0], points[1], points[2], points[3]);
    appendDrawingOperation(NEW_OP(DrawConvexPolygonQuad)(points, shouldAntialias), bounds);
}

void PlatformGraphicsContextRecording::drawEllipse(const IntRect& rect)
{
    appendDrawingOperation(NEW_OP(DrawEllipse)(rect), rect);
}

void PlatformGraphicsContextRecording::drawFocusRing(const Vector<IntRect>& rects,
                                            int width, int offset,
                                            const Color& color)
{
    if (!rects.size())
        return;
    IntRect bounds = rects[0];
    for (size_t i = 1; i < rects.size(); i++)
        bounds.unite(rects[i]);
    appendDrawingOperation(NEW_OP(DrawFocusRing)(rects, width, offset, color), bounds);
}

void PlatformGraphicsContextRecording::drawHighlightForText(
        const Font& font, const TextRun& run, const FloatPoint& point, int h,
        const Color& backgroundColor, ColorSpace colorSpace, int from,
        int to, bool isActive)
{
    IntRect rect = (IntRect)font.selectionRectForText(run, point, h, from, to);
    if (isActive)
        fillRect(rect, backgroundColor);
    else {
        int x = rect.x(), y = rect.y(), w = rect.width(), h = rect.height();
        const int t = 3, t2 = t * 2;

        fillRect(IntRect(x, y, w, t), backgroundColor);
        fillRect(IntRect(x, y+h-t, w, t), backgroundColor);
        fillRect(IntRect(x, y+t, t, h-t2), backgroundColor);
        fillRect(IntRect(x+w-t, y+t, t, h-t2), backgroundColor);
    }
}

void PlatformGraphicsContextRecording::drawLine(const IntPoint& point1,
                             const IntPoint& point2)
{
    FloatRect bounds = FloatQuad(point1, point1, point2, point2).boundingBox();
    float width = m_state->strokeThickness;
    if (!width) width = 1;
    bounds.inflate(width);
    appendDrawingOperation(NEW_OP(DrawLine)(point1, point2), bounds);
}

void PlatformGraphicsContextRecording::drawLineForText(const FloatPoint& pt, float width)
{
    FloatRect bounds(pt.x(), pt.y(), width, m_state->strokeThickness);
    appendDrawingOperation(NEW_OP(DrawLineForText)(pt, width), bounds);
}

void PlatformGraphicsContextRecording::drawLineForTextChecking(const FloatPoint& pt,
        float width, GraphicsContext::TextCheckingLineStyle lineStyle)
{
    FloatRect bounds(pt.x(), pt.y(), width, m_state->strokeThickness);
    appendDrawingOperation(NEW_OP(DrawLineForTextChecking)(pt, width, lineStyle), bounds);
}

void PlatformGraphicsContextRecording::drawRect(const IntRect& rect)
{
    appendDrawingOperation(NEW_OP(DrawRect)(rect), rect);
}

void PlatformGraphicsContextRecording::fillPath(const Path& pathToFill, WindRule fillRule)
{
    appendDrawingOperation(NEW_OP(FillPath)(pathToFill, fillRule), pathToFill.boundingRect());
}

void PlatformGraphicsContextRecording::fillRect(const FloatRect& rect)
{
    appendDrawingOperation(NEW_OP(FillRect)(rect), rect);
}

void PlatformGraphicsContextRecording::fillRect(const FloatRect& rect,
                                       const Color& color)
{
    GraphicsOperation::FillRect* operation = NEW_OP(FillRect)(rect);
    operation->setColor(color);
    appendDrawingOperation(operation, rect);
}

void PlatformGraphicsContextRecording::fillRoundedRect(
        const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
        const IntSize& bottomLeft, const IntSize& bottomRight,
        const Color& color)
{
    appendDrawingOperation(NEW_OP(FillRoundedRect)(rect, topLeft,
                 topRight, bottomLeft, bottomRight, color), rect);
}

void PlatformGraphicsContextRecording::strokeArc(const IntRect& r, int startAngle,
                              int angleSpan)
{
    appendDrawingOperation(NEW_OP(StrokeArc)(r, startAngle, angleSpan), r);
}

void PlatformGraphicsContextRecording::strokePath(const Path& pathToStroke)
{
    appendDrawingOperation(NEW_OP(StrokePath)(pathToStroke), pathToStroke.boundingRect());
}

void PlatformGraphicsContextRecording::strokeRect(const FloatRect& rect, float lineWidth)
{
    FloatRect bounds = rect;
    bounds.inflate(lineWidth);
    appendDrawingOperation(NEW_OP(StrokeRect)(rect, lineWidth), bounds);
}

void PlatformGraphicsContextRecording::drawPosText(const void* inText, size_t byteLength,
                                                   const SkPoint inPos[], const SkPaint& inPaint)
{
    if (inPaint.getTextEncoding() != SkPaint::kGlyphID_TextEncoding) {
        ALOGE("Unsupported text encoding! %d", inPaint.getTextEncoding());
        return;
    }
    FloatRect bounds = approximateTextBounds(byteLength / sizeof(uint16_t), inPos, inPaint);
    bounds.move(m_textOffset); // compensate font rendering-side translates

    const SkPaint* paint = mRecording->recording()->getSkPaint(inPaint);
    size_t posSize = sizeof(SkPoint) * paint->countText(inText, byteLength);
    void* text = heap()->alloc(byteLength);
    SkPoint* pos = (SkPoint*) heap()->alloc(posSize);
    memcpy(text, inText, byteLength);
    memcpy(pos, inPos, posSize);
    appendDrawingOperation(NEW_OP(DrawPosText)(text, byteLength, pos, paint), bounds);
}

void PlatformGraphicsContextRecording::drawMediaButton(const IntRect& rect, RenderSkinMediaButton::MediaButton buttonType,
                                                       bool translucent, bool drawBackground,
                                                       const IntRect& thumb)
{
    appendDrawingOperation(NEW_OP(DrawMediaButton)(rect, buttonType,
            translucent, drawBackground, thumb), rect);
}

void PlatformGraphicsContextRecording::clipState(const FloatRect& clip)
{
    if (mRecordingStateStack.size()) {
        SkRect mapBounds;
        mCurrentMatrix->mapRect(&mapBounds, clip);
        mRecordingStateStack.last().clip(mapBounds);
    }
}

void PlatformGraphicsContextRecording::pushStateOperation(CanvasState* canvasState)
{
    ALOGV("RECORDING: pushStateOperation: %p(isLayer=%d)", canvasState, canvasState->isTransparencyLayer());

    RecordingState* parent = mRecordingStateStack.isEmpty() ? 0 : &(mRecordingStateStack.last());
    mRecordingStateStack.append(RecordingState(canvasState, parent));
    mRecording->recording()->addCanvasState(canvasState);
}

void PlatformGraphicsContextRecording::popStateOperation()
{
    RecordingState state = mRecordingStateStack.last();
    mRecordingStateStack.removeLast();
    mOperationState = 0;
    if (!state.mHasDrawing) {
        ALOGV("RECORDING: popStateOperation is deleting %p(isLayer=%d)",
                state.mCanvasState, state.mCanvasState->isTransparencyLayer());
        mRecording->recording()->removeCanvasState(state.mCanvasState);
        state.mCanvasState->~CanvasState();
        heap()->rewindIfLastAlloc(state.mCanvasState, sizeof(CanvasState));
    } else {
        ALOGV("RECORDING: popStateOperation: %p(isLayer=%d)",
                state.mCanvasState, state.mCanvasState->isTransparencyLayer());
        // Make sure we propagate drawing upwards so we don't delete our parent
        mRecordingStateStack.last().mHasDrawing = true;
    }
}

void PlatformGraphicsContextRecording::pushMatrix()
{
    mMatrixStack.append(mMatrixStack.last());
    mCurrentMatrix = &(mMatrixStack.last());
}

void PlatformGraphicsContextRecording::popMatrix()
{
    mMatrixStack.removeLast();
    mCurrentMatrix = &(mMatrixStack.last());
}

IntRect PlatformGraphicsContextRecording::calculateFinalBounds(FloatRect bounds)
{
    if (bounds.isEmpty() && mRecordingStateStack.last().mHasClip) {
        ALOGV("Empty bounds, but has clip so using that");
        return enclosingIntRect(mRecordingStateStack.last().mBounds);
    }
    if (m_gc->hasShadow()) {
        const ShadowRec& shadow = m_state->shadow;
        if (shadow.blur > 0)
            bounds.inflate(ceilf(shadow.blur));
        bounds.setWidth(bounds.width() + abs(shadow.dx));
        bounds.setHeight(bounds.height() + abs(shadow.dy));
        if (shadow.dx < 0)
            bounds.move(shadow.dx, 0);
        if (shadow.dy < 0)
            bounds.move(0, shadow.dy);
        // Add a bit extra to deal with rounding and blurring
        bounds.inflate(4);
    }
    if (m_state->strokeStyle != NoStroke)
        bounds.inflate(std::min(1.0f, m_state->strokeThickness));
    SkRect translated;
    mCurrentMatrix->mapRect(&translated, bounds);
    FloatRect ftrect = translated;
    if (mRecordingStateStack.last().mHasClip
            && !translated.intersect(mRecordingStateStack.last().mBounds)) {
        ALOGV("Operation bounds=" FLOAT_RECT_FORMAT " clipped out by clip=" FLOAT_RECT_FORMAT,
                FLOAT_RECT_ARGS(ftrect), FLOAT_RECT_ARGS(mRecordingStateStack.last().mBounds));
        return IntRect();
    }
    return enclosingIntRect(translated);
}

IntRect PlatformGraphicsContextRecording::calculateCoveredBounds(FloatRect bounds)
{
    if (mRecordingStateStack.last().mOpaqueTrackingDisabled
        || m_state->alpha != 1.0f
        || (m_state->fillShader != 0 && !m_state->fillShader->isOpaque())
        || (m_state->mode != SkXfermode::kSrc_Mode && m_state->mode != SkXfermode::kSrcOver_Mode)
        || !mCurrentMatrix->rectStaysRect()) {
        return IntRect();
    }

    SkRect translated;
    mCurrentMatrix->mapRect(&translated, bounds);
    FloatRect ftrect = translated;
    if (mRecordingStateStack.last().mHasClip
            && !translated.intersect(mRecordingStateStack.last().mBounds)) {
        ALOGV("Operation opaque area=" FLOAT_RECT_FORMAT " clipped out by clip=" FLOAT_RECT_FORMAT,
                FLOAT_RECT_ARGS(ftrect), FLOAT_RECT_ARGS(mRecordingStateStack.last().mBounds));
        return IntRect();
    }
    return enclosedIntRect(translated);
}

void PlatformGraphicsContextRecording::appendDrawingOperation(
        GraphicsOperation::Operation* operation, const FloatRect& untranslatedBounds)
{
    m_isEmpty = false;
    RecordingState& state = mRecordingStateStack.last();
    state.mHasDrawing = true;
    if (!mOperationState)
        mOperationState = mRecording->recording()->getState(m_state);
    operation->m_state = mOperationState;
    operation->m_canvasState = state.mCanvasState;

    WebCore::IntRect ibounds = calculateFinalBounds(untranslatedBounds);
    if (ibounds.isEmpty()) {
        ALOGV("RECORDING: Operation %s() was clipped out", operation->name());
        operation->~Operation();
        return;
    }
#if USE_CLIPPING_PAINTER
    if (operation->isOpaque()
        && !untranslatedBounds.isEmpty()
        && (untranslatedBounds.width() * untranslatedBounds.height() > MIN_TRACKED_OPAQUE_AREA)) {
        // if the operation maps to an opaque rect, record the area it will cover
        operation->setOpaqueRect(calculateCoveredBounds(untranslatedBounds));
    }
#endif
    ALOGV("RECORDING: appendOperation %p->%s() bounds " INT_RECT_FORMAT, operation, operation->name(),
            INT_RECT_ARGS(ibounds));
    RecordingData* data = new (heap()) RecordingData(operation, mRecording->recording()->m_nodeCount++);
    mRecording->recording()->m_tree.insert(ibounds, data);
}

void PlatformGraphicsContextRecording::appendStateOperation(GraphicsOperation::Operation* operation)
{
    ALOGV("RECORDING: appendOperation %p->%s()", operation, operation->name());
    RecordingData* data = new (heap()) RecordingData(operation, mRecording->recording()->m_nodeCount++);
    mRecordingStateStack.last().mCanvasState->adoptAndAppend(data);
}

android::LinearAllocator* PlatformGraphicsContextRecording::heap()
{
    return mRecording->recording()->heap();
}

}   // WebCore
