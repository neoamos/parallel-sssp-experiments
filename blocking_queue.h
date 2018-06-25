#include <pthread.h>
#include <queue>

class BlockingQueue{
  private:
  pthread_mutex_t lock;
  pthread_cond_t cond;
  std::queue<int> queue;

  public:
  BlockingQueue(){
    pthread_mutex_init(&lock, NULL);
  }
  bool pop(int& item){
    pthread_mutex_lock(&lock);
    while(queue.empty()){
      pthread_cond_wait(&cond, &lock);
    }
    item = queue.front();
    queue.pop();
    pthread_mutex_unlock(&lock);
    return true;
  }

  bool push(const int& item){
    pthread_mutex_lock(&lock);
    queue.push(item);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
    return true;
  }

  bool empty(){
    bool ret = false;
    pthread_mutex_lock(&lock);
    ret = queue.empty();
    pthread_mutex_unlock(&lock);
    return ret;
  }


};
