package ringtree

import (
	"fmt"
	"os"
	"sync"
	"testing"
)

// Recursive function to populate the ring tree until all nodes are at the bottom level.
/*func populate(r *Ring, maxDepth int) {
	if r.level == maxDepth {
		// Base case: Do not split further, insert nodes directly.
		for i := 0; i < r.maxCount; i++ {
			r.InsertNode(NewNode(100000))
		}
		return
	}

	// Insert nodes and split if maxCount is reached.
	for i := 0; i < r.maxCount; i++ {
		r.InsertNode(NewNode(100000))

		// If the ring is full, split it into a new subring.
		if len(r.members) >= r.maxCount {
			newSubring := r.SplitNode()
			PopulateRingTree(newSubring, maxDepth)
		}
	}
}*/

func checkNum(num, expected int, t *testing.T) {
	if num != expected {
		t.Errorf("got %d, expected %d", num, expected)
	}
}

func TestMain(m *testing.M) {
	// Open the file for writing test output
	file, err := os.Create("../test_output" + ".txt")
	if err != nil {
		fmt.Println("Error creating file:", err)
		os.Exit(1)
	}
	defer file.Close()

	// Redirect stdout to the file
	os.Stdout = file

	// Run the tests
	exitVal := m.Run()

	// After tests finish, close the file and exit
	os.Exit(exitVal)
}

func TestNew(t *testing.T) {
	rt := New(5)
	if rt == nil {
		t.Errorf("expected ring, got nil")
		return
	}
	checkNum(rt.maxCount, 5, t)
	checkNum(rt.Size(), 0, t)
	checkNum(rt.circle.Size(), 0, t)
}

func TestInsertNode(t *testing.T) {
	rt := New(5)
	node := NewNode("", 10)
	err := rt.InsertNode(node)
	if err != nil {
		t.Errorf("expected node to be added, got error: %v", err)
	}

	checkNum(rt.Size(), 1, t)
	checkNum(rt.circle.Size(), NumReplicas, t)
}

func TestRemoveNode(t *testing.T) {
	rt := New(5)
	node := NewNode("", 10)
	rt.InsertNode(node)
	node = NewNode("", 10)
	rt.InsertNode(node)

	checkNum(rt.Size(), 2, t)
	checkNum(rt.circle.Size(), 2*NumReplicas, t)

	rt.RemoveNode(node)

	checkNum(rt.Size(), 1, t)
	checkNum(rt.circle.Size(), NumReplicas, t)
}

func TestRemoveNodeWithKeys(t *testing.T) {
	rt := New(5)
	node := NewNode("", 10)
	rt.InsertNode(node)
	node = NewNode("", 10)
	rt.InsertNode(node)

	for i := 0; i < 10; i++ {
		key, _ := GenerateRandomString(20)
		err := rt.InsertKey(key)
		if err != nil {
			t.Fatalf("expected key %s to be inserted, got error: %v", key, err)
		}
	}
	rt.RemoveNode(node)

	checkNum(rt.Size(), 1, t)
	checkNum(rt.circle.Size(), NumReplicas, t)
}

func TestInsertNodeExceedingMaxCount(t *testing.T) {
	rt := New(2)
	nodeA := NewNode("", 10)
	nodeB := NewNode("", 10)
	nodeC := NewNode("", 10)

	// Add the first two nodes (should succeed)
	err := rt.InsertNode(nodeA)
	if err != nil {
		t.Errorf("unexpected error: %v", err)
	}
	err = rt.InsertNode(nodeB)
	if err != nil {
		t.Errorf("unexpected error: %v", err)
	}

	// Try to add a second node (should fail)
	err = rt.InsertNode(nodeC)
	if err == nil {
		t.Errorf("expected error when adding more nodes than maxCount")
	}
}

