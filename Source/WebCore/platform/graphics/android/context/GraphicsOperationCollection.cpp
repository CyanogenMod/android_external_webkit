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

GraphicsOperationCollection::GraphicsOperationCollection()
{
}

GraphicsOperationCollection::~GraphicsOperationCollection()
{
    clear();
}

void GraphicsOperationCollection::apply(PlatformGraphicsContext* context) const
{
    size_t size = m_operations.size();
    for (size_t i = 0; i < size; i++)
        m_operations[i]->apply(context);
}

void GraphicsOperationCollection::adoptAndAppend(GraphicsOperation::Operation* operation)
{
    m_operations.append(operation);
}

void GraphicsOperationCollection::transferFrom(GraphicsOperationCollection& moveFrom)
{
    size_t size = moveFrom.m_operations.size();
    m_operations.reserveCapacity(m_operations.size() + size);
    for (size_t i = 0; i < size; i++)
        m_operations.append(moveFrom.m_operations[i]);
    moveFrom.m_operations.clear();
}

bool GraphicsOperationCollection::isEmpty()
{
    return !m_operations.size();
}

void GraphicsOperationCollection::clear()
{
    size_t size = m_operations.size();
    for (size_t i = 0; i < size; i++)
        delete m_operations[i];
    m_operations.clear();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
