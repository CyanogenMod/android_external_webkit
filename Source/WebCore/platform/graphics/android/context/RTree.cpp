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

#define LOG_TAG "RTree"
#define LOG_NDEBUG 1

#include "config.h"

#include "RTree.h"
#include "AndroidLog.h"

namespace RTree {

static unsigned gID = 0;

class Element;

//////////////////////////////////////////////////////////////////////
// utility functions used by ElementList and Node

static void recomputeBounds(int& minx, int& miny,
                            int& maxx, int& maxy,
                            unsigned& nbChildren,
                            Node**& children, int* area)
{
    // compute the bounds

    if (nbChildren) {
        minx = children[0]->m_minX;
        miny = children[0]->m_minY;
        maxx = children[0]->m_maxX;
        maxy = children[0]->m_maxY;
    }

    for (unsigned int i = 1; i < nbChildren; i++)
    {
        minx = std::min(minx, children[i]->m_minX);
        miny = std::min(miny, children[i]->m_minY);
        maxx = std::max(maxx, children[i]->m_maxX);
        maxy = std::max(maxy, children[i]->m_maxY);
    }

    if (area) {
        int w = maxx - minx;
        int h = maxy - miny;
        *area = w * h;
    }
}

int computeDeltaArea(Node* node, int& minx, int& miny,
                     int& maxx, int& maxy)
{
    int newMinX = std::min(minx, node->m_minX);
    int newMinY = std::min(miny, node->m_minY);
    int newMaxX = std::max(maxx, node->m_maxX);
    int newMaxY = std::max(maxy, node->m_maxY);
    int w = newMaxX - newMinX;
    int h = newMaxY - newMinY;
    return w * h;
}

//////////////////////////////////////////////////////////////////////
// RTree
//////////////////////////////////////////////////////////////////////
//
// This is an implementation of the R-Tree data structure
// "R-Trees - a dynamic index structure for spatial searching", Guttman(84)
//
// The structure works as follow -- elements have bounds, intermediate
// nodes will also maintain bounds (the union of their children' bounds).
//
// Searching is simple -- we just traverse the tree comparing the bounds
// until we find the elements we are interested in.
//
// Each node can have at most M children -- the performances / memory usage
// is strongly impacted by a choice of a good M value (RTree::m_maxChildren).
//
// Inserting an element
// --------------------
//
// To find the leaf node N where we can insert a new element (RTree::insert(),
// Node::insert()), we need to traverse the tree, picking the branch where
// adding the new element will result in the least growth of its bounds,
// until we reach a leaf node (Node::findNode()).
//
// If the number of children of that leaf node is under M, we simply
// insert it. Otherwise, if we reached maximum capacity for that leaf,
// we split the list of children (Node::split()), creating two lists,
// where each list' elements is as far as each other as possible
// (to decrease the likelyhood of future splits).
//
// We can then assign one of the list to the original leaf node N, and
// we then create a new node NN that we try to attach to N's parent.
//
// If N's parent is also full, we go up in the hierachy and repeat
// (Node::adjustTree()).
//
//////////////////////////////////////////////////////////////////////

RTree::RTree(int M)
{
    m_maxChildren = M;
    m_listA = new ElementList(M);
    m_listB = new ElementList(M);
    m_root = new Node(this);
}

RTree::~RTree()
{
    delete m_listA;
    delete m_listB;
    delete m_root;
}

void RTree::insert(WebCore::IntRect& bounds, WebCore::RecordingData* payload)
{
    Element* e = new Element(this, bounds.x(), bounds.y(),
                             bounds.maxX(), bounds.maxY(), payload);
    m_root->insert(e);
}

void RTree::search(WebCore::IntRect& clip, Vector<WebCore::RecordingData*>&list)
{
    m_root->search(clip.x(), clip.y(), clip.maxX(), clip.maxY(), list);
}

void RTree::display()
{
    m_root->drawTree();
}

//////////////////////////////////////////////////////////////////////
// ElementList

ElementList::ElementList(int size)
    : m_nbChildren(0)
    , m_minX(0)
    , m_maxX(0)
    , m_minY(0)
    , m_maxY(0)
    , m_area(0)
{
    m_children = new Node*[size];
}

ElementList::~ElementList()
{
    delete[] m_children;
}

void ElementList::add(Node* node, bool doTighten)
{
    m_children[m_nbChildren] = node;
    m_nbChildren++;
    if (doTighten)
        tighten();
}

void ElementList::tighten()
{
    recomputeBounds(m_minX, m_minY, m_maxX, m_maxY,
                    m_nbChildren, m_children, &m_area);
}

int ElementList::delta(Node* node)
{
    return computeDeltaArea(node, m_minX, m_minY, m_maxX, m_maxY);
}

void ElementList::removeAll()
{
    m_nbChildren = 0;
    m_minX = 0;
    m_maxX = 0;
    m_minY = 0;
    m_maxY = 0;
    m_area = 0;
}

void ElementList::display() {
    for (unsigned int i = 0; i < m_nbChildren; i++)
        m_children[i]->display(0);
}

//////////////////////////////////////////////////////////////////////
// Node

Node::Node(RTree* t)
    : m_tree(t)
    , m_parent(0)
    , m_children(0)
    , m_nbChildren(0)
    , m_minX(0)
    , m_minY(0)
    , m_maxX(0)
    , m_maxY(0)
#ifdef DEBUG
    , m_tid(gID++)
#endif
{
    ALOGV("-> New Node %d", m_tid);
}

Node::~Node()
{
    delete[] m_children;
}

void Node::setParent(Node* node)
{
    m_parent = node;
}

void Node::insert(Node* node)
{
    Node* N = findNode(node);
    ALOGV("-> Insert Node %d (%d, %d) in node %d",
          node->m_tid, node->m_minX, node->m_minY, N->m_tid);
    N->add(node);
}

Node* Node::findNode(Node* node)
{
    if (m_nbChildren == 0)
        return m_parent ? m_parent : this;

    // pick the child whose bounds will be extended least

    Node* pick = m_children[0];
    int minIncrease = pick->delta(node);
    for (unsigned int i = 1; i < m_nbChildren; i++) {
        int increase = m_children[i]->delta(node);
        if (increase < minIncrease) {
            minIncrease = increase;
            pick = m_children[i];
        }
    }

    return pick->findNode(node);
}

void Node::tighten()
{
    recomputeBounds(m_minX, m_minY, m_maxX, m_maxY,
                    m_nbChildren, m_children, 0);
}

int Node::delta(Node* node)
{
    return computeDeltaArea(node, m_minX, m_minY, m_maxX, m_maxY);
}

void Node::add(Node* node)
{
    node->setParent(this);
    if (!m_children)
        m_children = new Node*[m_tree->m_maxChildren + 1];
    m_children[m_nbChildren] = node;
    m_nbChildren++;
    Node* NN = 0;
    if (m_nbChildren > m_tree->m_maxChildren)
        NN = split();
    adjustTree(this, NN);
    tighten();
}

void Node::remove(Node* node)
{
    int nodeIndex = -1;
    for (unsigned int i = 0; i < m_nbChildren; i++) {
        if (m_children[i] == node) {
            nodeIndex = i;
            break;
        }
    }
    if (nodeIndex == -1)
        return;

    // compact
    for (unsigned int i = nodeIndex; i < m_nbChildren - 1; i++)
        m_children[i] = m_children[i + 1];
    m_nbChildren--;
}

void Node::removeAll()
{
    m_nbChildren = 0;
}

Node* Node::split()
{
    // First, let's get the seeds
    // The idea is to get elements as distant as possible
    // as we can, so that the resulting splitted lists
    // will be more likely to not overlap.
    Node* minElementX = m_children[0];
    Node* maxElementX = m_children[0];
    Node* minElementY = m_children[0];
    Node* maxElementY = m_children[0];
    for (unsigned int i = 1; i < m_nbChildren; i++) {
        if (m_children[i]->m_minX < minElementX->m_minX)
            minElementX = m_children[i];
        if (m_children[i]->m_minY < minElementY->m_minY)
            minElementY = m_children[i];
        if (m_children[i]->m_maxX >= maxElementX->m_maxX)
            maxElementX = m_children[i];
        if (m_children[i]->m_maxY >= maxElementY->m_maxY)
            maxElementY = m_children[i];
    }

    int dx = maxElementX->m_maxX - minElementX->m_minX;
    int dy = maxElementY->m_maxY - minElementY->m_minY;

    // assign the two seeds...
    Node* elementA = minElementX;
    Node* elementB = maxElementX;

    if (dx < dy) {
        elementA = minElementY;
        elementB = maxElementY;
    }

    // If we get the same element, just get the first and
    // last element inserted...
    if (elementA == elementB) {
        elementA = m_children[0];
        elementB = m_children[m_nbChildren - 1];
    }

    ALOGV("split Node %d, dx: %d dy: %d elem A is %d, elem B is %d",
        m_tid, dx, dy, elementA->m_tid, elementB->m_tid);

    // Let's use some temporary lists to do the split
    ElementList* listA = m_tree->m_listA;
    ElementList* listB = m_tree->m_listB;
    listA->removeAll();
    listB->removeAll();

    listA->add(elementA);
    listB->add(elementB);

    remove(elementA);
    remove(elementB);

    // For any remaining elements, insert it into the list
    // resulting in the smallest growth
    for (unsigned int i = 0; i < m_nbChildren; i++) {
        Node* node = m_children[i];
        int dA = listA->delta(node);
        int dB = listB->delta(node);

        if (dA < dB && listA->m_nbChildren < m_tree->m_maxChildren)
            listA->add(node);
        else if (dB < dA && listB->m_nbChildren < m_tree->m_maxChildren)
            listB->add(node);
        else {
            ElementList* smallestList =
                listA->m_nbChildren > listB->m_nbChildren ? listB : listA;
            smallestList->add(node);
        }
    }

    // Use the list to rebuild the nodes
    removeAll();
    for (unsigned int i = 0; i < listA->m_nbChildren; i++)
        add(listA->m_children[i]);

    Node* NN = new Node(m_tree);
    for (unsigned int i = 0; i < listB->m_nbChildren; i++)
        NN->add(listB->m_children[i]);

    return NN;
}

bool Node::isRoot()
{
    return m_tree->m_root == this;
}

void Node::adjustTree(Node* N, Node* NN)
{
    if (N->isRoot() && NN) {
        // build new root
        Node* root = new Node(m_tree);
        ALOGV("-> node %d created as new root", root->m_tid);
        root->add(N);
        root->add(NN);
        m_tree->m_root = root;
        return;
    }
    if (N->isRoot())
        return;

    if (NN && N->m_parent)
        N->m_parent->add(NN);
}

#ifdef DEBUG
static int gMaxLevel = 0;
static int gNbNodes = 0;
static int gNbElements = 0;
#endif

void Node::drawTree(int level)
{
    if (level == 0) {
        ALOGV("\n*** show tree ***\n");
#ifdef DEBUG
        gMaxLevel = 0;
        gNbNodes = 0;
        gNbElements = 0;
#endif
    }

    display(level);
    for (unsigned int i = 0; i < m_nbChildren; i++)
    {
        m_children[i]->drawTree(level + 1);
    }

#ifdef DEBUG
    if (gMaxLevel < level)
        gMaxLevel = level;

    if (!m_nbChildren)
        gNbElements++;
    else
        gNbNodes++;

    if (level == 0) {
        ALOGV("********************\n");
        ALOGV("Depth level %d, total bytes: %d, %d nodes, %d bytes (%d bytes/node), %d elements, %d bytes (%d bytes/node)",
               gMaxLevel, gNbNodes * sizeof(Node) + gNbElements * sizeof(Element),
               gNbNodes, gNbNodes * sizeof(Node), sizeof(Node),
               gNbElements, gNbElements * sizeof(Element), sizeof(Element));
    }
#endif
}

void Node::display(int level)
{
    ALOGV("%*sNode %d - %d, %d, %d, %d (%d x %d)",
      2*level, "", m_tid, m_minX, m_minY, m_maxX, m_maxY, m_maxX - m_minX, m_maxY - m_minY);
}

bool Node::overlap(int pminx, int pminy, int pmaxx, int pmaxy)
{
    return ! (pminx > m_maxX
           || pmaxx < m_minX
           || pmaxy < m_minY
           || pminy > m_maxY);
}

void Node::search(int minx, int miny, int maxx, int maxy, Vector<WebCore::RecordingData*>& list)
{
    if (isElement() && overlap(minx, miny, maxx, maxy))
        list.append(((Element*)this)->m_payload);

    for (unsigned int i = 0; i < m_nbChildren; i++) {
        if (m_children[i]->overlap(minx, miny, maxx, maxy))
            m_children[i]->search(minx, miny, maxx, maxy, list);
    }
}

//////////////////////////////////////////////////////////////////////
// Element

Element::Element(RTree* tree, int pminx, int pminy, int pmaxx, int pmaxy, WebCore::RecordingData* p)
    : Node(tree)
    , m_payload(p)
{
    m_minX = pminx;
    m_minY = pminy;
    m_maxX = pmaxx;
    m_maxY = pmaxy;
    ALOGV("-> New element %d (%d, %d) - (%d x %d)",
          m_tid, m_minX, m_minY, m_maxX - m_minX, m_maxY - m_minY);
}

Element::~Element()
{
    delete m_payload;
}

void Element::display(int level)
{
    ALOGV("%*selement %d (%d, %d, %d, %d) - (%d x %d)", 2*level, "",
          m_tid, m_minX, m_minY, m_maxX, m_maxY, m_maxX - m_minX, m_maxY - m_minY);
}

}
