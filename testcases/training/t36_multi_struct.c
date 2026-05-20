/* t36_multi_struct.c — Complex nested struct operations.
 * Pattern: high type complexity, struct field access chains, arrays of structs.
 */

struct Vec3 { float x, y, z; };
struct Color { unsigned char r, g, b, a; };
struct Material { struct Color diffuse, specular; float shininess; };
struct Vertex { struct Vec3 pos, normal; float u, v; };
struct Mesh { struct Vertex *verts; int count; struct Material mat; };
struct Transform { float matrix[4][4]; };
struct SceneObject { struct Mesh mesh; struct Transform transform; int visible; };

float vec3_dot(struct Vec3 a, struct Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

struct Vec3 vec3_cross(struct Vec3 a, struct Vec3 b) {
    struct Vec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

struct Vec3 vec3_add(struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}

struct Vec3 vec3_scale(struct Vec3 v, float s) {
    struct Vec3 r = { v.x * s, v.y * s, v.z * s };
    return r;
}

struct Vec3 transform_point(const struct Transform *t, struct Vec3 p) {
    struct Vec3 r;
    r.x = t->matrix[0][0]*p.x + t->matrix[0][1]*p.y + t->matrix[0][2]*p.z + t->matrix[0][3];
    r.y = t->matrix[1][0]*p.x + t->matrix[1][1]*p.y + t->matrix[1][2]*p.z + t->matrix[1][3];
    r.z = t->matrix[2][0]*p.x + t->matrix[2][1]*p.y + t->matrix[2][2]*p.z + t->matrix[2][3];
    return r;
}

void transform_mesh(struct SceneObject *obj) {
    if (!obj->visible) return;
    for (int i = 0; i < obj->mesh.count; i++) {
        obj->mesh.verts[i].pos = transform_point(&obj->transform, obj->mesh.verts[i].pos);
        obj->mesh.verts[i].normal = transform_point(&obj->transform, obj->mesh.verts[i].normal);
    }
}

float compute_lighting(struct Vec3 normal, struct Vec3 light_dir, struct Material mat) {
    float diffuse = vec3_dot(normal, light_dir);
    if (diffuse < 0) diffuse = 0;
    float specular = diffuse * diffuse; /* simplified */
    float spec_factor = 1.0f;
    for (int i = 0; i < (int)mat.shininess; i++) spec_factor *= specular;
    return diffuse * 0.8f + spec_factor * 0.2f;
}

struct Color blend_colors(struct Color a, struct Color b, unsigned char alpha) {
    struct Color r;
    r.r = (unsigned char)((a.r * (255 - alpha) + b.r * alpha) / 255);
    r.g = (unsigned char)((a.g * (255 - alpha) + b.g * alpha) / 255);
    r.b = (unsigned char)((a.b * (255 - alpha) + b.b * alpha) / 255);
    r.a = 255;
    return r;
}

int mesh_vertex_count(const struct SceneObject *objects, int n) {
    int total = 0;
    for (int i = 0; i < n; i++)
        if (objects[i].visible) total += objects[i].mesh.count;
    return total;
}
