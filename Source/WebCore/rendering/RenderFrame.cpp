/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderFrame.h"

#include "FrameView.h"
#include "HTMLFrameElement.h"
#include "RenderView.h"

namespace WebCore {

RenderFrame::RenderFrame(HTMLFrameElement* frame)
    : RenderFrameBase(frame)
{
    setInline(false);
}

FrameEdgeInfo RenderFrame::edgeInfo() const
{
    HTMLFrameElement* element = static_cast<HTMLFrameElement*>(node());
    return FrameEdgeInfo(element->noResize(), element->hasFrameBorder());
}

void RenderFrame::viewCleared()
{
    HTMLFrameElement* element = static_cast<HTMLFrameElement*>(node());
    if (!element || !widget() || !widget()->isFrameView())
        return;

    FrameView* view = static_cast<FrameView*>(widget());

    int marginWidth = element->marginWidth();
    int marginHeight = element->marginHeight();

    if (marginWidth != -1)
        view->setMarginWidth(marginWidth);
    if (marginHeight != -1)
        view->setMarginHeight(marginHeight);
}

#ifdef ANDROID_FLATTEN_FRAMESET
void RenderFrame::layout()
{
    FrameView* view = static_cast<FrameView*>(widget());
    RenderView* root = view ? view->frame()->contentRenderer() : 0;

    // Do not expand frames which has zero width or height
    if (!width() || !height() || !root) {
        updateWidgetPosition();
        if (view)
            view->layout();
        setNeedsLayout(false);
        return;
    }

    HTMLFrameElementBase* element = static_cast<HTMLFrameElementBase*>(node());
    if (element->scrollingMode() == ScrollbarAlwaysOff && !root->isFrameSet()) {
        setNeedsLayout(false);
        return;
    }

    // Update the dimensions to get the correct width and height
    updateWidgetPosition();
    if (root->preferredLogicalWidthsDirty())
        root->computePreferredLogicalWidths();

    // Expand the frame by setting frame height = content height
    setWidth(max(view->contentsWidth() + borderAndPaddingWidth(), width()));
    setHeight(max(view->contentsHeight() + borderAndPaddingHeight(), height()));

    // Update one more time
    updateWidgetPosition();

    setNeedsLayout(false);
}
#endif

} // namespace WebCore