func TestInsertKey(t *testing.T) {
	rt := New(5)
	node := NewNode("", 2)
	rt.InsertNode(node)

	err := rt.InsertKey("key1")
	if err != nil {
		t.Errorf("expected key to be inserted, got error: %v", err)
	}
	err = rt.InsertKey("key2")
	if err != nil {
		t.Errorf("expected key to be inserted, got error: %v", err)
	}
	if node.load != 2 {
		t.Errorf("expected node load to be 2, got %d", node.load)
	}
}

func TestInsertManyKeys(t *testing.T) {
	rt := New(7)
	node := NewNode("", 70)
	rt.InsertNode(node)

	for i := 0; i < 100000; i++ {
		key, _ := GenerateRandomString(20)
		err := rt.InsertKey(key)
		if err != nil {
			t.Fatalf("expected key %s to be inserted, got error: %v", key, err)
		}
	}
	PrintOperationTimeStats()
	logMemoryUsage("InsertNode")
}

func TestLookup(t *testing.T) {
	// Change to larger than num keys to get a flat ring
	rt := New(3)
	node := NewNode("", 50)
	rt.InsertNode(node)

	var keys []string

	for i := 0; i < 10000; i++ {
		key, _ := GenerateRandomString(20)
		keys = append(keys, key)
		err := rt.InsertKey(key)
		if err != nil {
			t.Fatalf("expected key %s to be inserted, got error: %v", key, err)
		}
	}

	fmt.Printf("\n\nSearching...\n\n")

	for i := 0; i < 10000; i++ {
		_, err := rt.Lookup(keys[i])
		if err != nil {
			t.Fatalf("expected key %s to be found, got error: %v", keys[i], err)
		}
	}

	PrintOperationTimeStats()

}

func TestRemoveKey(t *testing.T) {
	// Initialize a new RingTree with a maxCount of 2
	rt := New(2)
	nodeA := NewNode("", 5)
	nodeB := NewNode("", 5)

	// Insert nodes into the ring
	err := rt.InsertNode(nodeA)
	if err != nil {
		t.Fatalf("expected nodeA to be inserted, got error: %v", err)
	}

	err = rt.InsertNode(nodeB)
	if err != nil {
		t.Fatalf("expected nodeB to be inserted, got error: %v", err)
	}

	// Insert keys into the nodes
	keys := []string{"key1", "key2", "key3", "key4", "key5"}
	for _, key := range keys {
		err := rt.InsertKey(key)
		if err != nil {
			t.Fatalf("expected key %s to be inserted, got error: %v", key, err)
		}
	}

	// Ensure that the keys are inserted correctly
	for _, key := range keys {
		_, err := rt.Lookup(key)
		if err != nil {
			t.Fatalf("expected key %s to be found, got error: %v", key, err)
		}
	}

	// Remove a key and verify it's removed correctly
	keyToRemove := "key2"
	err = rt.RemoveKey(keyToRemove)
	if err != nil {
		t.Fatalf("expected key %s to be removed, got error: %v", keyToRemove, err)
	}

	// Ensure that the key is no longer present via lookup
	_, err = rt.Lookup(keyToRemove)
	if err == nil {
		t.Fatalf("expected key %s to not be found", keyToRemove)
	}
}

// **********FAILS SOMETIMES
func TestCollapseRing(t *testing.T) {
	d := 7
	rt := New(d)
	node := NewNode("", 70)
	rt.InsertNode(node)

	var keys []string

	for i := 0; i < 100000; i++ {
		key, _ := GenerateRandomString(20)
		keys = append(keys, key)
		err := rt.InsertKey(key)
		if err != nil {
			t.Fatalf("expected key %s to be inserted, got error: %v", key, err)
		}
	}

	fmt.Printf("\n\nRemoving...\n\n")

	for i := 0; i < 1000000; i++ {
		err := rt.RemoveKey(keys[i])
		if err != nil {
			t.Fatalf("expected key %s to be removed, got error: %v", keys[i], err)
		}
	}

	for i := 0; i < 1000000; i++ {
		_, err := rt.Lookup(keys[i])
		if err == nil {
			t.Fatalf("expected key %s to not be found", keys[i])
		}
	}

	PrintOperationTimeStats()
	checkNum(rt.Size(), d, t)
}

