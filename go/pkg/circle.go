package ringtree

import "sort"

// Circle interface defines the methods required for vNode storage and retrieval.
type Circle interface {
	Insert(vNodeHash uint64, nodeID string) bool
	FindClosest(vNodeHash uint64) (uint64, string)
	FindNextClosest(vNodeHash uint64) (uint64, string)
	Delete(vNodeHash uint64) bool
	Size() int
	Sort()
}

// RBTreeCircle implements the Circle interface using a red-black tree.
type RBTreeCircle struct {
	tree *redBlackTree
}

// ArrayCircle implements the Circle interface using an array.
type ArrayCircle struct {
	vNodes []VNode
}
type VNode struct {
	hash   uint64
	nodeID string
}

// Creates a New Circle with array or red-black tree.
func NewCircle(useArray bool) Circle {
	if useArray {
		return &ArrayCircle{
			vNodes: []VNode{},
		}
	} else {
		return &RBTreeCircle{
			tree: &redBlackTree{},
		}
	}
}

// Red-Black API.
func (rb *RBTreeCircle) Insert(vNodeHash uint64, nodeID string) bool {
	return rb.tree.Insert(vNodeHash, nodeID)
}
func (rb *RBTreeCircle) FindClosest(vNodeHash uint64) (uint64, string) {
	return rb.tree.FindClosest(vNodeHash)
}
func (rb *RBTreeCircle) FindNextClosest(vNodeHash uint64) (uint64, string) {
	return rb.tree.FindNextClosest(vNodeHash)
}
func (rbt *RBTreeCircle) Delete(vNodeHash uint64) bool {
	return rbt.tree.Delete(vNodeHash)
}
func (rbt *RBTreeCircle) Size() int {
	return rbt.tree.Size()
}
func (rb *RBTreeCircle) Sort() {
	// No-op for RBTreeCircle since it is always sorted
}
func (rb *RBTreeCircle) TraverseWhile(fn func(n *redBlackNode) bool) bool {
	return rb.tree.TraverseWhile(fn)
}
func (rb *RBTreeCircle) Root() (uint64, string, bool) {
	return rb.tree.root.key, rb.tree.root.value, true
}

// Array API.
func (ac *ArrayCircle) Insert(vNodeHash uint64, nodeID string) bool {
	// Check if vnode already exists
	for _, vnode := range ac.vNodes {
		if vnode.hash == vNodeHash {
			return false // Duplicate vnode
		}
	}
	ac.vNodes = append(ac.vNodes, VNode{hash: vNodeHash, nodeID: nodeID})
	return true
}

func (ac *ArrayCircle) FindClosest(vNodeHash uint64) (uint64, string) {
	if len(ac.vNodes) == 0 {
		return 0, ""
	}
	// Binary search for efficiency
	idx := sort.Search(len(ac.vNodes), func(i int) bool {
		return ac.vNodes[i].hash >= vNodeHash
	})
	if idx < len(ac.vNodes) {
		return ac.vNodes[idx].hash, ac.vNodes[idx].nodeID
	}
	// Wrap around to the first vnode
	return ac.vNodes[0].hash, ac.vNodes[0].nodeID
}

func (ac *ArrayCircle) FindNextClosest(vNodeHash uint64) (uint64, string) {
	if len(ac.vNodes) == 0 {
		return 0, ""
	}
	// Binary search for efficiency
	idx := sort.Search(len(ac.vNodes), func(i int) bool {
		return ac.vNodes[i].hash > vNodeHash
	})
	if idx < len(ac.vNodes) {
		return ac.vNodes[idx].hash, ac.vNodes[idx].nodeID
	}
	// Wrap around to the first vnode
	return ac.vNodes[0].hash, ac.vNodes[0].nodeID
}

func (ac *ArrayCircle) Delete(vNodeHash uint64) bool {
	for i, vnode := range ac.vNodes {
		if vnode.hash == vNodeHash {
			ac.vNodes = append(ac.vNodes[:i], ac.vNodes[i+1:]...)
			return true
		}
	}
	return false // Not found
}

func (ac *ArrayCircle) Size() int {
	return len(ac.vNodes)
}

func (ac *ArrayCircle) Sort() {
	sort.Slice(ac.vNodes, func(i, j int) bool {
		return ac.vNodes[i].hash < ac.vNodes[j].hash
	})
}
