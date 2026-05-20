/* t19_graph_algo.c — Graph algorithms on adjacency matrix.
 * Pattern: nested loops, conditional logic, multi-phase algorithms.
 */

#define GMAX 128
#define INF 999999

void floyd_warshall(int dist[GMAX][GMAX], int n) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (dist[i][k] + dist[k][j] < dist[i][j])
                    dist[i][j] = dist[i][k] + dist[k][j];
}

int dijkstra_min(const int *dist, const int *visited, int n) {
    int min_val = INF, min_idx = -1;
    for (int i = 0; i < n; i++)
        if (!visited[i] && dist[i] < min_val) {
            min_val = dist[i]; min_idx = i;
        }
    return min_idx;
}

void dijkstra(const int graph[GMAX][GMAX], int n, int src, int dist_out[GMAX]) {
    int visited[GMAX];
    for (int i = 0; i < n; i++) { dist_out[i] = INF; visited[i] = 0; }
    dist_out[src] = 0;
    for (int count = 0; count < n - 1; count++) {
        int u = dijkstra_min(dist_out, visited, n);
        if (u < 0) break;
        visited[u] = 1;
        for (int v = 0; v < n; v++)
            if (!visited[v] && graph[u][v] && dist_out[u] + graph[u][v] < dist_out[v])
                dist_out[v] = dist_out[u] + graph[u][v];
    }
}

int count_edges(const int adj[GMAX][GMAX], int n) {
    int edges = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (adj[i][j]) edges++;
    return edges;
}

void degree_sequence(const int adj[GMAX][GMAX], int n, int deg[GMAX]) {
    for (int i = 0; i < n; i++) {
        deg[i] = 0;
        for (int j = 0; j < n; j++)
            if (adj[i][j]) deg[i]++;
    }
}

int has_path_dfs(const int adj[GMAX][GMAX], int n, int src, int dst,
                 int visited[GMAX]) {
    if (src == dst) return 1;
    visited[src] = 1;
    for (int i = 0; i < n; i++)
        if (adj[src][i] && !visited[i])
            if (has_path_dfs(adj, n, i, dst, visited)) return 1;
    return 0;
}