// **********FAILS SOMETIMES
func TestRemapKeys(t *testing.T) {
	// Create a new ring with maxCount of 2
	rt := New(2)

	// Insert initial node (nodeA)
	nodeA := NewNode("", 2) // Threshold of 2 keys
	rt.InsertNode(nodeA)

	// Insert some keys into the initial node
	rt.InsertKey("key1-12345")
	rt.InsertKey("key2-23456")

	// Ensure that the keys are in nodeA before remapping
	if nodeA.load != 2 {
		t.Errorf("expected nodeA to have 2 keys before remapping, got %d", len(nodeA.keys))
	}

	// Insert a new node (nodeB) which should trigger remapping of some keys
	nodeB := NewNode("", 2)
	rt.InsertNode(nodeB)

	// Check if some keys were remapped to nodeB
	if nodeB.load == 0 {
		t.Errorf("expected some keys to be remapped to nodeB, but found none")
	}

	// Verify that nodeA has fewer keys after remapping
	if nodeA.load == 2 {
		t.Errorf("expected some keys to be removed from nodeA, but found 2")
	}

	fmt.Println("RemapKeys test completed successfully")
}

func TestInsertKeyOverflow(t *testing.T) {
	rt := New(2) // maxCount of 2 physical nodes
	nodeA := NewNode("", 1)
	rt.InsertNode(nodeA)

	// Insert key1 (within the threshold)
	err := rt.InsertKey("key1-239223")
	if err != nil {
		t.Errorf("expected key1 to be inserted, got error: %v", err)
	}
	if nodeA.load != 1 {
		t.Errorf("expected nodeA load to be 1 after inserting key1, got %d", nodeA.load)
	}

	// Insert key2 (should trigger the addition of a new physical node)
	err = rt.InsertKey("key2-fgwrgey2")
	if err != nil {
		t.Errorf("expected key2 to be inserted after adding a new node, got error: %v", err)
	}
	if len(rt.members) != 2 {
		t.Errorf("expected 2 physical nodes, got %d", len(rt.members))
	}

	// Insert key3 (should trigger a node to overflow and create a subring)
	err = rt.InsertKey("key3-43t34y3")
	if err != nil {
		t.Errorf("expected key3 to be inserted after overflow, got error: %v", err)
	}

	// Dynamically check which node has become a subring
	var subring *Ring

	switch circle := rt.circle.(type) {
	case *RBTreeCircle:
		// Use TraverseWhile for RBTreeCircle
		circle.TraverseWhile(func(n *redBlackNode) bool {
			if s, ok := rt.members[n.value].(*Ring); ok {
				subring = s
				return false // Stop the traversal once we find the subring
			}
			return true
		})
	case *ArrayCircle:
		// Traverse the vNodes array for ArrayCircle
		for _, vnode := range circle.vNodes {
			if s, ok := rt.members[vnode.nodeID].(*Ring); ok {
				subring = s
				break // Stop once we find the subring
			}
		}
	default:
		panic("Unknown Circle implementation")
	}

	if subring == nil {
		fmt.Println("No subring found.")
	} else {
		fmt.Printf("Found subring: %s\n", subring.id)
	}

	if subring == nil {
		t.Errorf("expected a subring to be created after overflow, but none was found")
		return
	}

	if len(subring.members) != 2 {
		t.Errorf("expected subring to contain 2 nodes, got %d", len(subring.members))
	}
}

