#define LOG_TAG "PlatformGraphicsContextRecording"
#define LOG_NDEBUG 1

#include "config.h"
#include "PlatformGraphicsContextRecording.h"

#include "AndroidLog.h"
#include "FloatRect.h"
#include "FloatQuad.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "GraphicsOperationCollection.h"
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

class RecordingImpl {
public:
    RecordingImpl()
        : m_nodeCount(0)
    {
    }

    ~RecordingImpl() {
        clearStates();
        clearMatrixes();
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

    SkMatrix* cloneMatrix(const SkMatrix& matrix) {
        m_matrixes.append(new SkMatrix(matrix));
        return m_matrixes.last();
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

    void clearMatrixes() {
        for (size_t i = 0; i < m_matrixes.size(); i++)
            delete m_matrixes[i];
        m_matrixes.clear();
    }

    // TODO: Use a global pool?
    StateHashSet m_states;
    Vector<SkMatrix*> m_matrixes;
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
    ALOGV("Drawing %d nodes out of %d (state storage=%d)", count,
          m_recording->m_nodeCount, sizeof(PlatformGraphicsContext::State) * m_recording->m_states.size());
    if (count) {
        nonCopyingSort(nodes.begin(), nodes.end(), CompareRecordingDataOrder);
        PlatformGraphicsContextSkia context(canvas);
        SkMatrix* matrix = 0;
        int saveCount = 0;
        for (size_t i = 0; i < count; i++) {
            GraphicsOperation::Operation* op = nodes[i]->m_operation;
            SkMatrix* opMatrix = op->m_matrix;
            if (opMatrix != matrix) {
                matrix = opMatrix;
                if (saveCount) {
                    canvas->restoreToCount(saveCount);
                    saveCount = 0;
                }
                if (!matrix->isIdentity()) {
                    saveCount = canvas->save(SkCanvas::kMatrix_SaveFlag);
                    canvas->concat(*matrix);
                }
            }
            op->apply(&context);
        }
        if (saveCount)
            canvas->restoreToCount(saveCount);
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
    , mOperationMatrix(0)
{
    pushMatrix();
    if (mRecording)
        mRecording->setRecording(new RecordingImpl());
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
}

//**************************************
// State management
//**************************************

void PlatformGraphicsContextRecording::beginTransparencyLayer(float opacity)
{
    pushSaveOperation(new GraphicsOperation::TransparencyLayer(opacity));
}

void PlatformGraphicsContextRecording::endTransparencyLayer()
{
    popSaveOperation();
}

void PlatformGraphicsContextRecording::save()
{
    PlatformGraphicsContext::save();
    pushSaveOperation(new GraphicsOperation::Save());
}

void PlatformGraphicsContextRecording::restore()
{
    PlatformGraphicsContext::restore();
    popSaveOperation();
}

//**************************************
// State setters
//**************************************

void PlatformGraphicsContextRecording::setAlpha(float alpha)
{
    PlatformGraphicsContext::setAlpha(alpha);
    appendStateOperation(new GraphicsOperation::SetAlpha(alpha));
}

void PlatformGraphicsContextRecording::setCompositeOperation(CompositeOperator op)
{
    PlatformGraphicsContext::setCompositeOperation(op);
    appendStateOperation(new GraphicsOperation::SetCompositeOperation(op));
}

bool PlatformGraphicsContextRecording::setFillColor(const Color& c)
{
    if (PlatformGraphicsContext::setFillColor(c)) {
        appendStateOperation(new GraphicsOperation::SetFillColor(c));
        return true;
    }
    return false;
}

bool PlatformGraphicsContextRecording::setFillShader(SkShader* fillShader)
{
    if (PlatformGraphicsContext::setFillShader(fillShader)) {
        appendStateOperation(new GraphicsOperation::SetFillShader(fillShader));
        return true;
    }
    return false;
}

void PlatformGraphicsContextRecording::setLineCap(LineCap cap)
{
    PlatformGraphicsContext::setLineCap(cap);
    appendStateOperation(new GraphicsOperation::SetLineCap(cap));
}

void PlatformGraphicsContextRecording::setLineDash(const DashArray& dashes, float dashOffset)
{
    PlatformGraphicsContext::setLineDash(dashes, dashOffset);
    appendStateOperation(new GraphicsOperation::SetLineDash(dashes, dashOffset));
}

void PlatformGraphicsContextRecording::setLineJoin(LineJoin join)
{
    PlatformGraphicsContext::setLineJoin(join);
    appendStateOperation(new GraphicsOperation::SetLineJoin(join));
}

void PlatformGraphicsContextRecording::setMiterLimit(float limit)
{
    PlatformGraphicsContext::setMiterLimit(limit);
    appendStateOperation(new GraphicsOperation::SetMiterLimit(limit));
}

void PlatformGraphicsContextRecording::setShadow(int radius, int dx, int dy, SkColor c)
{
    PlatformGraphicsContext::setShadow(radius, dx, dy, c);
    appendStateOperation(new GraphicsOperation::SetShadow(radius, dx, dy, c));
}

void PlatformGraphicsContextRecording::setShouldAntialias(bool useAA)
{
    m_state->useAA = useAA;
    PlatformGraphicsContext::setShouldAntialias(useAA);
    appendStateOperation(new GraphicsOperation::SetShouldAntialias(useAA));
}

bool PlatformGraphicsContextRecording::setStrokeColor(const Color& c)
{
    if (PlatformGraphicsContext::setStrokeColor(c)) {
        appendStateOperation(new GraphicsOperation::SetStrokeColor(c));
        return true;
    }
    return false;
}

bool PlatformGraphicsContextRecording::setStrokeShader(SkShader* strokeShader)
{
    if (PlatformGraphicsContext::setStrokeShader(strokeShader)) {
        appendStateOperation(new GraphicsOperation::SetStrokeShader(strokeShader));
        return true;
    }
    return false;
}

void PlatformGraphicsContextRecording::setStrokeStyle(StrokeStyle style)
{
    PlatformGraphicsContext::setStrokeStyle(style);
    appendStateOperation(new GraphicsOperation::SetStrokeStyle(style));
}

void PlatformGraphicsContextRecording::setStrokeThickness(float f)
{
    PlatformGraphicsContext::setStrokeThickness(f);
    appendStateOperation(new GraphicsOperation::SetStrokeThickness(f));
}

//**************************************
// Matrix operations
//**************************************

void PlatformGraphicsContextRecording::concatCTM(const AffineTransform& affine)
{
    mCurrentMatrix->preConcat(affine);
    onCurrentMatrixChanged();
    appendStateOperation(new GraphicsOperation::ConcatCTM(affine));
}

void PlatformGraphicsContextRecording::rotate(float angleInRadians)
{
    float value = angleInRadians * (180.0f / 3.14159265f);
    mCurrentMatrix->preRotate(SkFloatToScalar(value));
    onCurrentMatrixChanged();
    appendStateOperation(new GraphicsOperation::Rotate(angleInRadians));
}

void PlatformGraphicsContextRecording::scale(const FloatSize& size)
{
    mCurrentMatrix->preScale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
    onCurrentMatrixChanged();
    appendStateOperation(new GraphicsOperation::Scale(size));
}

void PlatformGraphicsContextRecording::translate(float x, float y)
{
    mCurrentMatrix->preTranslate(SkFloatToScalar(x), SkFloatToScalar(y));
    onCurrentMatrixChanged();
    appendStateOperation(new GraphicsOperation::Translate(x, y));
}

const SkMatrix& PlatformGraphicsContextRecording::getTotalMatrix()
{
    // Each RecordingState tracks the delta from its "parent" SkMatrix
    mTotalMatrix = mMatrixStack.first();
    for (size_t i = 1; i < mMatrixStack.size(); i++)
        mTotalMatrix.preConcat(mMatrixStack[i]);
    return mTotalMatrix;
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
    // TODO
}

void PlatformGraphicsContextRecording::drawEllipse(const IntRect& rect)
{
    appendDrawingOperation(new GraphicsOperation::DrawEllipse(rect), rect);
}

void PlatformGraphicsContextRecording::drawFocusRing(const Vector<IntRect>& rects,
                                            int /* width */, int /* offset */,
                                            const Color& color)
{
    // TODO
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

void PlatformGraphicsContextRecording::pushSaveOperation(GraphicsOperation::Save* saveOp)
{
    mRecordingStateStack.append(saveOp);
    if (saveOp->saveMatrix())
        pushMatrix();
}

void PlatformGraphicsContextRecording::popSaveOperation()
{
    RecordingState state = mRecordingStateStack.last();
    mRecordingStateStack.removeLast();
    if (state.mSaveOperation->saveMatrix())
        popMatrix();
    if (state.mHasDrawing)
        appendDrawingOperation(state.mSaveOperation, state.mBounds);
    else
        delete state.mSaveOperation;
}

void PlatformGraphicsContextRecording::pushMatrix()
{
    mMatrixStack.append(SkMatrix::I());
    mCurrentMatrix = &(mMatrixStack.last());
}

void PlatformGraphicsContextRecording::popMatrix()
{
    mMatrixStack.removeLast();
    mCurrentMatrix = &(mMatrixStack.last());
}

void PlatformGraphicsContextRecording::appendDrawingOperation(
        GraphicsOperation::Operation* operation, const FloatRect& untranslatedBounds)
{
    if (untranslatedBounds.isEmpty()) {
        ALOGW("Empty bounds for %s(%s)!", operation->name(), operation->parameters().ascii().data());
        return;
    }
    SkRect bounds;
    mCurrentMatrix->mapRect(&bounds, untranslatedBounds);
    if (mRecordingStateStack.size()) {
        RecordingState& state = mRecordingStateStack.last();
        state.mHasDrawing = true;
        state.addBounds(bounds);
        state.mSaveOperation->operations()->adoptAndAppend(operation);
        return;
    }
    if (!mOperationState)
        mOperationState = mRecording->recording()->getState(m_state);
    if (!mOperationMatrix)
        mOperationMatrix = mRecording->recording()->cloneMatrix(mMatrixStack.first());
    operation->m_state = mOperationState;
    operation->m_matrix = mOperationMatrix;
    RecordingData* data = new RecordingData(operation, mRecording->recording()->m_nodeCount++);

    WebCore::IntRect ibounds = enclosingIntRect(bounds);
    mRecording->recording()->m_tree.insert(ibounds, data);
}

void PlatformGraphicsContextRecording::appendStateOperation(GraphicsOperation::Operation* operation)
{
    if (mRecordingStateStack.size())
        mRecordingStateStack.last().mSaveOperation->operations()->adoptAndAppend(operation);
    else {
        delete operation;
        mOperationState = 0;
    }
}

void PlatformGraphicsContextRecording::onCurrentMatrixChanged()
{
    if (mCurrentMatrix == &(mMatrixStack.first()))
        mOperationMatrix = 0;
}

}   // WebCore
