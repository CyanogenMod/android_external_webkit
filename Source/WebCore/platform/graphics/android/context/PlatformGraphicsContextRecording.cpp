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

#include "wtf/NonCopyingSort.h"
#include "wtf/HashSet.h"
#include "wtf/StringHasher.h"

namespace WebCore {

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

typedef HashSet<PlatformGraphicsContext::State*, StateHash> StateHashSet;

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
            delete m_operations[i];
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

private:
    CanvasState *m_parent;
    bool m_isTransparencyLayer;
    float m_opacity;
    Vector<RecordingData*> m_operations;
};

class RecordingImpl {
public:
    RecordingImpl()
        : m_nodeCount(0)
    {
    }

    ~RecordingImpl() {
        clearStates();
        clearCanvasStates();
    }

    PlatformGraphicsContext::State* getState(PlatformGraphicsContext::State* inState) {
        StateHashSet::iterator it = m_states.find(inState);
        if (it != m_states.end())
            return (*it);
        // TODO: Use a custom allocator
        PlatformGraphicsContext::State* state = new PlatformGraphicsContext::State(*inState);
        m_states.add(state);
        return state;
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

    RTree::RTree m_tree;
    int m_nodeCount;

private:

    void clearStates() {
        StateHashSet::iterator end = m_states.end();
        for (StateHashSet::iterator it = m_states.begin(); it != end; ++it)
            delete (*it);
        m_states.clear();
    }

    void clearCanvasStates() {
        for (size_t i = 0; i < m_canvasStates.size(); i++)
            delete m_canvasStates[i];
        m_canvasStates.clear();
    }

    // TODO: Use a global pool?
    StateHashSet m_states;
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
        CanvasState* currState = 0;
        size_t lastOperationId = 0;
        for (size_t i = 0; i < count; i++) {
            GraphicsOperation::Operation* op = nodes[i]->m_operation;
            m_recording->applyState(&context, currState, lastOperationId,
                    op->m_canvasState, nodes[i]->m_orderBy);
            currState = op->m_canvasState;
            lastOperationId = nodes[i]->m_orderBy;
            ALOGV("apply: %p->%s(%s)", op, op->name(), op->parameters().ascii().data());
            op->apply(&context);
        }
        while (currState) {
            currState->exitState(&context);
            currState = currState->parent();
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
    , m_hasText(false)
    , m_isEmpty(true)
{
    if (mRecording)
        mRecording->setRecording(new RecordingImpl());
    mMatrixStack.append(SkMatrix::I());
    mCurrentMatrix = &(mMatrixStack.last());
    pushStateOperation(new CanvasState(0));
}

bool PlatformGraphicsContextRecording::isPaintingDisabled()
{
    return !mRecording;
}

SkCanvas* PlatformGraphicsContextRecording::recordingCanvas()
{
    SkSafeUnref(mPicture);
    mPicture = new SkPicture();
    return mPicture->beginRecording(0, 0, 0);
}

void PlatformGraphicsContextRecording::endRecording(const SkRect& bounds)
{
    if (!mPicture)
        return;
    mPicture->endRecording();
    GraphicsOperation::DrawComplexText* text = new GraphicsOperation::DrawComplexText(mPicture);
    appendDrawingOperation(text, bounds);
    mPicture = 0;

    m_hasText = true;
}

//**************************************
// State management
//**************************************

void PlatformGraphicsContextRecording::beginTransparencyLayer(float opacity)
{
    CanvasState* parent = mRecordingStateStack.last().mCanvasState;
    pushStateOperation(new CanvasState(parent, opacity));
}

void PlatformGraphicsContextRecording::endTransparencyLayer()
{
    popStateOperation();
}

void PlatformGraphicsContextRecording::save()
{
    PlatformGraphicsContext::save();
    CanvasState* parent = mRecordingStateStack.last().mCanvasState;
    pushStateOperation(new CanvasState(parent));
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
    appendStateOperation(new GraphicsOperation::ConcatCTM(affine));
}

void PlatformGraphicsContextRecording::rotate(float angleInRadians)
{
    float value = angleInRadians * (180.0f / 3.14159265f);
    mCurrentMatrix->preRotate(SkFloatToScalar(value));
    appendStateOperation(new GraphicsOperation::Rotate(angleInRadians));
}

void PlatformGraphicsContextRecording::scale(const FloatSize& size)
{
    mCurrentMatrix->preScale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
    appendStateOperation(new GraphicsOperation::Scale(size));
}

void PlatformGraphicsContextRecording::translate(float x, float y)
{
    mCurrentMatrix->preTranslate(SkFloatToScalar(x), SkFloatToScalar(y));
    appendStateOperation(new GraphicsOperation::Translate(x, y));
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
    appendStateOperation(new GraphicsOperation::InnerRoundedRectClip(rect, thickness));
}

void PlatformGraphicsContextRecording::canvasClip(const Path& path)
{
    clip(path);
}

bool PlatformGraphicsContextRecording::clip(const FloatRect& rect)
{
    clipState(rect);
    appendStateOperation(new GraphicsOperation::Clip(rect));
    return true;
}

bool PlatformGraphicsContextRecording::clip(const Path& path)
{
    clipState(path.boundingRect());
    appendStateOperation(new GraphicsOperation::ClipPath(path));
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
    appendStateOperation(new GraphicsOperation::ClipOut(r));
    return true;
}

bool PlatformGraphicsContextRecording::clipOut(const Path& path)
{
    appendStateOperation(new GraphicsOperation::ClipPath(path, true));
    return true;
}

bool PlatformGraphicsContextRecording::clipPath(const Path& pathToClip, WindRule clipRule)
{
    clipState(pathToClip.boundingRect());
    GraphicsOperation::ClipPath* operation = new GraphicsOperation::ClipPath(pathToClip);
    operation->setWindRule(clipRule);
    appendStateOperation(operation);
    return true;
}

void PlatformGraphicsContextRecording::clearRect(const FloatRect& rect)
{
    appendDrawingOperation(new GraphicsOperation::ClearRect(rect), rect);
}

//**************************************
// Drawing
//**************************************

void PlatformGraphicsContextRecording::drawBitmapPattern(
        const SkBitmap& bitmap, const SkMatrix& matrix,
        CompositeOperator compositeOp, const FloatRect& destRect)
{
    appendDrawingOperation(
            new GraphicsOperation::DrawBitmapPattern(bitmap, matrix, compositeOp, destRect),
            destRect);
}

void PlatformGraphicsContextRecording::drawBitmapRect(const SkBitmap& bitmap,
                                   const SkIRect* src, const SkRect& dst,
                                   CompositeOperator op)
{
    appendDrawingOperation(new GraphicsOperation::DrawBitmapRect(bitmap, *src, dst, op), dst);
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
    appendDrawingOperation(new GraphicsOperation::DrawConvexPolygonQuad(points, shouldAntialias), bounds);
}

void PlatformGraphicsContextRecording::drawEllipse(const IntRect& rect)
{
    appendDrawingOperation(new GraphicsOperation::DrawEllipse(rect), rect);
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
    appendDrawingOperation(new GraphicsOperation::DrawFocusRing(rects, width, offset, color), bounds);
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
    appendDrawingOperation(new GraphicsOperation::DrawLine(point1, point2), bounds);
}

void PlatformGraphicsContextRecording::drawLineForText(const FloatPoint& pt, float width)
{
    FloatRect bounds(pt.x(), pt.y(), width, m_state->strokeThickness);
    appendDrawingOperation(new GraphicsOperation::DrawLineForText(pt, width), bounds);
}

void PlatformGraphicsContextRecording::drawLineForTextChecking(const FloatPoint& pt,
        float width, GraphicsContext::TextCheckingLineStyle lineStyle)
{
    FloatRect bounds(pt.x(), pt.y(), width, m_state->strokeThickness);
    appendDrawingOperation(new GraphicsOperation::DrawLineForTextChecking(pt, width, lineStyle), bounds);
}

void PlatformGraphicsContextRecording::drawRect(const IntRect& rect)
{
    appendDrawingOperation(new GraphicsOperation::DrawRect(rect), rect);
}

void PlatformGraphicsContextRecording::fillPath(const Path& pathToFill, WindRule fillRule)
{
    appendDrawingOperation(new GraphicsOperation::FillPath(pathToFill, fillRule), pathToFill.boundingRect());
}

void PlatformGraphicsContextRecording::fillRect(const FloatRect& rect)
{
    appendDrawingOperation(new GraphicsOperation::FillRect(rect), rect);
}

void PlatformGraphicsContextRecording::fillRect(const FloatRect& rect,
                                       const Color& color)
{
    GraphicsOperation::FillRect* operation = new GraphicsOperation::FillRect(rect);
    operation->setColor(color);
    appendDrawingOperation(operation, rect);
}

void PlatformGraphicsContextRecording::fillRoundedRect(
        const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
        const IntSize& bottomLeft, const IntSize& bottomRight,
        const Color& color)
{
    appendDrawingOperation(new GraphicsOperation::FillRoundedRect(rect, topLeft,
                 topRight, bottomLeft, bottomRight, color), rect);
}

void PlatformGraphicsContextRecording::strokeArc(const IntRect& r, int startAngle,
                              int angleSpan)
{
    appendDrawingOperation(new GraphicsOperation::StrokeArc(r, startAngle, angleSpan), r);
}

void PlatformGraphicsContextRecording::strokePath(const Path& pathToStroke)
{
    appendDrawingOperation(new GraphicsOperation::StrokePath(pathToStroke), pathToStroke.boundingRect());
}

void PlatformGraphicsContextRecording::strokeRect(const FloatRect& rect, float lineWidth)
{
    FloatRect bounds = rect;
    bounds.inflate(lineWidth);
    appendDrawingOperation(new GraphicsOperation::StrokeRect(rect, lineWidth), bounds);
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
    ALOGV("pushStateOperation: %p(isLayer=%d)", canvasState, canvasState->isTransparencyLayer());
    mRecordingStateStack.append(canvasState);
    mRecording->recording()->addCanvasState(canvasState);
}

void PlatformGraphicsContextRecording::popStateOperation()
{
    RecordingState state = mRecordingStateStack.last();
    mRecordingStateStack.removeLast();
    if (!state.mHasDrawing) {
        ALOGV("popStateOperation is deleting %p(isLayer=%d)",
                state.mCanvasState, state.mCanvasState->isTransparencyLayer());
        mRecording->recording()->removeCanvasState(state.mCanvasState);
        delete state.mCanvasState;
    } else {
        ALOGV("popStateOperation: %p(isLayer=%d)",
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
        ALOGV("Operation %s(%s) was clipped out", operation->name(),
                operation->parameters().ascii().data());
        delete operation;
        return;
    }
    ALOGV("appendOperation %p->%s()", operation, operation->name());
    operation->m_globalBounds = ibounds;
    RecordingData* data = new RecordingData(operation, mRecording->recording()->m_nodeCount++);
    mRecording->recording()->m_tree.insert(ibounds, data);
}

void PlatformGraphicsContextRecording::appendStateOperation(GraphicsOperation::Operation* operation)
{
    ALOGV("appendOperation %p->%s()", operation, operation->name());
    RecordingData* data = new RecordingData(operation, mRecording->recording()->m_nodeCount++);
    mRecordingStateStack.last().mCanvasState->adoptAndAppend(data);
}

}   // WebCore
