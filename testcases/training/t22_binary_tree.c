/* t22_binary_tree.c — Binary tree / BST operations.
 * Pattern: recursive traversal, pointer chasing, branching.
 */

struct TreeNode { int val; struct TreeNode *left, *right; };

int tree_height(struct TreeNode *root) {
    if (!root) return 0;
    int lh = tree_height(root->left);
    int rh = tree_height(root->right);
    return 1 + (lh > rh ? lh : rh);
}

int tree_size(struct TreeNode *root) {
    if (!root) return 0;
    return 1 + tree_size(root->left) + tree_size(root->right);
}

int tree_sum(struct TreeNode *root) {
    if (!root) return 0;
    return root->val + tree_sum(root->left) + tree_sum(root->right);
}

int tree_min(struct TreeNode *root) {
    if (!root) return 0x7FFFFFFF;
    while (root->left) root = root->left;
    return root->val;
}

int tree_max(struct TreeNode *root) {
    if (!root) return -0x7FFFFFFF;
    while (root->right) root = root->right;
    return root->val;
}

struct TreeNode *tree_search(struct TreeNode *root, int key) {
    while (root) {
        if (key == root->val) return root;
        root = (key < root->val) ? root->left : root->right;
    }
    return 0;
}

int tree_is_bst_helper(struct TreeNode *root, int min, int max) {
    if (!root) return 1;
    if (root->val <= min || root->val >= max) return 0;
    return tree_is_bst_helper(root->left, min, root->val) &&
           tree_is_bst_helper(root->right, root->val, max);
}

int tree_is_bst(struct TreeNode *root) {
    return tree_is_bst_helper(root, -0x7FFFFFFF, 0x7FFFFFFF);
}

int tree_count_leaves(struct TreeNode *root) {
    if (!root) return 0;
    if (!root->left && !root->right) return 1;
    return tree_count_leaves(root->left) + tree_count_leaves(root->right);
}

int tree_diameter(struct TreeNode *root) {
    if (!root) return 0;
    int lh = tree_height(root->left);
    int rh = tree_height(root->right);
    int ld = tree_diameter(root->left);
    int rd = tree_diameter(root->right);
    int through_root = lh + rh;
    int child_max = ld > rd ? ld : rd;
    return through_root > child_max ? through_root : child_max;
}
