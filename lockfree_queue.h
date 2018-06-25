
#include <stdio.h>
#include <stdlib.h>
#include <atomic>

struct node;

struct alignas(2*sizeof(void*)) ptr{
  node* p;
  int c;
  ptr();
  ptr(node* p, int c);
  bool operator==(const ptr& rhs);
};

struct node{
  std::atomic<int> val;
  std::atomic<ptr> next;
  node(int val, ptr next);
  node(int val);
};

node::node(int val, ptr next):
val(val),
next(next)
{}

node::node(int val):
val(val),
next(ptr())
{}

ptr::ptr():
p(NULL),
c(0)
{}

ptr::ptr(node* p, int c):
p(p),
c(c)
{}

bool ptr::operator==(const ptr& rhs){
  return p == rhs.p && c == rhs.c;
}

class MSQueue{

  private:
  std::atomic<ptr> head;
  std::atomic<ptr> tail;

  public:
  MSQueue():
  head(ptr()),
  tail(ptr())
  {
    node* n = new node(-1);
    head.store(ptr(n, 0));
    tail.store(ptr(n, 0));
  }


  bool push(int v){
    node* w = new node(v);
    std::atomic_thread_fence(std::memory_order_release);
    ptr t, n;
    while(true){
      t = tail.load();
      n = t.p->next.load();
      if (t == tail.load()){
        if(n.p == NULL){
          if(t.p->next.compare_exchange_weak(n, ptr(w, n.c+1))) break;
        }else{
          tail.compare_exchange_weak(t, ptr(n.p, t.c+1));
        }
      }

    }
    tail.compare_exchange_weak(t, ptr(w, t.c+1));
    return true;
  }

  bool pop(int& v){
    ptr h, t, n;
    while(true){
      h = head.load();
      t = tail.load();
      n = h.p->next.load();
      if(h == head.load()){
        if(h.p == t.p){
          if(n.p == NULL) return false;
          tail.compare_exchange_weak(t, ptr(n.p, t.c+1));
        }else{
          v = n.p->val.load();
          if(head.compare_exchange_weak(h, ptr(n.p, h.c+1))) break;
        }
      }
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    return true;
  }
};
