#define LOG_TAG "GraphicsOperationCollection"
#define LOG_NDEBUG 1

#include "config.h"
#include "GraphicsOperationCollection.h"

#include "AndroidLog.h"
#include "GraphicsContext.h"
#include "PlatformGraphicsContext.h"
#include "PlatformGraphicsContextRecording.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

using namespace GraphicsOperation;

static bool isDrawingOperation(GraphicsOperation::Operation* operation) {
    switch (operation->type()) {
    case Operation::DrawBitmapPatternOperation:
    case Operation::DrawBitmapRectOperation:
    case Operation::DrawEllipseOperation:
    case Operation::DrawLineOperation:
    case Operation::DrawLineForTextOperation:
    case Operation::DrawLineForTextCheckingOperation:
    case Operation::DrawRectOperation:
    case Operation::FillPathOperation:
    case Operation::FillRectOperation:
    case Operation::FillRoundedRectOperation:
    case Operation::StrokeArcOperation:
    case Operation::StrokePathOperation:
    case Operation::StrokeRectOperation:
    case Operation::DrawComplexTextOperation:
    case Operation::DrawTextOperation:
        return true;
    default:
        return false;
    }
}

GraphicsOperationCollection::GraphicsOperationCollection()
{
}

GraphicsOperationCollection::~GraphicsOperationCollection()
{
}

void GraphicsOperationCollection::apply(PlatformGraphicsContext* context)
{
    flush();
    for (unsigned int i = 0; i < m_operations.size(); i++)
        m_operations[i]->apply(context);
}

void GraphicsOperationCollection::adoptAndAppend(GraphicsOperation::Operation* rawOp)
{
    PassRefPtr<GraphicsOperation::Operation> operation = adoptRef(rawOp);
    if (operation->type() == Operation::SaveOperation) {
        // TODO: Support nested Save/Restore checking?
        flush();
        m_pendingOperations.append(operation);
        return;
    }
    if (m_pendingOperations.size()) {
        if (operation->type() == Operation::RestoreOperation) {
            // We hit a Save/Restore pair without any drawing, discard
            m_pendingOperations.clear();
            return;
        }
        if (!isDrawingOperation(operation.get())) {
            // Isn't a drawing operation, so append to pending and return
            m_pendingOperations.append(operation);
            return;
        }
        // Hit a drawing operation, so flush the pending and append to m_operations
        flush();
    }
    m_operations.append(operation);
}

bool GraphicsOperationCollection::isEmpty()
{
    flush();
    return !m_operations.size();
}

void GraphicsOperationCollection::flush()
{
    if (m_pendingOperations.size()) {
        m_operations.append(m_pendingOperations);
        m_pendingOperations.clear();
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
