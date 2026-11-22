#include "Trie.h"

void Trie::insert(const std::string& word, const std::string& id) {
    Node* cur = root_.get();
    for (size_t i = 0; i < word.size(); ++i) {
        std::unique_ptr<Node>& child = cur->next[word[i]];
        if (!child) child.reset(new Node());
        cur = child.get();
        cur->ids.insert(id);
    }
}

std::set<std::string> Trie::withPrefix(const std::string& prefix) const {
    const Node* cur = root_.get();
    for (size_t i = 0; i < prefix.size(); ++i) {
        std::map<char, std::unique_ptr<Node> >::const_iterator it = cur->next.find(prefix[i]);
        if (it == cur->next.end()) return std::set<std::string>();
        cur = it->second.get();
    }
    return cur->ids;
}
