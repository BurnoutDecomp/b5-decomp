#pragma once

#include "types.hpp"
#include <new>
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"

namespace Attrib
{
// The EASTL map instantiation used by LiveLink in ARTIST. Kept local to this
// runtime instead of including a later EASTL with a different platform/ABI.
// DecFIGS red_black_tree.h names these fields; ARTIST 8280FD10 initializes the
// anchor at +4, size at +14 and the 24-byte allocator at +18 (table size 30).
// Only the operations instantiated by the ARTIST LiveLink caller are present.
template <class Key, class Record>
class LiveLinkMap
{
public:
    struct key_compare
    {
        bool operator()(const Key& lhs, const Key& rhs) const { return lhs < rhs; }
    };

    struct node_base
    {
        node_base* mpNodeRight;
        node_base* mpNodeLeft;
        node_base* mpNodeParent;
        char mColor; // red=0, black=1; the anchor remains red
    };

    struct value_type
    {
        const Key first;
        Record second;
        value_type(const Key& key, const Record& record) : first(key), second(record) {}
    };

    struct node_type : node_base
    {
        value_type mValue;
    };

    struct iterator
    {
        node_base* mpNode;
        explicit iterator(node_base* node = NULL) : mpNode(node) {}
        value_type* operator->() const { return &static_cast<node_type*>(mpNode)->mValue; }
        bool operator==(const iterator& rhs) const { return mpNode == rhs.mpNode; }
        bool operator!=(const iterator& rhs) const { return mpNode != rhs.mpNode; }
    };

    struct insert_return_type
    {
        iterator first;
        bool second;
        insert_return_type(iterator position, bool inserted) : first(position), second(inserted) {}
    };

    typedef CgsAttribSys::AttribSysPackageAllocator allocator_type;

    explicit LiveLinkMap(const allocator_type& allocator = allocator_type())
        : mnSize(0), mAllocator(allocator)
    {
        mAnchor.mpNodeRight = &mAnchor;
        mAnchor.mpNodeLeft = &mAnchor;
        mAnchor.mpNodeParent = NULL;
        mAnchor.mColor = 0;
    }

    iterator end() { return iterator(&mAnchor); }

    // ARTIST 828071D0: lower_bound traversal followed by the reverse comparison.
    iterator find(const Key& key)
    {
        node_base* candidate = &mAnchor;
        node_base* node = mAnchor.mpNodeParent;
        while (node != NULL)
        {
            if (mCompare(static_cast<node_type*>(node)->mValue.first, key))
                node = node->mpNodeRight;
            else
            {
                candidate = node;
                node = node->mpNodeLeft;
            }
        }
        if (candidate == &mAnchor || mCompare(key, static_cast<node_type*>(candidate)->mValue.first))
            return end();
        return iterator(candidate);
    }

    // ARTIST 8280F328: unique insertion checks the preceding key when the search
    // ended on a left edge. A duplicate returns its node without allocating.
    insert_return_type insert(const value_type& value)
    {
        node_base* parent = &mAnchor;
        node_base* node = mAnchor.mpNodeParent;
        bool left = true;
        while (node != NULL)
        {
            parent = node;
            left = mCompare(value.first, static_cast<node_type*>(node)->mValue.first);
            node = left ? node->mpNodeLeft : node->mpNodeRight;
        }
        node_base* previous = parent;
        if (left)
        {
            if (parent == mAnchor.mpNodeLeft)
                return insert_return_type(DoInsert(parent, value), true);
            previous = Decrement(parent);
        }
        if (mCompare(static_cast<node_type*>(previous)->mValue.first, value.first))
            return insert_return_type(DoInsert(parent, value), true);
        return insert_return_type(iterator(previous), false);
    }

    // Original storage, including the non-empty rb_base's comparator member.
    key_compare mCompare;
    node_base mAnchor;
    unsigned int mnSize;
    allocator_type mAllocator;

private:
    // ARTIST 82B445E8.
    static node_base* Decrement(node_base* node)
    {
        node_base* parent = node->mpNodeParent;
        if (parent->mpNodeParent == node && node->mColor == 0)
            return node->mpNodeRight;
        if (node->mpNodeLeft != NULL)
        {
            node = node->mpNodeLeft;
            while (node->mpNodeRight != NULL)
                node = node->mpNodeRight;
            return node;
        }
        while (node == parent->mpNodeLeft)
        {
            node = parent;
            parent = parent->mpNodeParent;
        }
        return parent;
    }

