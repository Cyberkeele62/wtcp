#include "link.hh"
#include <sys/time.h>
#include<assert.h>
#include<sys/socket.h>
#include<iostream>
Link::Link(double rate,int fd)
 : pkt_queue(),
   pkt_queue_occupancy(0),
   byte_queue_occupancy(0),
   next_transmission(-1),
   last_token_update(Link::timestamp()),
   token_count(0),
   BUFFER_SIZE_BYTES(1000000000),
   BURST_SIZE(1600), /* 1 packet */
   link_socket(fd),
   total_bytes(0),
   begin_time(Link::timestamp()),
   last_stat_update(0),
   last_stat_bytes(0), 
   link_rate(rate) {

}

int Link::dequeue() {
  if(pkt_queue.empty()) return -1;  /* underflow */
  Payload p=pkt_queue.front();
  pkt_queue_occupancy--;
  byte_queue_occupancy=byte_queue_occupancy-p.size; 
  pkt_queue.pop();
  return 0; 
}

int Link::enqueue(Payload p) {
   if(byte_queue_occupancy+p.size>BUFFER_SIZE_BYTES) return -1; /* overflow */
   pkt_queue.push(p); 
   byte_queue_occupancy=byte_queue_occupancy+p.size; 
   pkt_queue_occupancy++;
   return 0; 
}

int Link::recv(uint8_t* ether_frame,uint16_t size) {
     Payload p(ether_frame,size); /* */ 
     if(link_rate<0) { 
       /* no need to traffic shape, send packet right away. That way tick will always find an empty queue */
       send_pkt(p); 
       return 0;
     } 
     return enqueue(p); 
}

inline void Link::print_stats(uint64_t ts_now){
  if(ts_now>last_stat_update+1e9)  {/* 1 second ago */
          std::cout<<"At time " <<ts_now<<" , queue is " <<byte_queue_occupancy<<" , "<<" @ "<<(float)((total_bytes-last_stat_bytes)*1e9)/(ts_now-last_stat_update)<<" bytes per sec "<<token_count<<" tokens \n";
          last_stat_update=ts_now;
          last_stat_bytes=total_bytes;
   }
}
void Link::tick() {

   uint64_t ts_now=Link::timestamp(); 
   print_stats(ts_now);
   /* compare against last_token_update */
   uint64_t elapsed = ts_now - last_token_update ;
   /* get new count */
   long double new_token_count=token_count+elapsed*link_rate*1.e-9; 
   update_token_count(ts_now,new_token_count);
   /* Can I send pkts right away ? */ 
   if(!pkt_queue.empty()) { 
     Payload head=pkt_queue.front();
     while(token_count>=head.size && head.size > 0) {
        send_pkt(head);
        dequeue();
        ts_now=Link::timestamp();  
        elapsed = ts_now - last_token_update ;
        new_token_count=token_count-head.size+elapsed*link_rate*1.e-9; 
        update_token_count(ts_now,new_token_count);
        if(pkt_queue.empty()) head.size=-1;
        else  head=pkt_queue.front();
     }
     /* if there are packets wait till tokens accumulate in the future */ 
     if(!pkt_queue.empty())  {
      uint32_t requiredTokens = head.size-token_count; 
      uint64_t wait_time_ns = (1.e9*requiredTokens) / link_rate ;  
      next_transmission=wait_time_ns+ts_now;
     }
   }
   else next_transmission = -1 ;
}

void Link::update_token_count(uint64_t current_ts,long double new_count) {
      /* maintain invariant */ 
      last_token_update=current_ts; 
      token_count=(new_count > BURST_SIZE) ? BURST_SIZE : new_count;
}

void Link::send_pkt(Payload p)  {
   int sent_bytes;
   if ((sent_bytes = send(link_socket,p.pkt_data,p.size,MSG_TRUNC))<0) {
               perror("send() on egress failed:");
               exit(EXIT_FAILURE);
   }
   total_bytes=total_bytes+sent_bytes;
}

int Link::wait_time_ns( void ) const
{
  return next_transmission - Link::timestamp();
}

uint64_t Link::timestamp( void )
{
  struct timespec ts;
  if ( clock_gettime( CLOCK_REALTIME, &ts ) < 0 ) {
    perror( "clock_gettime" );
    exit( 1 );
  }
  uint64_t ret = ts.tv_sec * 1000000000 + ts.tv_nsec;
  return ret;
}
