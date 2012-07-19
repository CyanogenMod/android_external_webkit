/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012 The Linux Foundation All rights reserved.
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
 */

#include "config.h"
#include "TagNodeList.h"

#include "Element.h"
#include <wtf/Assertions.h>

namespace WebCore {

TagNodeListNS::TagNodeListNS(PassRefPtr<Node> rootNode, const AtomicString& namespaceURI, const AtomicString& localName)
    : DynamicNodeList(rootNode)
    , m_namespaceURI(namespaceURI)
    , m_localName(localName)
    , m_isStarAtomNamespaceURI(m_namespaceURI == starAtom)
    , m_isStarAtomlocalName(m_localName == starAtom)
{
    ASSERT(m_namespaceURI.isNull() || !m_namespaceURI.isEmpty());
}

TagNodeListNS::~TagNodeListNS()
{
    m_rootNode->removeCachedTagNodeListNS(this, QualifiedName(nullAtom, m_localName, m_namespaceURI));
}

bool TagNodeListNS::nodeMatches(Element* testNode) const
{
    if (!m_isStarAtomNamespaceURI && m_namespaceURI != testNode->namespaceURI())
        return false;

    return m_isStarAtomlocalName || m_localName == testNode->localName();
}

TagNodeList::TagNodeList(PassRefPtr<Node> rootNode, const AtomicString& localName)
    : DynamicNodeList(rootNode)
    , m_localName(localName)
    , m_isStarAtomlocalName(m_localName == starAtom)
{
}

TagNodeList::~TagNodeList()
{
    m_rootNode->removeCachedTagNodeList(this, m_localName);
}

bool TagNodeList::nodeMatches(Element* testNode) const
{
    return m_isStarAtomlocalName || m_localName == testNode->localName();
}


} // namespace WebCore
