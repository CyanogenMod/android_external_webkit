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

GraphicsOperationCollection::GraphicsOperationCollection(const IntRect& drawArea)
    : m_drawArea(drawArea)
{
}

GraphicsOperationCollection::~GraphicsOperationCollection()
{
    for (unsigned int i = 0; i < m_operations.size(); i++)
        SkSafeUnref(m_operations[i]);
}

void GraphicsOperationCollection::apply(PlatformGraphicsContext* context)
{
    ALOGD("\nApply GraphicsOperationCollection %x, %d operations", this, m_operations.size());
    for (unsigned int i = 0; i < m_operations.size(); i++) {
        ALOGD("[%d] (%x) %s %s", i, this, m_operations[i]->name().ascii().data(),
              m_operations[i]->parameters().ascii().data());
        m_operations[i]->apply(context);
    }
}

void GraphicsOperationCollection::append(GraphicsOperation::Operation* operation)
{
    m_operations.append(operation);
}

bool GraphicsOperationCollection::isEmpty()
{
    return !m_operations.size();
}

AutoGraphicsOperationCollection::AutoGraphicsOperationCollection(const IntRect& area)
{
    m_graphicsOperationCollection = new GraphicsOperationCollection(area);
    m_platformGraphicsContext = new PlatformGraphicsContextRecording(m_graphicsOperationCollection);
    m_graphicsContext = new GraphicsContext(m_platformGraphicsContext);
}

AutoGraphicsOperationCollection::~AutoGraphicsOperationCollection()
{
    SkSafeUnref(m_graphicsOperationCollection);
    delete m_graphicsContext;
    delete m_platformGraphicsContext;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
