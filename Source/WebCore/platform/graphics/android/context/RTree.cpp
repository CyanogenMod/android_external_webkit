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
        minx = children[0]->minx;
        miny = children[0]->miny;
        maxx = children[0]->maxx;
        maxy = children[0]->maxy;
    }

    for (unsigned int i = 1; i < nbChildren; i++)
    {
        minx = std::min(minx, children[i]->minx);
        miny = std::min(miny, children[i]->miny);
        maxx = std::max(maxx, children[i]->maxx);
        maxy = std::max(maxy, children[i]->maxy);
    }

    if (area) {
        int w = maxx - minx;
        int h = maxy - miny;
        *area = w * h;
    }
}

int computeDeltaArea(Node* n, int& minx, int& miny,
                     int& maxx, int& maxy)
{
    int newMinX = std::min(minx, n->minx);
    int newMinY = std::min(miny, n->miny);
    int newMaxX = std::max(maxx, n->maxx);
    int newMaxY = std::max(maxy, n->maxy);
    int w = newMaxX - newMinX;
    int h = newMaxY - newMinY;
    return w * h;
}

//////////////////////////////////////////////////////////////////////
// RTree

RTree::RTree(int M)
{
    maxChildren = M;
    listA = new ElementList(M);
    listB = new ElementList(M);
    root = new Node(this);
}

RTree::~RTree()
{
    delete listA;
    delete listB;
    delete root;
}

void RTree::insert(WebCore::IntRect& bounds, WebCore::RecordingData* payload)
{
    Element* e = new Element(this, bounds.x(), bounds.y(),
                             bounds.maxX(), bounds.maxY(), payload);
    root->insert(e);
}

void RTree::search(WebCore::IntRect& clip, Vector<WebCore::RecordingData*>&list)
{
    root->search(clip.x(), clip.y(), clip.maxX(), clip.maxY(), list);
}

void RTree::display()
{
    root->drawTree();
}

//////////////////////////////////////////////////////////////////////
// ElementList

ElementList::ElementList(int size)
    : nbChildren(0)
    , minx(0)
    , maxx(0)
    , miny(0)
    , maxy(0)
    , area(0)
{
    children = new Node*[size];
}

ElementList::~ElementList()
{
    delete[] children;
}

void ElementList::add(Node* n, bool doTighten)
{
    children[nbChildren] = n;
    nbChildren++;
    if (doTighten)
        tighten();
}

void ElementList::tighten()
{
    recomputeBounds(minx, miny, maxx, maxy,
                    nbChildren, children, &area);
}

int ElementList::delta(Node* n)
{
    return computeDeltaArea(n, minx, miny, maxx, maxy);
}

void ElementList::removeAll()
{
    nbChildren = 0;
    minx = 0;
    maxx = 0;
    miny = 0;
    maxy = 0;
    area = 0;
}

void ElementList::display() {
    for (unsigned int i = 0; i < nbChildren; i++)
        children[i]->display(0);
}

//////////////////////////////////////////////////////////////////////
// Node

Node::Node(RTree* t)
    : tree(t)
    , parent(0)
    , children(0)
    , nbChildren(0)
    , minx(0)
    , miny(0)
    , maxx(0)
    , maxy(0)
#ifdef DEBUG
    , tid(gID++)
#endif
{
#ifdef DEBUG
    ALOGV("-> New Node %d", tid);
#endif
}

Node::~Node()
{
    delete[] children;
}

void Node::setParent(Node* node)
{
    parent = node;
}

void Node::insert(Node* node)
{
    Node* N = findNode(node);
    ALOGV("-> Insert Node %d (%d, %d) in node %d",
          node->tid, node->minx, node->miny, N->tid);
    N->add(node);
}

Node* Node::findNode(Node* node)
{
    if (nbChildren == 0)
        return parent ? parent : this;

    // pick the child whose bounds will be extended least

    Node* pick = children[0];
    int minIncrease = pick->delta(node);
    for (unsigned int i = 1; i < nbChildren; i++) {
        int increase = children[i]->delta(node);
        if (increase < minIncrease) {
            minIncrease = increase;
            pick = children[i];
        }
    }

    return pick->findNode(node);
}

void Node::tighten()
{
    recomputeBounds(minx, miny, maxx, maxy,
                    nbChildren, children, 0);
}

int Node::delta(Node* node)
{
    return computeDeltaArea(node, minx, miny, maxx, maxy);
}

void Node::add(Node* node)
{
    node->setParent(this);
    if (!children)
        children = new Node*[tree->maxChildren + 1];
    children[nbChildren] = node;
    nbChildren++;
    Node* NN = 0;
    if (nbChildren > tree->maxChildren)
        NN = split();
    adjustTree(this, NN);
    tighten();
}