func TestSubringCreation(t *testing.T) {
	rt := New(1)
	node := NewNode("", 1)
	rt.InsertNode(node)

	rt.InsertKey("key1")
	rt.InsertKey("key2")
	rt.InsertKey("key2")
	rt.InsertKey("key2")

	// After exceeding the threshold, the node should become a subring
	var rootNodeID string

	switch circle := rt.circle.(type) {
	case *RBTreeCircle:
		// Fetch the root from the RBTreeCircle
		if circle.tree.root != nil {
			rootNodeID = circle.tree.root.value
		}
	case *ArrayCircle:
		// Fetch the first node in the sorted array as the "root"
		if len(circle.vNodes) > 0 {
			rootNodeID = circle.vNodes[0].nodeID
		}
	default:
		panic("Unknown Circle implementation")
	}

	if rootNodeID == "" {
		t.Errorf("expected node to become a subring after overflow, but no root node found")
		return
	}

	if _, ok := rt.members[rootNodeID].(*Ring); !ok {
		t.Errorf("expected node to become a subring after overflow")
	}
}

func TestFindNode(t *testing.T) {
	rt := New(5)
	nodeA := NewNode("", 10)
	nodeB := NewNode("", 10)
	rt.InsertNode(nodeA)
	rt.InsertNode(nodeB)

	nodeOrSubring, _, _, _, err := rt.FindNode("key1")
	if err != nil {
		t.Errorf("expected node to be found")
	}
	if nodeOrSubring == nil {
		t.Errorf("expected node, got nil")
	}
}

func TestAddCollision(t *testing.T) {
	rt := New(5)
	rt.InsertNode(NewNode("", 5))
	rt.InsertNode(NewNode("", 5))

	err := rt.InsertKey("collisionKey")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	// This will not fail unless explicit collision detection is added
	err = rt.InsertKey("collisionKey")
	if err != nil {
		t.Errorf("expected key to be inserted without collision handling, got error: %v", err)
	}
}

func TestGossip(t *testing.T) {
	rt := New(3)
	rt.InsertNode(NewNode("", 5))

	for i := 0; i < 1000; i++ {
		key, _ := GenerateRandomString(20)
		err := rt.InsertKey(key)
		if err != nil {
			t.Fatalf("expected key %s to be inserted, got error: %v", key, err)
		}
	}

	var wg sync.WaitGroup
	wg.Add(1)
	go rt.Gossip("hi", &wg)
	wg.Wait()

	PrintHierarchyDetails(rt)
	PrintOperationTimeStats()
}

func TestGossipFromLeaf(t *testing.T) {
	rt := New(3)
	rt.InsertNode(NewNode("", 5))

	for i := 0; i < 100; i++ {
		key, _ := GenerateRandomString(20)
		err := rt.InsertKey(key)
		if err != nil {
			t.Fatalf("expected key %s to be inserted, got error: %v", key, err)
		}
	}

	// Find a leaf ring (no subrings as members)
	var leafRing *Ring
	var findLeaf func(r *Ring)
	findLeaf = func(r *Ring) {
		hasSubring := false
		for _, member := range r.members {
			if sub, ok := member.(*Ring); ok {
				hasSubring = true
				findLeaf(sub)
			}
		}
		if !hasSubring {
			leafRing = r
		}
	}
	findLeaf(rt)

	if leafRing == nil {
		t.Fatalf("no leaf ring found")
	}

	var wg sync.WaitGroup
	wg.Add(1)
	go leafRing.Gossip("gossip from leaf ring", &wg)
	wg.Wait()

	PrintHierarchyDetails(rt)
	PrintOperationTimeStats()
}

func TestTraversal(t *testing.T) {
	rt := New(5)
	rt.InsertNode(NewNode("", 100))

	for i := 0; i < 100000; i++ {
		key, _ := GenerateRandomString(20)
		err := rt.InsertKey(key)
		if err != nil {
			t.Fatalf("expected key %s to be inserted, got error: %v", key, err)
		}
	}

	numK := 0
	rt.Traversal(func(node *Node) { fmt.Println(node.load) })
	fmt.Println(numK)
}
