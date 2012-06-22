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
    for (unsigned int i = 0; i < m_operations.size(); i++)
        SkSafeUnref(m_operations[i]);
}

void GraphicsOperationCollection::apply(PlatformGraphicsContext* context)
{
    for (unsigned int i = 0; i < m_operations.size(); i++)
        m_operations[i]->apply(context);
}

void GraphicsOperationCollection::append(GraphicsOperation::Operation* operation)
{
    m_operations.append(operation);
}

bool GraphicsOperationCollection::isEmpty()
{
    return !m_operations.size();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