    // ARTIST 82B44660 / 82B446C8 return the possibly changed root.
    static node_base* RotateLeft(node_base* node, node_base* root)
    {
        node_base* right = node->mpNodeRight;
        node->mpNodeRight = right->mpNodeLeft;
        if (right->mpNodeLeft != NULL)
            right->mpNodeLeft->mpNodeParent = node;
        right->mpNodeParent = node->mpNodeParent;
        if (node == root)
            root = right;
        else if (node == node->mpNodeParent->mpNodeLeft)
            node->mpNodeParent->mpNodeLeft = right;
        else
            node->mpNodeParent->mpNodeRight = right;
        right->mpNodeLeft = node;
        node->mpNodeParent = right;
        return root;
    }

    static node_base* RotateRight(node_base* node, node_base* root)
    {
        node_base* left = node->mpNodeLeft;
        node->mpNodeLeft = left->mpNodeRight;
        if (left->mpNodeRight != NULL)
            left->mpNodeRight->mpNodeParent = node;
        left->mpNodeParent = node->mpNodeParent;
        if (node == root)
            root = left;
        else if (node == node->mpNodeParent->mpNodeRight)
            node->mpNodeParent->mpNodeRight = left;
        else
            node->mpNodeParent->mpNodeLeft = left;
        left->mpNodeRight = node;
        node->mpNodeParent = left;
        return root;
    }

    // ARTIST 82B44730: attach a red leaf, update both anchor extrema, then
    // recolor/rotate until the root or a black parent is reached.
    void InsertNode(node_base* node, node_base* parent, bool right)
    {
        node->mpNodeParent = parent;
        node->mpNodeRight = NULL;
        node->mpNodeLeft = NULL;
        node->mColor = 0;
        if (!right)
        {
            parent->mpNodeLeft = node;
            if (parent == &mAnchor)
            {
                mAnchor.mpNodeParent = node;
                mAnchor.mpNodeRight = node;
            }
            else if (parent == mAnchor.mpNodeLeft)
                mAnchor.mpNodeLeft = node;
        }
        else
        {
            parent->mpNodeRight = node;
            if (parent == mAnchor.mpNodeRight)
                mAnchor.mpNodeRight = node;
        }
        while (node != mAnchor.mpNodeParent && node->mpNodeParent->mColor == 0)
        {
            parent = node->mpNodeParent;
            node_base* grandparent = parent->mpNodeParent;
            if (parent == grandparent->mpNodeLeft)
            {
                node_base* uncle = grandparent->mpNodeRight;
                if (uncle != NULL && uncle->mColor == 0)
                {
                    parent->mColor = 1;
                    uncle->mColor = 1;
                    grandparent->mColor = 0;
                    node = grandparent;
                }
                else
                {
                    if (node == parent->mpNodeRight)
                    {
                        node = parent;
                        mAnchor.mpNodeParent = RotateLeft(node, mAnchor.mpNodeParent);
                    }
                    node->mpNodeParent->mColor = 1;
                    grandparent->mColor = 0;
                    mAnchor.mpNodeParent = RotateRight(grandparent, mAnchor.mpNodeParent);
                }
            }
            else
            {
                node_base* uncle = grandparent->mpNodeLeft;
                if (uncle != NULL && uncle->mColor == 0)
                {
                    parent->mColor = 1;
                    uncle->mColor = 1;
                    grandparent->mColor = 0;
                    node = grandparent;
                }
                else
                {
                    if (node == parent->mpNodeLeft)
                    {
                        node = parent;
                        mAnchor.mpNodeParent = RotateRight(node, mAnchor.mpNodeParent);
                    }
                    node->mpNodeParent->mColor = 1;
                    grandparent->mColor = 0;
                    mAnchor.mpNodeParent = RotateLeft(grandparent, mAnchor.mpNodeParent);
                }
            }
        }
        mAnchor.mpNodeParent->mColor = 1;
    }

    // ARTIST 8280F120 + 8280EB08. The allocator retained in the container is
    // not the allocation receiver: the original EASTL macro uses the global
    // EASTL package allocator. Its Malloc performs the original failure assert.
    iterator DoInsert(node_base* parent, const value_type& value)
    {
        const bool right = parent != &mAnchor
            && !mCompare(value.first, static_cast<node_type*>(parent)->mValue.first);
        node_type* node = static_cast<node_type*>(
            CgsAttribSys::AttribSysMemoryManager::GetEaStlAllocator()->Malloc(sizeof(node_type), 0));
        ::new (static_cast<void*>(&node->mValue)) value_type(value);
        InsertNode(node, parent, right);
        ++mnSize;
        return iterator(node);
    }
};
}
