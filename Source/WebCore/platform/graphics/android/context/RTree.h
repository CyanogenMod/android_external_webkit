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

class RecordingData {
public:
    RecordingData(GraphicsOperation::Operation* ops, int orderBy)
        : m_orderBy(orderBy)
        , m_operation(ops)
    {}
    ~RecordingData() {
        delete m_operation;
    }

    unsigned int m_orderBy;
    GraphicsOperation::Operation* m_operation;
};

}

namespace RTree {

class ElementList;
class Node;

class RTree {
public:
    // M -- max number of children per node
    RTree(int M = 10);
    ~RTree();

    void insert(WebCore::IntRect& bounds, WebCore::RecordingData* payload);
    // Does an overlap search
    void search(WebCore::IntRect& clip, Vector<WebCore::RecordingData*>& list);
    void display();

private:

    Node* root;
    unsigned maxChildren;
    ElementList* listA;
    ElementList* listB;

    friend class Node;
};

class ElementList {
public:

    ElementList(int size);
    ~ElementList();
    void add(Node* n, bool doTighten = true);
    void tighten();
    int delta(Node* n);
    void removeAll();
    void display();

    Node** children;
    unsigned nbChildren;

private:

    int minx;
    int maxx;
    int miny;
    int maxy;
    int area;
};

class Node {
public:
    static Node* gRoot;

    Node(RTree* t);
    virtual ~Node();

    void insert(Node* n);
    void search(int minx, int miny, int maxx, int maxy, Vector<WebCore::RecordingData*>& list);
    void drawTree(int level = 0);
    virtual void display(int level = 0);

private:

    void setParent(Node* n);
    Node* findNode(Node* n);
    void add(Node* n);
    void remove(Node* n);
    void removeAll();
    Node* split();
    void adjustTree(Node* N, Node* NN);
    void tighten();
    int delta(Node* n);

    bool overlap(int minx, int miny, int maxx, int maxy);

    virtual bool isElement() { return false; }
    bool isRoot();

private:

    RTree* tree;
    Node* parent;

    Node** children;
    unsigned nbChildren;

public:

    int minx;
    int miny;
    int maxx;
    int maxy;

#ifdef DEBUG
    unsigned tid;
#endif
};

class Element : public Node {
public:

    Element(RTree* tree, int minx, int miny, int maxx, int maxy, WebCore::RecordingData* payload);
    virtual ~Element();
    virtual bool isElement() { return true; }

    virtual void display(int level = 0);

    WebCore::RecordingData* payload;
};

}

#endif // RTree_h
