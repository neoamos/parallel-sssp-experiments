/* -*- mode: c++ -*- */

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <math.h>
#include "simplegraph.h"
#include "Timer.h"

const int INF = INT_MAX;

typedef SimpleCSRGraph<unsigned int, std::atomic<int>> SimpleCSRGraphUIAI;
std::mutex *node_lock;
std::mutex m1;
std::mutex m2;

void sssp_init_work(SimpleCSRGraphUIAI g, unsigned int src, int start, int end){
  for(int i = start; i < end; i++) {
    g.node_wt[i] = (i == src) ? 0 : INF;
  }
}

void sssp_init(SimpleCSRGraphUIAI g, unsigned int src, int numth) {
  std::vector<std::thread> ths;
  int work_per = ceil(g.num_nodes/(double)numth);
  for(int i = 0; i < numth; i++){
    int end = (i+1)*work_per > g.num_nodes ? g.num_nodes : (i+1)*work_per;
    ths.push_back(std::thread(&sssp_init_work, g, src, i*work_per, end));
  }

  for (auto& th : ths){
    th.join();
  }
  return;
}

void sssp_round_work(SimpleCSRGraphUIAI g, int start, int end, bool &changed) {

  for(unsigned int node = start; node < end; node++) {
    if(g.node_wt[node] == INF) continue;

    for(unsigned int e = g.row_start[node]; e < g.row_start[node + 1]; e++) {
      
      unsigned int dest = g.edge_dst[e];
      std::lock(node_lock[e], node_lock[dest]);
      std::lock_guard<std::mutex> dest_lg(node_lock[dest], std::adopt_lock);
      std::lock_guard<std::mutex> e_lg(node_lock[e], std::adopt_lock);
      
      int distance = g.node_wt[node] + g.edge_wt[e];

      int prev_distance = g.node_wt[dest];
      
      if(prev_distance > distance) {
        g.node_wt[dest] = distance;
        changed = true;
      }
    }
  }

}

bool sssp_round(SimpleCSRGraphUIAI g, int numth){
  bool changed = false;
  std::vector<std::thread> ths;
  int work_per = ceil(g.num_nodes/(double)numth);
  for(int i = 0; i < numth; i++){
    int end = (i+1)*work_per;
    ths.push_back(std::thread(&sssp_round_work, g, i*work_per, end>g.num_nodes ? g.num_nodes : end, std::ref(changed) ));
  }

  for (auto& th : ths){
    th.join();
  }
  return changed;
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
  if(argc != 3) {
    fprintf(stderr, "Usage: %s inputgraph outputfile\n", argv[0]);
    exit(1);
  }

  SimpleCSRGraphUIAI input;

  if(!input.load_file(argv[1])) {
    fprintf(stderr, "ERROR: failed to load graph\n");
    exit(1);
  } 

  printf("Loaded '%s', %u nodes, %u edges\n", argv[1], input.num_nodes, input.num_edges);

  ggc::Timer t("sssp");

  int src = 0, rounds = 0;
  int numth = 4;
  //node_lock = (std::mutex*)malloc(sizeof(std::mutex)*input.num_edges);
  node_lock = new std::mutex[input.num_edges];
  //std::lock(node_lock[1], node_lock[2]);

  t.start();
  sssp_init(input, src, numth);
  for(rounds = 0; rounds < input.num_nodes - 1; rounds++) {
    if(!sssp_round(input, numth)) {
      //no changes in graph, so exit early
      break;
    }
  }
  t.stop();

  printf("%d rounds\n", rounds); /* parallel versions may have a different number of rounds */
  printf("Total time: %u ms\n", t.duration_ms());

  write_output(input, argv[2]);

  printf("Wrote output '%s'\n", argv[2]);
  return 0;
}
