#ifndef TRIE_H
#define TRIE_H

#include <map>
#include <memory>
#include <set>
#include <string>

// Prefix tree used by `minigit search`. Every node keeps the set of commit ids
// whose text contains a word passing through that node, so a prefix lookup
// costs O(length of the prefix) no matter how many commits exist.
class Trie {
public:
    Trie() : root_(new Node()) {}
    void insert(const std::string& word, const std::string& id);
    std::set<std::string> withPrefix(const std::string& prefix) const;

private:
    struct Node {
        std::map<char, std::unique_ptr<Node> > next;
        std::set<std::string> ids;
    };
    std::unique_ptr<Node> root_;
};

#endif
