/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012, Code Aurora Forum. All rights reserved
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MarkupAccumulator.h"

#include "CDATASection.h"
#include "Comment.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Editor.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "ProcessingInstruction.h"
#include "XMLNSNames.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace HTMLNames;

void appendCharactersReplacingEntities(StringBuilder& result, const UChar* content, size_t length, EntityMask entityMask)
{
    size_t positionAfterLastEntity = 0;
    if (entityMask & EntityNbsp) {
        for (size_t i = 0; i < length; ++i) {
            UChar c = content[i];
            if (c == noBreakSpace) {
                result.append(content + positionAfterLastEntity, i - positionAfterLastEntity);
                result.append("&nbsp;", 6);
                positionAfterLastEntity = i + 1;
            } else if (c == '&' && EntityAmp & entityMask) {
                result.append(content + positionAfterLastEntity, i - positionAfterLastEntity);
                result.append("&amp;", 5);
                positionAfterLastEntity = i + 1;
            } else if (c == '<' && EntityLt & entityMask) {
                result.append(content + positionAfterLastEntity, i - positionAfterLastEntity);
                result.append("&lt;", 4);
                positionAfterLastEntity = i + 1;
            } else if (c == '>' && EntityGt & entityMask) {
                result.append(content + positionAfterLastEntity, i - positionAfterLastEntity);
                result.append("&gt;", 4);
                positionAfterLastEntity = i + 1;
            } else if (c == '"' && EntityQuot & entityMask) {
                result.append(content + positionAfterLastEntity, i - positionAfterLastEntity);
                result.append("&quot;", 6);
                positionAfterLastEntity = i + 1;
            }
        }
    }else if (entityMask) {
        for (size_t i = 0; i < length; ++i) {
            UChar c = content[i];
            if (c == '&' && EntityAmp & entityMask) {
                result.append(content + positionAfterLastEntity, i - positionAfterLastEntity);
                result.append("&amp;", 5);
                positionAfterLastEntity = i + 1;
            } else if (c == '<' && EntityLt & entityMask) {
                result.append(content + positionAfterLastEntity, i - positionAfterLastEntity);
                result.append("&lt;", 4);
                positionAfterLastEntity = i + 1;
            } else if (c == '>' && EntityGt & entityMask) {
                result.append(content + positionAfterLastEntity, i - positionAfterLastEntity);
                result.append("&gt;", 4);
                positionAfterLastEntity = i + 1;
            } else if (c == '"' && EntityQuot & entityMask) {
                result.append(content + positionAfterLastEntity, i - positionAfterLastEntity);
                result.append("&quot;", 6);
                positionAfterLastEntity = i + 1;
            }
        }
    }
    result.append(content + positionAfterLastEntity, length - positionAfterLastEntity);
}

MarkupAccumulator::MarkupAccumulator(Vector<Node*>* nodes, EAbsoluteURLs shouldResolveURLs, const Range* range)
    : m_nodes(nodes)
    , m_range(range)
    , m_shouldResolveURLs(shouldResolveURLs)
{
}

MarkupAccumulator::~MarkupAccumulator()
{
}

String MarkupAccumulator::serializeNodes(Node* node, Node* nodeToSkip, EChildrenOnly childrenOnly)
{
    serializeNodesWithNamespaces(node, nodeToSkip, childrenOnly, 0);
    return m_markup.toString();
}

void MarkupAccumulator::serializeNodesWithNamespaces(Node* node, Node* nodeToSkip, EChildrenOnly childrenOnly, const Namespaces* namespaces)
{
    if (node == nodeToSkip)
        return;

    Namespaces namespaceHash;
    if (namespaces)
        namespaceHash = *namespaces;

    if (!childrenOnly)
        appendStartTag(node, &namespaceHash);

    if (!(node->document()->isHTMLDocument() && elementCannotHaveEndTag(node))) {
        for (Node* current = node->firstChild(); current; current = current->nextSibling())
            serializeNodesWithNamespaces(current, nodeToSkip, IncludeNode, &namespaceHash);
    }

    if (!childrenOnly)
        appendEndTag(node);
}

void MarkupAccumulator::appendString(const String& string)
{
    m_markup.append(string);
}

