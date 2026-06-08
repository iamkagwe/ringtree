package main

import (
	"flag"
	"fmt"
	"log"
	"strings"

	"github.com/iamkagwe/ringtree"
)

// simple CLI to experiment with the ring tree implementation.
// Example:
//
//	go run ./cmd/ringtree --nodes a,b,c --keys user:1,user:2
func main() {
	nodesFlag := flag.String("nodes", "", "comma-separated list of node IDs")
	keysFlag := flag.String("keys", "", "comma-separated list of keys to map")
	flag.Parse()

	if *nodesFlag == "" || *keysFlag == "" {
		flag.Usage()
		return
	}

	nodes := strings.Split(*nodesFlag, ",")
	keys := strings.Split(*keysFlag, ",")

	// TODO: expose a constructor that accepts options/config.
	tree, err := ringtree.New(5)
	if err != nil {
		log.Fatalf("failed to create ring tree: %v", err)
	}

	for _, n := range nodes {
		if strings.TrimSpace(n) == "" {
			continue
		}
		if err := tree.AddNode(n, 1.0); err != nil {
			log.Fatalf("failed to add node %q: %v", n, err)
		}
	}

	for _, k := range keys {
		if strings.TrimSpace(k) == "" {
			continue
		}
		node, err := tree.GetNode([]byte(k))
		if err != nil {
			log.Fatalf("failed to get node for key %q: %v", k, err)
		}
		fmt.Printf("%s -> %s\n", k, node)
	}
}
