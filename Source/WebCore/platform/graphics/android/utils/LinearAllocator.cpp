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

#define LOG_TAG "LinearAllocator"
#define LOG_NDEBUG 1

#include "config.h"
#include "LinearAllocator.h"

#include "AndroidLog.h"

namespace WebCore {

// The ideal size of a page allocation (these need to be multiples of 4)
#define INITIAL_PAGE_SIZE ((size_t)4096) // 4kb
#define MAX_PAGE_SIZE ((size_t)131072) // 128kb

// The maximum amount of wasted space we can have per page
// Allocations exceeding this will have their own dedicated page
// If this is too low, we will malloc too much
// Too high, and we may waste too much space
// Must be smaller than INITIAL_PAGE_SIZE
#define MAX_WASTE_SIZE ((size_t)1024)

#define ALIGN(x) (x + (x % sizeof(int)))

#if LOG_NDEBUG
#define ADD_ALLOCATION(size)
#define RM_ALLOCATION(size)
#else
#include <utils/Thread.h>
static size_t s_totalAllocations = 0;
static double s_lastLogged = 0;
static android::Mutex s_mutex;

static void _logUsageLocked() {
    double now = currentTimeMS();
    if (now - s_lastLogged > 5) {
        s_lastLogged = now;
        ALOGV("Total memory usage: %d kb", s_totalAllocations / 1024);
    }
}

static void _addAllocation(size_t size) {
    android::AutoMutex lock(s_mutex);
    s_totalAllocations += size;
    _logUsageLocked();
}

#define ADD_ALLOCATION(size) _addAllocation(size);
#define RM_ALLOCATION(size) _addAllocation(-size);
#endif

class LinearAllocator::Page {
public:
    Page* next() { return m_nextPage; }
    void setNext(Page* next) { m_nextPage = next; }

    Page()
        : m_nextPage(0)
    {}

    void* start()
    {
        return (void*) (((unsigned)this) + sizeof(LinearAllocator::Page));
    }

    void* end(int pageSize)
    {
        return (void*) (((unsigned)start()) + pageSize);
    }

private:
    Page(const Page& other) {}
    Page* m_nextPage;
};

LinearAllocator::LinearAllocator()
    : m_pageSize(INITIAL_PAGE_SIZE)
    , m_maxAllocSize(MAX_WASTE_SIZE)
    , m_next(0)
    , m_currentPage(0)
    , m_pages(0)
    , m_totalAllocated(0)
    , m_wastedSpace(0)
    , m_pageCount(0)
    , m_dedicatedPageCount(0)
{
}

LinearAllocator::~LinearAllocator(void)
{
    Page* p = m_pages;
    while (p) {
        Page* next = p->next();
        delete p;
        RM_ALLOCATION(m_pageSize);
        p = next;
    }
}

void* LinearAllocator::start(Page* p)
{
    return ((char*)p) + sizeof(Page);
}

void* LinearAllocator::end(Page* p)
{
    return ((char*)p) + m_pageSize;
}

bool LinearAllocator::fitsInCurrentPage(size_t size)
{
    return m_next && ((char*)m_next + size) <= end(m_currentPage);
}

void LinearAllocator::ensureNext(size_t size)
{
    if (fitsInCurrentPage(size))
        return;
    if (m_currentPage && m_pageSize < MAX_PAGE_SIZE) {
        m_pageSize = std::min(MAX_PAGE_SIZE, m_pageSize * 2);
        m_pageSize = ALIGN(m_pageSize);
    }
    m_wastedSpace += m_pageSize;
    Page* p = newPage(m_pageSize);
    if (m_currentPage)
        m_currentPage->setNext(p);
    m_currentPage = p;
    if (!m_pages)
        m_pages = m_currentPage;
    m_next = start(m_currentPage);
}

void* LinearAllocator::alloc(size_t size)
{
    size = ALIGN(size);
    if (size > m_maxAllocSize && !fitsInCurrentPage(size)) {
        ALOGV("Exceeded max size %d > %d", size, m_maxAllocSize);
        // Allocation is too large, create a dedicated page for the allocation
        Page* page = newPage(size);
        m_dedicatedPageCount++;
        page->setNext(m_pages);
        m_pages = page;
        if (!m_currentPage)
            m_currentPage = m_pages;
        return start(page);
    }
    ensureNext(size);
    void* ptr = m_next;
    m_next = ((char*)m_next) + size;
    m_wastedSpace -= size;
    return ptr;
}

void LinearAllocator::rewindIfLastAlloc(void* ptr, size_t allocSize)
{
    // Don't bother rewinding across pages
    if (ptr >= start(m_currentPage) && ptr < end(m_currentPage)
            && ptr == ((char*)m_next - allocSize)) {
        m_totalAllocated -= allocSize;
        m_wastedSpace += allocSize;
        m_next = ptr;
    }
}

LinearAllocator::Page* LinearAllocator::newPage(size_t pageSize)
{
    pageSize += sizeof(LinearAllocator::Page);
    ADD_ALLOCATION(pageSize);
    m_totalAllocated += pageSize;
    m_pageCount++;
    void* buf = malloc(pageSize);
    return new (buf) Page();
}

static const char* toSize(size_t value, float& result)
{
    if (value < 2000) {
        result = value;
        return "B";
    }
    if (value < 2000000) {
        result = value / 1024.0f;
        return "KB";
    }
    result = value / 1048576.0f;
    return "MB";
}

void LinearAllocator::dumpMemoryStats(const char* prefix)
{
    float prettySize;
    const char* prettySuffix;
    prettySuffix = toSize(m_totalAllocated, prettySize);
    ALOGD("%sTotal allocated: %.2f%s", prefix, prettySize, prettySuffix);
    prettySuffix = toSize(m_wastedSpace, prettySize);
    ALOGD("%sWasted space: %.2f%s (%.1f%%)", prefix, prettySize, prettySuffix,
          (float) m_wastedSpace / (float) m_totalAllocated * 100.0f);
    ALOGD("%sPages %d (dedicated %d)", prefix, m_pageCount, m_dedicatedPageCount);
}

} // namespace WebCore