void MarkupAccumulator::appendStartTag(Node* node, Namespaces* namespaces)
{
    appendStartMarkup(m_markup, node, namespaces);
    if (m_nodes)
        m_nodes->append(node);
}

void MarkupAccumulator::appendEndTag(Node* node)
{
    appendEndMarkup(m_markup, node);
}

size_t MarkupAccumulator::totalLength(const Vector<String>& strings)
{
    size_t length = 0;
    for (size_t i = 0; i < strings.size(); ++i)
        length += strings[i].length();
    return length;
}

void MarkupAccumulator::concatenateMarkup(StringBuilder& result)
{
    result.append(m_markup);
}

void MarkupAccumulator::appendAttributeValue(StringBuilder& result, const String& attribute, bool documentIsHTML)
{
    appendCharactersReplacingEntities(result, attribute.characters(), attribute.length(),
        documentIsHTML ? EntityMaskInHTMLAttributeValue : EntityMaskInAttributeValue);
}

void MarkupAccumulator::appendQuotedURLAttributeValue(StringBuilder& result, const String& urlString)
{
    UChar quoteChar = '"';
    String strippedURLString = urlString.stripWhiteSpace();
    if (protocolIsJavaScript(strippedURLString)) {
        // minimal escaping for javascript urls
        if (strippedURLString.contains('"')) {
            if (strippedURLString.contains('\''))
                strippedURLString.replace('"', "&quot;");
            else
                quoteChar = '\'';
        }
        result.append(quoteChar);
        result.append(strippedURLString);
        result.append(quoteChar);
        return;
    }

    // FIXME: This does not fully match other browsers. Firefox percent-escapes non-ASCII characters for innerHTML.
    result.append(quoteChar);
    appendAttributeValue(result, urlString, false);
    result.append(quoteChar);
}

void MarkupAccumulator::appendNodeValue(StringBuilder& out, const Node* node, const Range* range, EntityMask entityMask)
{
    String str = node->nodeValue();
    const UChar* characters = str.characters();
    size_t length = str.length();

    if (range) {
        ExceptionCode ec;
        if (node == range->endContainer(ec))
            length = range->endOffset(ec);
        if (node == range->startContainer(ec)) {
            size_t start = range->startOffset(ec);
            characters += start;
            length -= start;
        }
    }

    appendCharactersReplacingEntities(out, characters, length, entityMask);
}

bool MarkupAccumulator::shouldAddNamespaceElement(const Element* element)
{
    // Don't add namespace attribute if it is already defined for this elem.
    const AtomicString& prefix = element->prefix();
    AtomicString attr = !prefix.isEmpty() ? "xmlns:" + prefix : "xmlns";
    return !element->hasAttribute(attr);
}

bool MarkupAccumulator::shouldAddNamespaceAttribute(const Attribute& attribute, Namespaces& namespaces)
{
    namespaces.checkConsistency();

    // Don't add namespace attributes twice
    if (attribute.name() == XMLNSNames::xmlnsAttr) {
        namespaces.set(emptyAtom.impl(), attribute.value().impl());
        return false;
    }
    
    QualifiedName xmlnsPrefixAttr(xmlnsAtom, attribute.localName(), XMLNSNames::xmlnsNamespaceURI);
    if (attribute.name() == xmlnsPrefixAttr) {
        namespaces.set(attribute.localName().impl(), attribute.value().impl());
        return false;
    }
    
    return true;
}

void MarkupAccumulator::appendNamespace(StringBuilder& result, const AtomicString& prefix, const AtomicString& namespaceURI, Namespaces& namespaces)
{
    namespaces.checkConsistency();
    if (namespaceURI.isEmpty())
        return;
        
    // Use emptyAtoms's impl() for both null and empty strings since the HashMap can't handle 0 as a key
    AtomicStringImpl* pre = prefix.isEmpty() ? emptyAtom.impl() : prefix.impl();
    AtomicStringImpl* foundNS = namespaces.get(pre);
    if (foundNS != namespaceURI.impl()) {
        namespaces.set(pre, namespaceURI.impl());
        result.append(' ');
        result.append(xmlnsAtom.string());
        if (!prefix.isEmpty()) {
            result.append(':');
            result.append(prefix);
        }

        result.append('=');
        result.append('"');
        appendAttributeValue(result, namespaceURI, false);
        result.append('"');
    }
}

