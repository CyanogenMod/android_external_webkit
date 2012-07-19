/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTMLAllCollection.h"

#include "Element.h"
#include "Node.h"

namespace WebCore {

PassRefPtr<HTMLAllCollection> HTMLAllCollection::create(PassRefPtr<Node> base, CollectionType type)
{
    return adoptRef(new HTMLAllCollection(base, type));
}

HTMLAllCollection::HTMLAllCollection(PassRefPtr<Node> base, CollectionType type)
    : HTMLCollection(base, type)
{
}

HTMLAllCollection::~HTMLAllCollection()
{
}

Element* HTMLAllCollection::itemAfter(Element* previous) const
{
    bool includeChildren = (type() == DocAll);
    Node* current;
    Node* root = base();
    if (!previous)
        current = root->firstChild();
    else
        current = includeChildren ? previous->traverseNextNode(root) : previous->traverseNextSibling(root);

    if (includeChildren) {
        Node * lastDecendant = info()->lastDecendantOfBase;
        if (!lastDecendant) {
            info()->lastDecendantOfBase = root->lastDescendantNode();
            lastDecendant = info()->lastDecendantOfBase;
        }

        for (; current; current = current->traverseNextNodeFastPath()) {
            if (current->isElementNode())
                return static_cast<Element*>(current);
            if (current == lastDecendant)
                break;
        }
    } else {
        for (; current; current = current->traverseNextSibling(root)) {
            if (current->isElementNode())
                return static_cast<Element*>(current);
        }
    }

    return 0;
}


} // namespace WebCore