void Node::remove(Node* node)
{
    int nodeIndex = -1;
    for (unsigned int i = 0; i < nbChildren; i++) {
        if (children[i] == node) {
            nodeIndex = i;
            break;
        }
    }
    if (nodeIndex == -1)
        return;

    // compact
    for (unsigned int i = nodeIndex; i < nbChildren-1; i++)
        children[i] = children[i+1];
    nbChildren--;
}

void Node::removeAll()
{
    nbChildren = 0;
}

Node* Node::split()
{
    // First, let's get the seeds
    // The idea is to get elements as distant as possible
    // as we can, so that the resulting splitted lists
    // will be more likely to not overlap.
    Node* minElementX = children[0];
    Node* maxElementX = children[0];
    Node* minElementY = children[0];
    Node* maxElementY = children[0];
    for (unsigned int i = 1; i < nbChildren; i++) {
        if (children[i]->minx < minElementX->minx)
            minElementX = children[i];
        if (children[i]->miny < minElementY->miny)
            minElementY = children[i];
        if (children[i]->maxx >= maxElementX->maxx)
            maxElementX = children[i];
        if (children[i]->maxy >= maxElementY->maxy)
            maxElementY = children[i];
    }

    int dx = maxElementX->maxx - minElementX->minx;
    int dy = maxElementY->maxy - minElementY->miny;

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
        elementA = children[0];
        elementB = children[nbChildren-1];
    }
    ALOGV("split Node %d, dx: %d dy: %d elem A is %d, elem B is %d",
        tid, dx, dy, elementA->tid, elementB->tid);

    // Let's use some temporary lists to do the split
    ElementList* listA = tree->listA;
    ElementList* listB = tree->listB;
    listA->removeAll();
    listB->removeAll();

    listA->add(elementA);
    listB->add(elementB);

    remove(elementA);
    remove(elementB);

    // For any remaining elements, insert it into the list
    // resulting in the smallest growth
    for (unsigned int i = 0; i < nbChildren; i++) {
        Node* node = children[i];
        int dA = listA->delta(node);
        int dB = listB->delta(node);

        if (dA < dB && listA->nbChildren < tree->maxChildren)
            listA->add(node);
        else if (dB < dA && listB->nbChildren < tree->maxChildren)
            listB->add(node);
        else {
            ElementList* smallestList =
                listA->nbChildren > listB->nbChildren ? listB : listA;
            smallestList->add(node);
        }
    }

    // Use the list to rebuild the nodes
    removeAll();
    for (unsigned int i = 0; i < listA->nbChildren; i++)
        add(listA->children[i]);

    Node* NN = new Node(tree);
    for (unsigned int i = 0; i < listB->nbChildren; i++)
        NN->add(listB->children[i]);

    return NN;
}

bool Node::isRoot()
{
    return tree->root == this;
}

void Node::adjustTree(Node* N, Node* NN)
{
    if (N->isRoot() && NN) {
        // build new root
        Node* root = new Node(tree);
        ALOGV("-> node %d created as new root", root->tid);
        root->add(N);
        root->add(NN);
        tree->root = root;
        return;
    }
    if (N->isRoot())
        return;

    if (NN && N->parent)
        N->parent->add(NN);
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
    for (unsigned int i = 0; i < nbChildren; i++)
    {
        children[i]->drawTree(level+1);
    }

#ifdef DEBUG
    if (gMaxLevel < level)
        gMaxLevel = level;

    if (!nbChildren)
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
      2*level, "", tid, minx, miny, maxx, maxy, maxx - minx, maxy - miny);
}

bool Node::overlap(int pminx, int pminy, int pmaxx, int pmaxy)
{
    return ! (pminx > maxx
           || pmaxx < minx
           || pmaxy < miny
           || pminy > maxy);
}

void Node::search(int minx, int miny, int maxx, int maxy, Vector<WebCore::RecordingData*>& list)
{
    if (isElement() && overlap(minx, miny, maxx, maxy))
        list.append(((Element*)this)->payload);

    for (unsigned int i = 0; i < nbChildren; i++) {
        if (children[i]->overlap(minx, miny, maxx, maxy))
            children[i]->search(minx, miny, maxx, maxy, list);
    }
}

//////////////////////////////////////////////////////////////////////
// Element

Element::Element(RTree* tree, int pminx, int pminy, int pmaxx, int pmaxy, WebCore::RecordingData* p)
    : Node(tree)
    , payload(p)
{
    minx = pminx;
    miny = pminy;
    maxx = pmaxx;
    maxy = pmaxy;
    ALOGV("-> New element %d (%d, %d) - (%d x %d)",
          tid, minx, miny, maxx-minx, maxy-miny);
}

Element::~Element()
{
    delete payload;
}

void Element::display(int level)
{
    ALOGV("%*selement %d (%d, %d, %d, %d) - (%d x %d)", 2*level, "",
          tid, minx, miny, maxx, maxy, maxx-minx, maxy-miny);
}

}
