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

#ifndef RTree_h
#define RTree_h

#include <Vector.h>
#include "IntRect.h"
#include "GraphicsOperation.h"

namespace WebCore {

class LinearAllocator;

class RecordingData {
public:
    RecordingData(GraphicsOperation::Operation* ops, size_t orderBy)
        : m_orderBy(orderBy)
        , m_operation(ops)
    {}
    ~RecordingData() {
        m_operation->~Operation();
    }

    size_t m_orderBy;
    GraphicsOperation::Operation* m_operation;

    void* operator new(size_t size, LinearAllocator* allocator);

    // Purposely not implemented - use a LinearAllocator please
    void* operator new(size_t size);
    void operator delete(void* ptr);
};

} // namespace WebCore

namespace RTree {

class ElementList;
class Node;

class RTree {
public:
    // M -- max number of children per node
    RTree(WebCore::LinearAllocator* allocator, int M = 10);
    ~RTree();

    void insert(WebCore::IntRect& bounds, WebCore::RecordingData* payload);
    // Does an overlap search
    void search(WebCore::IntRect& clip, Vector<WebCore::RecordingData*>& list);
    // Does an inclusive remove -- all elements fully inside the clip will
    // be removed from the tree
    void remove(WebCore::IntRect& clip);
    void display();

    void* allocateNode();
    void deleteNode(Node* n);

private:

    Node* m_root;
    unsigned m_maxChildren;
    ElementList* m_listA;
    ElementList* m_listB;
    WebCore::LinearAllocator* m_allocator;

    friend class Node;
};

class ElementList {
public:

    ElementList(int size);
    ~ElementList();
    void add(Node* n);
    void tighten();
    int delta(Node* n);
    void removeAll();
    void display();

    Node** m_children;
    unsigned m_nbChildren;

private:

    int m_minX;
    int m_maxX;
    int m_minY;
    int m_maxY;
    int m_area;
    bool m_didTighten;
};

class Node {
public:
    static Node* gRoot;

    static Node* create(RTree* tree) {
        return create(tree, 0, 0, 0, 0, 0);
    }

    static Node* create(RTree* tree, int minx, int miny, int maxx, int maxy,
                        WebCore::RecordingData* payload) {
        return new (tree->allocateNode()) Node(tree, minx, miny, maxx, maxy, payload);
    }

    ~Node();

    void insert(Node* n);
    void search(int minx, int miny, int maxx, int maxy, Vector<WebCore::RecordingData*>& list);
    void remove(int minx, int miny, int maxx, int maxy);

    // Intentionally not implemented as Node* is custom allocated, we don't want to use this
    void operator delete(void*);

private:
    Node(RTree* tree, int minx, int miny, int maxx, int maxy, WebCore::RecordingData* payload);

    void setParent(Node* n);
    Node* findNode(Node* n);
    void simpleAdd(Node* n);
    void add(Node* n);
    void remove(Node* n);
    void destroy(int index);
    void removeAll();
    Node* split();
    void adjustTree(Node* N, Node* NN);
    void tighten();
    bool updateBounds();
    int delta(Node* n);

    bool overlap(int minx, int miny, int maxx, int maxy);
    bool inside(int minx, int miny, int maxx, int maxy);

    bool isElement() { return m_payload; }
    bool isRoot();

private:

    RTree* m_tree;
    Node* m_parent;

    Node** m_children;
    unsigned m_nbChildren;

    WebCore::RecordingData* m_payload;

public:

    int m_minX;
    int m_minY;
    int m_maxX;
    int m_maxY;

#ifdef DEBUG
    void drawTree(int level = 0);
    void display(int level = 0);
    unsigned m_tid;
#endif
};

} // namespace RTree

#endif // RTree_h