EntityMask MarkupAccumulator::entityMaskForText(Text* text) const
{
    const QualifiedName* parentName = 0;
    if (text->parentElement())
        parentName = &static_cast<Element*>(text->parentElement())->tagQName();

    if (parentName && (*parentName == scriptTag || *parentName == styleTag || *parentName == xmpTag))
        return EntityMaskInCDATA;

    return text->document()->isHTMLDocument() ? EntityMaskInHTMLPCDATA : EntityMaskInPCDATA;
}

void MarkupAccumulator::appendText(StringBuilder& result, Text* text)
{
    appendNodeValue(result, text, m_range, entityMaskForText(text));
}

void MarkupAccumulator::appendComment(StringBuilder& result, const String& comment)
{
    // FIXME: Comment content is not escaped, but XMLSerializer (and possibly other callers) should raise an exception if it includes "-->".
    static const char commentBegin[] = "<!--";
    result.append(commentBegin, sizeof(commentBegin) - 1);
    result.append(comment);
    static const char commentEnd[] = "-->";
    result.append(commentEnd, sizeof(commentEnd) - 1);
}

void MarkupAccumulator::appendDocumentType(StringBuilder& result, const DocumentType* n)
{
    if (n->name().isEmpty())
        return;

    static const char doctypeString[] = "<!DOCTYPE ";
    result.append(doctypeString, sizeof(doctypeString) - 1);
    result.append(n->name());
    if (!n->publicId().isEmpty()) {
        static const char publicString[] = " PUBLIC \"";
        result.append(publicString, sizeof(publicString) - 1);
        result.append(n->publicId());
        result.append('"');
        if (!n->systemId().isEmpty()) {
            result.append(' ');
            result.append('"');
            result.append(n->systemId());
            result.append('"');
        }
    } else if (!n->systemId().isEmpty()) {
        static const char systemString[] = " SYSTEM \"";
        result.append(systemString, sizeof(systemString) - 1);
        result.append(n->systemId());
        result.append('"');
    }
    if (!n->internalSubset().isEmpty()) {
        result.append(' ');
        result.append('[');
        result.append(n->internalSubset());
        result.append(']');
    }
    result.append('>');
}

void MarkupAccumulator::appendProcessingInstruction(StringBuilder& result, const String& target, const String& data)
{
    // FIXME: PI data is not escaped, but XMLSerializer (and possibly other callers) this should raise an exception if it includes "?>".
    result.append('<');
    result.append('?');
    result.append(target);
    result.append(' ');
    result.append(data);
    result.append('?');
    result.append('>');
}

void MarkupAccumulator::appendElement(StringBuilder& out, Element* element, Namespaces* namespaces)
{
    appendOpenTag(out, element, namespaces);

    NamedNodeMap* attributes = element->attributes();
    unsigned length = attributes->length();
    for (unsigned int i = 0; i < length; i++)
        appendAttribute(out, element, *attributes->attributeItem(i), namespaces);

    appendCloseTag(out, element);
}

void MarkupAccumulator::appendOpenTag(StringBuilder& out, Element* element, Namespaces* namespaces)
{
    out.append('<');
    out.append(element->nodeNamePreservingCase());
    if (!element->document()->isHTMLDocument() && namespaces && shouldAddNamespaceElement(element))
        appendNamespace(out, element->prefix(), element->namespaceURI(), *namespaces);    
}

void MarkupAccumulator::appendCloseTag(StringBuilder& out, Element* element)
{
    if (shouldSelfClose(element)) {
        if (element->isHTMLElement())
            out.append(' '); // XHTML 1.0 <-> HTML compatibility.
        out.append('/');
    }
    out.append('>');
}

