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

namespace WebCore {

class RecordingData {
public:
    RecordingData(GraphicsOperation::Operation* ops, int orderBy)
        : m_orderBy(orderBy)
        , m_operation(ops)
    {}
    ~RecordingData() {
        delete m_operation;
    }

    unsigned int m_orderBy;
    GraphicsOperation::Operation* m_operation;
};

typedef RTree<RecordingData*, float, 2> RecordingTree;

class RecordingImpl {
public:
    RecordingImpl()
        : m_nodeCount(0)
    {
        m_states.reserveCapacity(50000);
    }

    ~RecordingImpl() {
        clear();
    }

    void clear() {
        RecordingTree::Iterator it;
        for (m_tree.GetFirst(it); !m_tree.IsNull(it); m_tree.GetNext(it)) {
            RecordingData* removeElem = m_tree.GetAt(it);
            if (removeElem)
                delete removeElem;
        }
        m_tree.RemoveAll();
    }

    RecordingTree m_tree;
    Vector<PlatformGraphicsContext::State> m_states;
    int m_nodeCount;
};

Recording::~Recording()
{
    delete m_recording;
}

static bool GatherSearchResults(RecordingData* data, void* context)
{
    ((Vector<RecordingData*>*)context)->append(data);
    return true;
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
    float searchMin[] = {clip.fLeft, clip.fTop};
    float searchMax[] = {clip.fRight, clip.fBottom};
    m_recording->m_tree.Search(searchMin, searchMax, GatherSearchResults, &nodes);
    size_t count = nodes.size();
    ALOGV("Drawing %d nodes out of %d", count, m_recording->m_nodeCount);
    if (count) {
        nonCopyingSort(nodes.begin(), nodes.end(), CompareRecordingDataOrder);
        PlatformGraphicsContextSkia context(canvas);
        for (size_t i = 0; i < count; i++)
            nodes[i]->m_operation->apply(&context);
    }
    ALOGV("Using %dkb for state storage", (sizeof(PlatformGraphicsContext::State) * m_recording->m_states.size()) / 1024);
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
{
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
    appendStateOperation(new GraphicsOperation::BeginTransparencyLayer(opacity));
}

void PlatformGraphicsContextRecording::endTransparencyLayer()
{
    appendStateOperation(new GraphicsOperation::EndTransparencyLayer());
}

void PlatformGraphicsContextRecording::save()
{
    PlatformGraphicsContext::save();
    mRecordingStateStack.append(new GraphicsOperation::Save());
}

void PlatformGraphicsContextRecording::restore()
{
    PlatformGraphicsContext::restore();
    RecordingState state = mRecordingStateStack.last();
    mRecordingStateStack.removeLast();
    if (state.mHasDrawing)
        appendDrawingOperation(state.mSaveOperation, state.mBounds);
    else
        delete state.mSaveOperation;
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
    mCurrentMatrix.preConcat(affine);
    appendStateOperation(new GraphicsOperation::ConcatCTM(affine));
}

void PlatformGraphicsContextRecording::rotate(float angleInRadians)
{
    float value = angleInRadians * (180.0f / 3.14159265f);
    mCurrentMatrix.preRotate(SkFloatToScalar(value));
    appendStateOperation(new GraphicsOperation::Rotate(angleInRadians));
}

void PlatformGraphicsContextRecording::scale(const FloatSize& size)
{
    mCurrentMatrix.preScale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
    appendStateOperation(new GraphicsOperation::Scale(size));
}

void PlatformGraphicsContextRecording::translate(float x, float y)
{
    mCurrentMatrix.preTranslate(SkFloatToScalar(x), SkFloatToScalar(y));
    appendStateOperation(new GraphicsOperation::Translate(x, y));
}

const SkMatrix& PlatformGraphicsContextRecording::getTotalMatrix()
{
    return mCurrentMatrix;
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
    if (mRecordingStateStack.size())
        mRecordingStateStack.last().clip(rect);
    appendStateOperation(new GraphicsOperation::Clip(rect));
    return true;
}

bool PlatformGraphicsContextRecording::clip(const Path& path)
{
    if (mRecordingStateStack.size())
        mRecordingStateStack.last().clip(path.boundingRect());
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
    if (mRecordingStateStack.size())
        mRecordingStateStack.last().clip(pathToClip.boundingRect());
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

void PlatformGraphicsContextRecording::appendDrawingOperation(
        GraphicsOperation::Operation* operation, const FloatRect& bounds)
{
    if (bounds.isEmpty()) {
        ALOGW("Empty bounds for %s(%s)!", operation->name(), operation->parameters().ascii().data());
        return;
    }
    if (mRecordingStateStack.size()) {
        RecordingState& state = mRecordingStateStack.last();
        state.mHasDrawing = true;
        state.addBounds(bounds);
        state.mSaveOperation->operations()->adoptAndAppend(operation);
        return;
    }
    if (!mOperationState) {
        mRecording->recording()->m_states.append(m_state->cloneInheritedProperties());
        mOperationState = &mRecording->recording()->m_states.last();
    }
    operation->m_state = mOperationState;
    RecordingData* data = new RecordingData(operation, mRecording->recording()->m_nodeCount++);
    float min[] = {bounds.x(), bounds.y()};
    float max[] = {bounds.maxX(), bounds.maxY()};
    mRecording->recording()->m_tree.Insert(min, max, data);
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

}   // WebCore
