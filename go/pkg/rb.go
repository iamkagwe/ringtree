package ringtree

// redBlackTree is an implementation of a Red-Black Tree
type redBlackTree struct {
	root *redBlackNode
	size int
}

// redBlackNode is a node of the redBlackTree
type redBlackNode struct {
	key   uint64 // vNodeHash
	value string // nodeID
	left  *redBlackNode
	right *redBlackNode
	red   bool
}

func (t *redBlackTree) Size() int {
	return t.size
}

func (n *redBlackNode) Child(right bool) *redBlackNode {
	if right {
		return n.right
	}
	return n.left
}

// findMin returns the node with the minimum key in the subtree rooted at h.
func findMin(h *redBlackNode) *redBlackNode {
	if h == nil {
		return nil
	}
	for h.left != nil {
		h = h.left
	}
	return h
}

// Inserts a value into the tree with a given key key.
// Returns true on successful insertion, false if duplicate exists.
func (t *redBlackTree) Insert(key uint64, value string) (ret bool) {
	if t.root == nil {
		t.root = &redBlackNode{
			key:   key,
			value: value,
		}
		ret = true
	} else {
		var head = &redBlackNode{}

		var dir = true
		var last = true

		var parent *redBlackNode  // parent
		var gparent *redBlackNode // grandparent
		var ggparent = head       // great grandparent
		var node = t.root

		ggparent.right = t.root

		for {
			if node == nil {
				// insert new node at bottom
				node = &redBlackNode{
					key:   key,
					value: value,
					red:   true,
				}
				parent.setChild(dir, node)
				ret = true
			} else if isRed(node.left) && isRed(node.right) {
				// flip colors
				node.red = true
				node.left.red, node.right.red = false, false
			}

			// fix red violation
			if isRed(node) && isRed(parent) {
				dir2 := ggparent.right == gparent

				if node == parent.Child(last) {
					ggparent.setChild(dir2, singleRotate(gparent, !last))
				} else {
					ggparent.setChild(dir2, doubleRotate(gparent, !last))
				}
			}

			if node.key == key {
				// Duplicate key, stop insertion
				break
			}

			last = dir
			dir = key > node.key

			// update helpers
			if gparent != nil {
				ggparent = gparent
			}
			gparent = parent
			parent = node

			node = node.Child(dir)
		}

		t.root = head.right
	}

	// make root black
	t.root.red = false

	if ret {
		t.size++
	}

	return ret
}

// Delete removes the entry for key from the redBlackTree. Returns true on successful deletion, false if the key is not in the tree.
func (t *redBlackTree) Delete(key uint64) bool {
	if t.root == nil {
		return false
	}

	var head = &redBlackNode{red: true} // fake red node to push down
	var node = head
	var parent *redBlackNode  //parent
	var gparent *redBlackNode //grandparent
	var found *redBlackNode

	var dir = true

	node.right = t.root

	for node.Child(dir) != nil {
		last := dir

		// update helpers
		gparent = parent
		parent = node
		node = node.Child(dir)

		if node.key == key {
			found = node
		}

		dir = key > node.key

		// pretend to push red node down
		if !isRed(node) && !isRed(node.Child(dir)) {
			if isRed(node.Child(!dir)) {
				sr := singleRotate(node, dir)
				parent.setChild(last, sr)
				parent = sr
			} else {
				sibling := parent.Child(!last)
				if sibling != nil {
					if !isRed(sibling.Child(!last)) && !isRed(sibling.Child(last)) {
						// flip colors
						parent.red = false
						sibling.red, node.red = true, true
					} else {
						dir2 := gparent.right == parent

						if isRed(sibling.Child(last)) {
							gparent.setChild(dir2, doubleRotate(parent, last))
						} else if isRed(sibling.Child(!last)) {
							gparent.setChild(dir2, singleRotate(parent, last))
						}

						gpc := gparent.Child(dir2)
						gpc.red = true
						node.red = true
						gpc.left.red, gpc.right.red = false, false
					}
				}
			}
		}
	}

	// get rid of node if we've found one
	if found != nil {
		found.key = node.key
		found.value = node.value
		parent.setChild(parent.right == node, node.Child(node.left == nil))
		t.size--
	}

	t.root = head.right
	if t.root != nil {
		t.root.red = false
	}

	return found != nil
}

// Find returns the value associated with the closest node responsible for the given key.
func (t *redBlackTree) Find(key uint64) interface{} {
	h := t.root
	for h != nil {
		if key < h.key {
			h = h.left
		} else if key > h.key {
			h = h.right
		} else {
			return h.value
		}
	}
	return nil
}

// FindClosest finds the closest node greater than or equal to the key and returns both the key and the value.
func (t *redBlackTree) FindClosest(key uint64) (uint64, string) {
	if t.root == nil {
		return 0, ""
	}

	h := t.root
	var closest *redBlackNode
	for h != nil {
		if key < h.key {
			closest = h
			h = h.left
		} else if key > h.key {
			h = h.right
		} else {
			return h.key, h.value
		}
	}

	if closest == nil {
		closest = findMin(t.root)
	}

	return closest.key, closest.value
}

func (t *redBlackTree) FindNextClosest(key uint64) (uint64, string) {
	var nextNode *redBlackNode
	currentNode := t.root

	for currentNode != nil {
		if key < currentNode.key {
			// Candidate for the next closest
			nextNode = currentNode
			currentNode = currentNode.left
		} else {
			// Continue searching in the right subtree
			currentNode = currentNode.right
		}
	}

	// If no larger key is found, wrap around to the smallest key in the tree
	if nextNode == nil {
		minNode := findMin(t.root)
		return minNode.key, minNode.value
	}

	return nextNode.key, nextNode.value
}

func (t *redBlackTree) TraverseWhile(condition func(*redBlackNode) bool) bool {
	// If the tree is empty, there's nothing to traverse
	if t.root == nil {
		return true
	}

	// Helper function for recursive traversal
	var traverseWhile func(*redBlackNode, func(*redBlackNode) bool) bool
	traverseWhile = func(n *redBlackNode, condition func(*redBlackNode) bool) bool {
		if n == nil {
			// Reached the end of a branch
			return true
		}

		// Traverse left
		if n.left != nil {
			if !traverseWhile(n.left, condition) {
				// Stop traversal if condition indicates to stop
				return false
			}
		}

		// Check the current node
		if !condition(n) {
			// Stop traversal if condition indicates to stop
			return false
		}

		// Traverse right
		if n.right != nil {
			if !traverseWhile(n.right, condition) {
				// Stop traversal if condition indicates to stop
				return false
			}
		}

		// Continue traversal
		return true
	}

	// Start traversing from the root
	return traverseWhile(t.root, condition)
}

func (n *redBlackNode) setChild(right bool, node *redBlackNode) {
	if right {
		n.right = node
	} else {
		n.left = node
	}
}

func isRed(node *redBlackNode) bool {
	return node != nil && node.red
}

func singleRotate(oldroot *redBlackNode, dir bool) *redBlackNode {
	newroot := oldroot.Child(!dir)

	oldroot.setChild(!dir, newroot.Child(dir))
	newroot.setChild(dir, oldroot)

	oldroot.red = true
	newroot.red = false

	return newroot
}

func doubleRotate(root *redBlackNode, dir bool) *redBlackNode {
	root.setChild(!dir, singleRotate(root.Child(!dir), !dir))
	return singleRotate(root, dir)
}
