/* -*- mode: c++ -*- */

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <thread>
#include <atomic>
#include <vector>
#include <math.h>
#include "simplegraph.h"
#include "Timer.h"

const int INF = INT_MAX;
pthread_barrier_t barrier;
ggc::Timer **timers;

typedef SimpleCSRGraph<unsigned int, std::atomic<int>> SimpleCSRGraphUIAI;

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

void sssp_work(SimpleCSRGraphUIAI g, int id, int start, int end, bool &changed, int &rounds) {
  while(true){
    timers[id]->start();
    if(start==0) rounds++;
    for(unsigned int node = start; node < end; node++) {
      if(g.node_wt[node] == INF) continue;

      for(unsigned int e = g.row_start[node]; e < g.row_start[node + 1]; e++) {
        unsigned int dest = g.edge_dst[e];
        int distance = g.node_wt[node].load( std::memory_order_relaxed ) + g.edge_wt[e];

        int prev_distance = g.node_wt[dest].load( std::memory_order_relaxed );
        
        if(prev_distance > distance) {
            g.node_wt[dest].store(distance, std::memory_order_relaxed );
            changed = true;
        }
      }
    }
    timers[id]->stop();
    pthread_barrier_wait(&barrier);
    if(!changed) return;
    pthread_barrier_wait(&barrier);
    changed = false;
    pthread_barrier_wait(&barrier);
  }

}

void sssp_work_two(SimpleCSRGraphUIAI g, int start, int end, bool &changed1, bool &changed2, int &rounds) {
  bool sense = false;
  while(true){
    if(start==0) rounds++;
    for(unsigned int node = start; node < end; node++) {
      if(g.node_wt[node] == INF) continue;

      for(unsigned int e = g.row_start[node]; e < g.row_start[node + 1]; e++) {
        unsigned int dest = g.edge_dst[e];
        int distance = g.node_wt[node].load( std::memory_order_relaxed ) + g.edge_wt[e];

        int prev_distance = g.node_wt[dest].load( std::memory_order_relaxed );
        
        if(prev_distance > distance) {
            g.node_wt[dest].store(distance, std::memory_order_relaxed );
            sense? changed1 = true : changed2 = true;
        }
      }
    }
    pthread_barrier_wait(&barrier);
    if(sense ? !changed1 : !changed2) return;
    !sense ? changed1 = false : changed2 = false;
    pthread_barrier_wait(&barrier);
    sense = !sense;
    
  }

}

int sssp(SimpleCSRGraphUIAI g, int numth){
  bool changed = false;
  int rounds = 0;
  pthread_barrier_init(&barrier, NULL, numth);
  std::vector<std::thread> ths;
  int work_per = ceil(g.num_nodes/(double)numth);
  for(int i = 0; i < numth; i++){
    int end = (i+1)*work_per;
    ths.push_back(std::thread(&sssp_work, g, i, i*work_per, end>g.num_nodes ? g.num_nodes : end, std::ref(changed), std::ref(rounds) ));
  }

  for (auto& th : ths){
    th.join();
  }
  return rounds;
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
    fprintf(stderr, "Usage: %s inputgraph outputfile threadnum\n", argv[0]);
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
  int numth = atoi(argv[3]);
  timers = (ggc::Timer**)malloc(sizeof(ggc::Timer*)*numth);
  for(int i = 0; i<numth; i++){
    timers[i] = new ggc::Timer("a");
  }

  t.start();
  sssp_init(input, src, numth);
  rounds = sssp(input, numth);
  t.stop();

  printf("%d rounds\n", rounds); /* parallel versions may have a different number of rounds */
  printf("Total time: %u ms\n", t.duration_ms());
  printf("Thread time: \n");
  for(int i = 0; i<numth; i++){
    printf("[%d] -> %u ms\n", i, timers[i]->total_duration_ms());
  }

  write_output(input, argv[2]);

  printf("Wrote output '%s'\n", argv[2]);
  return 0;
}
