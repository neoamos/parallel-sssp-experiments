/* -*- mode: c++ -*- */

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <thread>
#include <atomic>
#include <vector>
#include "simplegraph.h"
#include "lockfree_queue.h"
#include "Timer.h"

const int INF = INT_MAX;
int numidle = 0;
enum Color {WHITE, BLACK};

typedef SimpleCSRGraph<unsigned int, std::atomic<int>> SimpleCSRGraphUIAI;

void sssp_init(SimpleCSRGraphUIAI g, unsigned int src) {
  for(int i = 0; i < g.num_nodes; i++) {
    g.node_wt[i] = (i == src) ? 0 : INF;
  }
}

void sssp(SimpleCSRGraphUIAI g, MSQueue *q, int id, int numth) {
  int node;
  bool have_token = false;
  Color c = BLACK;

  while(true){
    while(q[id].pop(node)){
      if(node == -1){
        if(c == WHITE){
          numidle++;
          if(numidle >= numth) return;
          //printf("id: %d, idle: %d\n", id, numidle);
          q[(id+1)%numth].push(-1);
        }else{
          numidle = 0;
          have_token = true;
          continue;
        }
      }else{
        c = BLACK;
      }
      for(unsigned int e = g.row_start[node]; e < g.row_start[node + 1]; e++) {

        unsigned int dest = g.edge_dst[e];
        int distance = g.node_wt[node] + g.edge_wt[e];
        
        
        for(;;){
          int prev_distance = g.node_wt[dest];
          
          if(prev_distance <= distance) {
            break;
          }else if(g.node_wt[dest].compare_exchange_weak(prev_distance, distance)){
            q[(id+1)%numth].push(dest);
            break;
          }
        }

      }
    }
    if(numidle >= numth) return;
    if(have_token){
      have_token = false;
      c = WHITE;
      q[(id+1)%numth].push(-1);
    }
  }

}

void write_output(SimpleCSRGraphUIAI &g, const char *out) {
  FILE *fp; 
  
  fp = fopen(out, "w");
  if(!fp) {
    fprintf(stderr, "ERROR: Unable to open output file '%s'\n", out);
    exit(1);
  }

  for(int i = 0; i < g.num_nodes; i++) {
    int r;
    if(g.node_wt[i] == INF) {
      r = fprintf(fp, "%d INF\n", i);
    } else {
      r = fprintf(fp, "%d %d\n", i, (int)g.node_wt[i]);
    }

    if(r < 0) {
      fprintf(stderr, "ERROR: Unable to write output\n");
      exit(1);
    }
  }
}

int main(int argc, char *argv[]) 
{
  if(argc != 4) {
    fprintf(stderr, "Usage: %s inputgraph outputfile nuthreads\n", argv[0]);
    exit(1);
  }

  SimpleCSRGraphUIAI input;
  int numthreads = atoi(argv[3]);
  MSQueue *sq = new MSQueue[numthreads];
  std::vector<std::thread> ths;

  if(!input.load_file(argv[1])) {
    fprintf(stderr, "ERROR: failed to load graph\n");
    exit(1);
  } 

  //printf("Loaded '%s', %u nodes, %u edges\n", argv[1], input.num_nodes, input.num_edges);
  
  ggc::Timer t("sssp");

  int src = 0;

  /* no need to parallelize this */
  sssp_init(input, src);

  t.start();
  sq[0].push(src);
  sq[0].push(-1);
  for(int i = 0; i < numthreads; i++){
    ths.push_back(std::thread(&sssp, input, sq, i, numthreads));
  }
  for (auto& th : ths){
    th.join();
  }
  t.stop();

  //printf("Total time: %u ms\n", t.duration_ms());
  printf("%u", t.duration_ms());

  write_output(input, argv[2]);

  //printf("Wrote output '%s'\n", argv[2]);
  return 0;
}
