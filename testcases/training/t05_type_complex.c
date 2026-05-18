/* t05_type_complex.c
 *
 * Complexity pattern: deeply nested aggregate types — structs containing
 * fixed arrays of other structs, plus multi-level pointers (float **).
 *
 * Why it is interesting to the model: functions that move data through
 * complex types generate more work for type-based reasoning and alias
 * analysis than functions over plain scalars. This is a MEDIUM-tier
 * sample that isolates type complexity from loop/branch complexity.
 */

struct Inner {
    int *p;
    int count;
};

struct Middle {
    struct Inner items[4];
    double weight;
};

typedef struct {
    struct Middle layers[2];
    float **matrix;
    int depth;
} Outer;

int inner_total(const struct Inner *in) {
    int t = 0;
    for (int i = 0; i < in->count; i++)
        t += in->p[i];
    return t;
}

double outer_weight(const Outer *o) {
    double w = 0.0;
    for (int i = 0; i < 2; i++)
        w += o->layers[i].weight;
    return w;
}

float matrix_elem(const Outer *o, int row, int col) {
    if (row < 0 || col < 0 || row >= o->depth)
        return 0.0f;
    return o->matrix[row][col];
}