void MarkupAccumulator::appendAttribute(StringBuilder& out, Element* element, const Attribute& attribute, Namespaces* namespaces)
{
    bool documentIsHTML = element->document()->isHTMLDocument();

    out.append(' ');

    if (documentIsHTML)
        out.append(attribute.name().localName());
    else
        out.append(attribute.name().toString());

    out.append('=');

    if (element->isURLAttribute(const_cast<Attribute*>(&attribute))) {
        // We don't want to complete file:/// URLs because it may contain sensitive information
        // about the user's system.
        if (shouldResolveURLs() && !element->document()->url().isLocalFile())
            appendQuotedURLAttributeValue(out, element->document()->completeURL(attribute.value()).string());
        else
            appendQuotedURLAttributeValue(out, attribute.value()); 
    } else {
        out.append('"');
        appendAttributeValue(out, attribute.value(), documentIsHTML);
        out.append('"');
    }

    if (!documentIsHTML && namespaces && shouldAddNamespaceAttribute(attribute, *namespaces))
        appendNamespace(out, attribute.prefix(), attribute.namespaceURI(), *namespaces);
}

void MarkupAccumulator::appendCDATASection(StringBuilder& result, const String& section)
{
    // FIXME: CDATA content is not escaped, but XMLSerializer (and possibly other callers) should raise an exception if it includes "]]>".
    static const char cdataBegin[] = "<![CDATA[";
    result.append(cdataBegin, sizeof(cdataBegin) - 1);
    result.append(section);
    static const char cdataEnd[] = "]]>";
    result.append(cdataEnd, sizeof(cdataEnd) - 1);
}

void MarkupAccumulator::appendStartMarkup(StringBuilder& result, const Node* node, Namespaces* namespaces)
{
    if (namespaces)
        namespaces->checkConsistency();

    switch (node->nodeType()) {
    case Node::TEXT_NODE:
        appendText(result, static_cast<Text*>(const_cast<Node*>(node)));
        break;
    case Node::COMMENT_NODE:
        appendComment(result, static_cast<const Comment*>(node)->data());
        break;
    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        break;
    case Node::DOCUMENT_TYPE_NODE:
        appendDocumentType(result, static_cast<const DocumentType*>(node));
        break;
    case Node::PROCESSING_INSTRUCTION_NODE:
        appendProcessingInstruction(result, static_cast<const ProcessingInstruction*>(node)->target(), static_cast<const ProcessingInstruction*>(node)->data());
        break;
    case Node::ELEMENT_NODE:
        appendElement(result, static_cast<Element*>(const_cast<Node*>(node)), namespaces);
        break;
    case Node::CDATA_SECTION_NODE:
        appendCDATASection(result, static_cast<const CDATASection*>(node)->data());
        break;
    case Node::ATTRIBUTE_NODE:
    case Node::ENTITY_NODE:
    case Node::ENTITY_REFERENCE_NODE:
    case Node::NOTATION_NODE:
    case Node::XPATH_NAMESPACE_NODE:
        ASSERT_NOT_REACHED();
        break;
    }
}

// Rules of self-closure
// 1. No elements in HTML documents use the self-closing syntax.
// 2. Elements w/ children never self-close because they use a separate end tag.
// 3. HTML elements which do not have a "forbidden" end tag will close with a separate end tag.
// 4. Other elements self-close.
bool MarkupAccumulator::shouldSelfClose(const Node* node)
{
    if (node->document()->isHTMLDocument())
        return false;
    if (node->hasChildNodes())
        return false;
    if (node->isHTMLElement() && !elementCannotHaveEndTag(node))
        return false;
    return true;
}

bool MarkupAccumulator::elementCannotHaveEndTag(const Node* node)
{
    if (!node->isHTMLElement())
        return false;

    // FIXME: ieForbidsInsertHTML may not be the right function to call here
    // ieForbidsInsertHTML is used to disallow setting innerHTML/outerHTML
    // or createContextualFragment.  It does not necessarily align with
    // which elements should be serialized w/o end tags.
    return static_cast<const HTMLElement*>(node)->ieForbidsInsertHTML();
}

void MarkupAccumulator::appendEndMarkup(StringBuilder& result, const Node* node)
{
    if (!node->isElementNode() || shouldSelfClose(node) || (!node->hasChildNodes() && elementCannotHaveEndTag(node)))
        return;

    result.append('<');
    result.append('/');
    result.append(static_cast<const Element*>(node)->nodeNamePreservingCase());
    result.append('>');
}

}
