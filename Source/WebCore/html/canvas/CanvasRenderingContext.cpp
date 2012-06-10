/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CanvasRenderingContext.h"

#include "CachedImage.h"
#include "CanvasPattern.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "KURL.h"
#include "SecurityOrigin.h"

namespace WebCore {

CanvasRenderingContext::CanvasRenderingContext(HTMLCanvasElement* canvas)
    : m_canvas(canvas)
{
}

void CanvasRenderingContext::ref()
{
    m_canvas->ref();
}

void CanvasRenderingContext::deref()
{
    m_canvas->deref(); 
}

void CanvasRenderingContext::checkOrigin(const CanvasPattern* pattern)
{
    if (canvas()->originClean() && pattern && !pattern->originClean())
        canvas()->setOriginTainted();
}

void CanvasRenderingContext::checkOrigin(const HTMLCanvasElement* sourceCanvas)
{
    if (canvas()->originClean() && sourceCanvas && !sourceCanvas->originClean())
        canvas()->setOriginTainted();
}

void CanvasRenderingContext::checkOrigin(const HTMLImageElement* image)
{
    if (!image || !canvas()->originClean())
        return;

    CachedImage* cachedImage = image->cachedImage();
    checkOrigin(cachedImage->response().url());

    if (canvas()->originClean() && !cachedImage->image()->hasSingleSecurityOrigin())
        canvas()->setOriginTainted();
}

void CanvasRenderingContext::checkOrigin(const HTMLVideoElement* video)
{
#if ENABLE(VIDEO)
    checkOrigin(KURL(KURL(), video->currentSrc()));
    if (canvas()->originClean() && video && !video->hasSingleSecurityOrigin())
        canvas()->setOriginTainted();
#endif
}

void CanvasRenderingContext::checkOrigin(const KURL& url)
{
    if (!canvas()->originClean() || m_cleanOrigins.contains(url.string()))
        return;

    if (canvas()->securityOrigin().taintsCanvas(url))
        canvas()->setOriginTainted();
    else
        m_cleanOrigins.add(url.string());
}



bool CanvasRenderingContext::wouldTaintOrigin(const CanvasPattern* pattern)
{
    if (canvas()->originClean() && pattern && !pattern->originClean())
        return true;
    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const HTMLCanvasElement* sourceCanvas)
{
    if (canvas()->originClean() && sourceCanvas && !sourceCanvas->originClean())
        return true;
    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const HTMLImageElement* image)
{
    if (!image || !canvas()->originClean())
        return false;

    CachedImage* cachedImage = image->cachedImage();
    if (wouldTaintOrigin(cachedImage->response().url()))
        return true;

    if (!cachedImage->image()->hasSingleSecurityOrigin())
        return true;

    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const HTMLVideoElement* video)
{
#if ENABLE(VIDEO)
    if (!video || !canvas()->originClean())
        return false;

    if (wouldTaintOrigin(KURL(ParsedURLString, video->currentSrc())))
        return true;

    if (!video->hasSingleSecurityOrigin())
        return true;
#endif

    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const KURL& url)
{
    if (!canvas()->originClean() || m_cleanOrigins.contains(url.string()))
        return false;

    if (canvas()->securityOrigin().taintsCanvas(url))
        return true;

    m_cleanOrigins.add(url.string());
    return false;
}

} // namespace WebCore
